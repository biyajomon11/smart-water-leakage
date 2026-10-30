from datetime import datetime, timezone
import logging
from flask_sqlalchemy import SQLAlchemy
from sqlalchemy import exc

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

db = SQLAlchemy()

class Device(db.Model):
    __tablename__ = 'devices'
    
    id = db.Column(db.String(50), primary_key=True)
    device_name = db.Column(db.String(100), nullable=False, default='ESP32 Water Leakage Monitor')
    device_status = db.Column(db.String(20), default='offline') # 'online' or 'offline'
    ip_address = db.Column(db.String(45), nullable=True)
    wifi_ssid = db.Column(db.String(50), nullable=True)
    wifi_rssi = db.Column(db.Integer, nullable=True)
    last_seen = db.Column(db.DateTime(timezone=True), default=lambda: datetime.now(timezone.utc))
    created_at = db.Column(db.DateTime(timezone=True), default=lambda: datetime.now(timezone.utc))

    readings = db.relationship('SensorReading', backref='device', lazy='dynamic', cascade='all, delete-orphan')
    events = db.relationship('LeakageEvent', backref='device', lazy='dynamic', cascade='all, delete-orphan')

    def to_dict(self, offline_threshold_seconds=15):
        now = datetime.now(timezone.utc)
        is_online = False
        seconds_since_last_seen = None
        
        if self.last_seen:
            last_seen_aware = self.last_seen if self.last_seen.tzinfo else self.last_seen.replace(tzinfo=timezone.utc)
            delta = (now - last_seen_aware).total_seconds()
            seconds_since_last_seen = round(delta, 1)
            is_online = delta <= offline_threshold_seconds

        return {
            'id': self.id,
            'device_name': self.device_name,
            'device_status': 'online' if is_online else 'offline',
            'is_online': is_online,
            'ip_address': self.ip_address,
            'wifi_ssid': self.wifi_ssid,
            'wifi_rssi': self.wifi_rssi,
            'last_seen': self.last_seen.isoformat() if self.last_seen else None,
            'seconds_since_last_seen': seconds_since_last_seen,
            'created_at': self.created_at.isoformat() if self.created_at else None
        }


class SensorReading(db.Model):
    __tablename__ = 'sensor_readings'
    
    id = db.Column(db.Integer, primary_key=True, autoincrement=True)
    device_id = db.Column(db.String(50), db.ForeignKey('devices.id', ondelete='CASCADE'), nullable=False)
    sensor_name = db.Column(db.String(50), nullable=False, default='Water Surface Sensor')
    sensor_value = db.Column(db.Integer, nullable=False) # 0 or 1
    water_status = db.Column(db.String(10), nullable=False) # 'WET' or 'DRY'
    wifi_rssi = db.Column(db.Integer, nullable=True)
    is_simulated = db.Column(db.Boolean, default=False)
    recorded_at = db.Column(db.DateTime(timezone=True), default=lambda: datetime.now(timezone.utc), index=True)

    def to_dict(self):
        return {
            'id': self.id,
            'device_id': self.device_id,
            'sensor_name': self.sensor_name,
            'sensor_value': self.sensor_value,
            'water_status': self.water_status,
            'wifi_rssi': self.wifi_rssi,
            'is_simulated': self.is_simulated,
            'recorded_at': self.recorded_at.isoformat() if self.recorded_at else None
        }


class LeakageEvent(db.Model):
    __tablename__ = 'leakage_events'
    
    id = db.Column(db.Integer, primary_key=True, autoincrement=True)
    device_id = db.Column(db.String(50), db.ForeignKey('devices.id', ondelete='CASCADE'), nullable=False)
    event_type = db.Column(db.String(50), nullable=False, default='WATER_LEAKAGE')
    description = db.Column(db.Text, nullable=True)
    detected_at = db.Column(db.DateTime(timezone=True), default=lambda: datetime.now(timezone.utc), index=True)
    resolved_at = db.Column(db.DateTime(timezone=True), nullable=True)
    duration_seconds = db.Column(db.Integer, nullable=True)
    status = db.Column(db.String(20), default='ACTIVE', index=True) # 'ACTIVE' or 'RESOLVED'

    def to_dict(self):
        duration = self.duration_seconds
        if not duration and self.status == 'ACTIVE' and self.detected_at:
            detected_aware = self.detected_at if self.detected_at.tzinfo else self.detected_at.replace(tzinfo=timezone.utc)
            duration = int((datetime.now(timezone.utc) - detected_aware).total_seconds())

        return {
            'id': self.id,
            'device_id': self.device_id,
            'event_type': self.event_type,
            'description': self.description,
            'detected_at': self.detected_at.isoformat() if self.detected_at else None,
            'resolved_at': self.resolved_at.isoformat() if self.resolved_at else None,
            'duration_seconds': duration,
            'status': self.status
        }


class SystemLog(db.Model):
    __tablename__ = 'system_logs'
    
    id = db.Column(db.Integer, primary_key=True, autoincrement=True)
    log_level = db.Column(db.String(20), default='INFO') # 'INFO', 'WARNING', 'ALERT', 'ERROR'
    event_category = db.Column(db.String(50), nullable=False) # 'LEAKAGE', 'CONNECTION', 'SYSTEM', 'RESTORE'
    message = db.Column(db.Text, nullable=False)
    device_id = db.Column(db.String(50), nullable=True)
    created_at = db.Column(db.DateTime(timezone=True), default=lambda: datetime.now(timezone.utc), index=True)

    def to_dict(self):
        return {
            'id': self.id,
            'log_level': self.log_level,
            'event_category': self.event_category,
            'message': self.message,
            'device_id': self.device_id,
            'created_at': self.created_at.isoformat() if self.created_at else None
        }


def log_system_event(category, message, log_level='INFO', device_id=None):
    """Helper to log system activity with automatic database persistence"""
    try:
        log_entry = SystemLog(
            log_level=log_level,
            event_category=category,
            message=message,
            device_id=device_id,
            created_at=datetime.now(timezone.utc)
        )
        db.session.add(log_entry)
        db.session.commit()
    except Exception as e:
        logger.error(f"Failed to record system log: {e}")
        db.session.rollback()


def init_database(app):
    """
    Initializes database with smart fallback:
    Attempts PostgreSQL first; if unavailable, falls back to SQLite for local development ease.
    """
    try:
        db.init_app(app)
        with app.app_context():
            db.create_all()
            
            # Ensure default ESP8266 and ESP32 devices exist
            for dev_id, dev_name in [
                ('ESP8266_LEAK_01', 'ESP8266 NodeMCU Water Leakage Monitor'),
                ('ESP32_LEAK_01', 'ESP32 Water Leakage Monitor')
            ]:
                existing = Device.query.filter_by(id=dev_id).first()
                if not existing:
                    db.session.add(Device(
                        id=dev_id,
                        device_name=dev_name,
                        device_status='offline'
                    ))
            db.session.commit()
                
            log_system_event(
                category='SYSTEM',
                message='Smart Water Leakage Database connected & initialized for ESP8266/ESP32.',
                log_level='INFO',
                device_id='ESP8266_LEAK_01'
            )
            logger.info("Database initialized successfully.")
    except Exception as err:
        logger.warning(f"Could not connect using primary URI ({app.config['SQLALCHEMY_DATABASE_URI']}): {err}")
        logger.warning("Attempting automatic fallback to SQLite for local development...")
        
        app.config['SQLALCHEMY_DATABASE_URI'] = 'sqlite:///water_leakage.db'
        db.init_app(app)
        with app.app_context():
            db.create_all()
            for dev_id, dev_name in [
                ('ESP8266_LEAK_01', 'ESP8266 NodeMCU Water Leakage Monitor'),
                ('ESP32_LEAK_01', 'ESP32 Water Leakage Monitor')
            ]:
                existing = Device.query.filter_by(id=dev_id).first()
                if not existing:
                    db.session.add(Device(
                        id=dev_id,
                        device_name=dev_name,
                        device_status='offline'
                    ))
            db.session.commit()
            log_system_event(
                category='SYSTEM',
                message='Running with SQLite local database engine.',
                log_level='INFO',
                device_id='ESP8266_LEAK_01'
            )
            logger.info("Fallback to SQLite successful. Database ready.")

