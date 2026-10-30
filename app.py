import csv
from datetime import datetime, timedelta, timezone
import io
import os
from flask import Flask, jsonify, render_template, request, Response
from flask_cors import CORS
from config import Config
from database import (
    db, init_database, Device, SensorReading, LeakageEvent, SystemLog, log_system_event
)

app = Flask(__name__)
app.config.from_object(Config)
CORS(app)

# Initialize database
init_database(app)


# -------------------------------------------------------------------------
# Helper Functions
# -------------------------------------------------------------------------
def resolve_target_device_id(requested_id=None):
    """
    Intelligently resolves device ID:
    If a specific ID is asked for, use it.
    Otherwise, automatically pick the most recently active hardware (ESP8266 or ESP32).
    """
    if requested_id and requested_id.strip():
        return requested_id.strip()
    latest_reading = SensorReading.query.order_by(SensorReading.recorded_at.desc()).first()
    if latest_reading:
        return latest_reading.device_id
    latest_device = Device.query.order_by(Device.last_seen.desc()).first()
    if latest_device:
        return latest_device.id
    return 'ESP8266_LEAK_01'

def get_or_create_device(device_id, device_name=None):
    device = Device.query.filter_by(id=device_id).first()
    if not device:
        dev_label = "ESP8266 NodeMCU" if "8266" in device_id else "ESP32"
        device = Device(
            id=device_id,
            device_name=device_name or f"{dev_label} Water Leakage Monitor ({device_id})",
            device_status='offline'
        )
        db.session.add(device)
        db.session.commit()
    return device


# -------------------------------------------------------------------------
# Web Page Routes (Navigation Sidebar)
# -------------------------------------------------------------------------
@app.route('/')
def index():
    return render_template('index.html', active_page='dashboard')

@app.route('/live')
def live_monitoring():
    return render_template('live.html', active_page='live')

@app.route('/history')
def sensor_history():
    return render_template('history.html', active_page='history')

@app.route('/alerts')
def leakage_alerts():
    return render_template('alerts.html', active_page='alerts')

@app.route('/analytics')
def analytics():
    return render_template('analytics.html', active_page='analytics')

@app.route('/device')
def device_info():
    return render_template('device.html', active_page='device')

@app.route('/settings')
def settings():
    return render_template('settings.html', active_page='settings')

@app.route('/about')
def about():
    return render_template('about.html', active_page='about')


# ------------------------------ -------------------------------------------
# REST API Endpoints
# -------------------------------------------------------------------------

@app.route('/api/sensor-data', methods=['POST'])
def receive_sensor_data():
    """
    Primary ingestion endpoint for ESP32 & simulated inputs.
    Receives JSON payload, updates device status, processes wet/dry events,
    and logs changes to the database.
    """
    data = request.get_json(silent=True)
    if not data:
        return jsonify({'status': 'error', 'message': 'Invalid or empty JSON body'}), 400

    device_id = data.get('device_id', 'ESP8266_LEAK_01').strip()
    sensor_name = data.get('sensor_name', 'Water Surface Sensor').strip()
    
    # Value parsing (0 or 1)
    raw_val = data.get('sensor_value')
    if raw_val is None:
        return jsonify({'status': 'error', 'message': 'Missing sensor_value'}), 400
    try:
        sensor_value = int(raw_val)
        if sensor_value not in (0, 1):
            raise ValueError
    except (ValueError, TypeError):
        return jsonify({'status': 'error', 'message': 'sensor_value must be 0 or 1'}), 400

    water_status = data.get('water_status', '').upper().strip()
    if water_status not in ('WET', 'DRY'):
        water_status = 'WET' if sensor_value == 1 else 'DRY'

    wifi_rssi = data.get('wifi_rssi')
    ip_address = data.get('ip_address', request.remote_addr)
    wifi_ssid = data.get('wifi_ssid')
    is_simulated = bool(data.get('is_simulated', False))
    now = datetime.now(timezone.utc)

    # 1. Device lookup & status tracking
    device = get_or_create_device(device_id)
    prev_status = device.device_status
    device.last_seen = now
    device.device_status = 'online'
    if ip_address:
        device.ip_address = ip_address
    if wifi_ssid:
        device.wifi_ssid = wifi_ssid
    if wifi_rssi is not None:
        try:
            device.wifi_rssi = int(wifi_rssi)
        except (ValueError, TypeError):
            pass

    # Log device reconnection if previously marked offline
    if prev_status == 'offline':
        sim_tag = "[SIMULATED] " if is_simulated else ""
        log_system_event(
            category='CONNECTION',
            message=f"{sim_tag}Device {device_id} connected and established heartbeat.",
            log_level='INFO',
            device_id=device_id
        )

    # 2. Store Sensor Reading
    reading = SensorReading(
        device_id=device_id,
        sensor_name=sensor_name,
        sensor_value=sensor_value,
        water_status=water_status,
        wifi_rssi=device.wifi_rssi,
        is_simulated=is_simulated,
        recorded_at=now
    )
    db.session.add(reading)

    # 3. Handle Active/Resolved Leakage Events
    active_event = LeakageEvent.query.filter_by(device_id=device_id, status='ACTIVE').first()

    if water_status == 'WET':
        if not active_event:
            # New leakage event detected!
            new_event = LeakageEvent(
                device_id=device_id,
                event_type='WATER_LEAKAGE',
                description=f"Water leakage detected on {sensor_name} (Pin GPIO 34). Buzzer activated and Red LED illuminated.",
                detected_at=now,
                status='ACTIVE'
            )
            db.session.add(new_event)
            log_system_event(
                category='LEAKAGE',
                message=f"CRITICAL: Water leakage detected on {device_id}! Buzzer activated.",
                log_level='ALERT',
                device_id=device_id
            )
    else: # water_status == 'DRY'
        if active_event:
            # Leakage has been resolved / dried out
            detected_at = active_event.detected_at
            if detected_at:
                det_aware = detected_at if detected_at.tzinfo else detected_at.replace(tzinfo=timezone.utc)
                duration = int((now - det_aware).total_seconds())
            else:
                duration = 0
            
            active_event.status = 'RESOLVED'
            active_event.resolved_at = now
            active_event.duration_seconds = max(duration, 1)
            
            log_system_event(
                category='RESTORE',
                message=f"Sensor returned to dry condition. Leakage incident resolved after {duration}s. Green LED restored.",
                log_level='INFO',
                device_id=device_id
            )

    try:
        db.session.commit()
    except Exception as e:
        db.session.rollback()
        return jsonify({'status': 'error', 'message': f'Database error: {str(e)}'}), 500

    return jsonify({
        'status': 'success',
        'message': 'Sensor reading recorded successfully',
        'data': {
            'reading_id': reading.id,
            'device_id': device_id,
            'water_status': water_status,
            'sensor_value': sensor_value,
            'alert_active': (water_status == 'WET'),
            'timestamp': now.isoformat()
        }
    }), 201


@app.route('/api/latest-reading', methods=['GET'])
def get_latest_reading():
    """Returns the most recent sensor reading, device status, and any active leakage event."""
    device_id = resolve_target_device_id(request.args.get('device_id'))
    device = get_or_create_device(device_id)
    
    threshold = app.config['DEVICE_OFFLINE_THRESHOLD_SECONDS']
    device_dict = device.to_dict(threshold) if device else None

    latest_reading = (
        SensorReading.query.filter_by(device_id=device_id)
        .order_by(SensorReading.recorded_at.desc())
        .first()
    )
    
    active_event = (
        LeakageEvent.query.filter_by(device_id=device_id, status='ACTIVE')
        .order_by(LeakageEvent.detected_at.desc())
        .first()
    )

    return jsonify({
        'status': 'success',
        'device': device_dict,
        'reading': latest_reading.to_dict() if latest_reading else None,
        'active_event': active_event.to_dict() if active_event else None,
        'server_time': datetime.now(timezone.utc).isoformat()
    })


@app.route('/api/readings', methods=['GET'])
def get_readings():
    """
    Returns historical sensor readings with filtering (time range, wet/dry status, search)
    and pagination.
    """
    device_id = resolve_target_device_id(request.args.get('device_id'))
    time_range = request.args.get('range', '24h') # '1h', '24h', '7d', 'all'
    status_filter = request.args.get('status', 'all').upper() # 'all', 'WET', 'DRY'
    limit = min(int(request.args.get('limit', 50)), 500)
    page = max(int(request.args.get('page', 1)), 1)
    offset = (page - 1) * limit

    query = SensorReading.query.filter_by(device_id=device_id)

    # Time range filtering
    now = datetime.now(timezone.utc)
    if time_range == '1h':
        query = query.filter(SensorReading.recorded_at >= now - timedelta(hours=1))
    elif time_range == '24h':
        query = query.filter(SensorReading.recorded_at >= now - timedelta(hours=24))
    elif time_range == '7d':
        query = query.filter(SensorReading.recorded_at >= now - timedelta(days=7))

    # Status filter
    if status_filter in ('WET', 'DRY'):
        query = query.filter(SensorReading.water_status == status_filter)

    total_count = query.count()
    readings = query.order_by(SensorReading.recorded_at.desc()).offset(offset).limit(limit).all()

    return jsonify({
        'status': 'success',
        'total': total_count,
        'page': page,
        'limit': limit,
        'pages': (total_count + limit - 1) // limit if limit > 0 else 1,
        'readings': [r.to_dict() for r in readings]
    })


@app.route('/api/statistics', methods=['GET'])
def get_statistics():
    """Returns aggregated monitoring statistics."""
    device_id = resolve_target_device_id(request.args.get('device_id'))
    device = get_or_create_device(device_id)
    threshold = app.config['DEVICE_OFFLINE_THRESHOLD_SECONDS']
    dev_dict = device.to_dict(threshold) if device else {}

    total_readings = SensorReading.query.filter_by(device_id=device_id).count()
    wet_readings = SensorReading.query.filter_by(device_id=device_id, water_status='WET').count()
    dry_readings = SensorReading.query.filter_by(device_id=device_id, water_status='DRY').count()

    total_events = LeakageEvent.query.filter_by(device_id=device_id).count()
    active_events = LeakageEvent.query.filter_by(device_id=device_id, status='ACTIVE').count()
    resolved_events = LeakageEvent.query.filter_by(device_id=device_id, status='RESOLVED').count()

    # Average duration of resolved events
    resolved = LeakageEvent.query.filter_by(device_id=device_id, status='RESOLVED').all()
    durations = [e.duration_seconds for e in resolved if e.duration_seconds is not None]
    avg_duration = round(sum(durations) / len(durations), 1) if durations else 0

    # Last leakage detected timestamp
    last_event = LeakageEvent.query.filter_by(device_id=device_id).order_by(LeakageEvent.detected_at.desc()).first()

    return jsonify({
        'status': 'success',
        'device': dev_dict,
        'statistics': {
            'total_readings': total_readings,
            'wet_readings': wet_readings,
            'dry_readings': dry_readings,
            'total_leakage_events': total_events,
            'active_leakage_events': active_events,
            'resolved_leakage_events': resolved_events,
            'average_resolution_seconds': avg_duration,
            'last_leakage_at': last_event.detected_at.isoformat() if last_event else None
        }
    })


@app.route('/api/device-status', methods=['GET'])
def get_device_status():
    """Returns detailed connectivity & telemetry about the ESP8266/ESP32 node."""
    device_id = resolve_target_device_id(request.args.get('device_id'))
    device = get_or_create_device(device_id)
    threshold = app.config['DEVICE_OFFLINE_THRESHOLD_SECONDS']
    info = device.to_dict(threshold)

    # Compute Wi-Fi Signal Percentage from RSSI
    # Typically -100 dBm (0%) to -50 dBm (100%)
    rssi = info.get('wifi_rssi')
    signal_pct = 0
    signal_quality = 'Unknown'
    if rssi is not None:
        if rssi <= -100:
            signal_pct = 0
            signal_quality = 'Very Poor'
        elif rssi >= -50:
            signal_pct = 100
            signal_quality = 'Excellent'
        else:
            signal_pct = 2 * (rssi + 100)
            if rssi >= -65:
                signal_quality = 'Good'
            elif rssi >= -75:
                signal_quality = 'Fair'
            else:
                signal_quality = 'Poor'

    info['wifi_signal_pct'] = signal_pct
    info['wifi_signal_quality'] = signal_quality
    info['offline_threshold_seconds'] = threshold

    return jsonify({
        'status': 'success',
        'device': info
    })


@app.route('/api/alerts', methods=['GET'])
def get_alerts():
    """Returns leakage alerts and incidents history."""
    device_id = resolve_target_device_id(request.args.get('device_id'))
    status_filter = request.args.get('status', 'all').upper()
    limit = min(int(request.args.get('limit', 100)), 500)

    query = LeakageEvent.query.filter_by(device_id=device_id)
    if status_filter in ('ACTIVE', 'RESOLVED'):
        query = query.filter_by(status=status_filter)

    events = query.order_by(LeakageEvent.detected_at.desc()).limit(limit).all()

    return jsonify({
        'status': 'success',
        'count': len(events),
        'alerts': [e.to_dict() for e in events]
    })


@app.route('/api/logs', methods=['GET'])
def get_logs():
    """Returns recent system logs for activity tracking."""
    limit = min(int(request.args.get('limit', 50)), 200)
    device_id = request.args.get('device_id')
    query = SystemLog.query
    if device_id:
        query = query.filter_by(device_id=device_id)

    logs = query.order_by(SystemLog.created_at.desc()).limit(limit).all()
    return jsonify({
        'status': 'success',
        'logs': [l.to_dict() for l in logs]
    })


@app.route('/api/simulate', methods=['POST'])
def simulate_sensor():
    """
    Simulation mode endpoint:
    Allows testing wet/dry transitions directly from the dashboard or testing tools
    when the physical ESP8266 is offline or in lab testing.
    Clearly marks records with `is_simulated = True`.
    """
    data = request.get_json(silent=True) or {}
    state = data.get('state', 'WET').upper()
    if state not in ('WET', 'DRY'):
        state = 'WET'
    sensor_val = 1 if state == 'WET' else 0
    device_id = data.get('device_id') or resolve_target_device_id()

    sim_payload = {
        'device_id': device_id,
        'sensor_name': 'Water Surface Sensor (NodeMCU D5)',
        'sensor_value': sensor_val,
        'water_status': state,
        'wifi_rssi': -58,
        'ip_address': '127.0.0.1',
        'wifi_ssid': 'Lab-Simulation-WLAN',
        'is_simulated': True
    }
    
    with app.test_request_context('/api/sensor-data', method='POST', json=sim_payload):
        return receive_sensor_data()


@app.route('/api/export-csv', methods=['GET'])
def export_csv():
    """Generates and downloads CSV reports for sensor readings or leakage events."""
    export_type = request.args.get('type', 'readings').lower()
    device_id = resolve_target_device_id(request.args.get('device_id'))

    output = io.StringIO()
    writer = csv.writer(output)

    if export_type == 'events':
        events = LeakageEvent.query.filter_by(device_id=device_id).order_by(LeakageEvent.detected_at.desc()).all()
        writer.writerow(['Event ID', 'Device ID', 'Event Type', 'Status', 'Detected At (UTC)', 'Resolved At (UTC)', 'Duration (seconds)', 'Description'])
        for e in events:
            writer.writerow([
                e.id,
                e.device_id,
                e.event_type,
                e.status,
                e.detected_at.isoformat() if e.detected_at else '',
                e.resolved_at.isoformat() if e.resolved_at else '',
                e.duration_seconds or '',
                e.description or ''
            ])
        filename = f"leakage_events_{datetime.now(timezone.utc).strftime('%Y%m%d_%H%M%S')}.csv"
    else:
        # Default: readings
        readings = SensorReading.query.filter_by(device_id=device_id).order_by(SensorReading.recorded_at.desc()).limit(2000).all()
        writer.writerow(['Reading ID', 'Device ID', 'Sensor Name', 'Sensor Value', 'Water Status', 'Wi-Fi RSSI (dBm)', 'Simulated', 'Recorded At (UTC)'])
        for r in readings:
            writer.writerow([
                r.id,
                r.device_id,
                r.sensor_name,
                r.sensor_value,
                r.water_status,
                r.wifi_rssi if r.wifi_rssi is not None else '',
                'Yes' if r.is_simulated else 'No',
                r.recorded_at.isoformat() if r.recorded_at else ''
            ])
        filename = f"sensor_readings_{datetime.now(timezone.utc).strftime('%Y%m%d_%H%M%S')}.csv"

    output.seek(0)
    return Response(
        output.getvalue(),
        mimetype='text/csv',
        headers={'Content-Disposition': f'attachment; filename={filename}'}
    )


if __name__ == '__main__':
    port = int(os.environ.get('PORT', 5000))
    host = os.environ.get('HOST', '0.0.0.0')
    print(f"===========================================================")
    print(f" Smart Water Leakage Detection System Backend Starting ")
    print(f" URL: http://localhost:{port} (Accessible on LAN at 0.0.0.0)")
    print(f"===========================================================")
    app.run(host=host, port=port, debug=True)

