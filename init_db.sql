-- PostgreSQL Database Initialization Script
-- Database: water_leakage_db

-- Drop tables if they exist (for clean initialization)
DROP TABLE IF EXISTS leakage_events CASCADE;
DROP TABLE IF EXISTS sensor_readings CASCADE;
DROP TABLE IF EXISTS devices CASCADE;
DROP TABLE IF EXISTS system_logs CASCADE;

-- 1. Devices Table
CREATE TABLE devices (
    id VARCHAR(50) PRIMARY KEY,
    device_name VARCHAR(100) NOT NULL,
    device_status VARCHAR(20) DEFAULT 'offline',
    ip_address VARCHAR(45),
    wifi_ssid VARCHAR(50),
    wifi_rssi INT,
    last_seen TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

-- 2. Sensor Readings Table
CREATE TABLE sensor_readings (
    id SERIAL PRIMARY KEY,
    device_id VARCHAR(50) NOT NULL,
    sensor_name VARCHAR(50) NOT NULL DEFAULT 'Water Surface Sensor',
    sensor_value INT NOT NULL CHECK (sensor_value IN (0, 1)),
    water_status VARCHAR(10) NOT NULL CHECK (water_status IN ('WET', 'DRY')),
    wifi_rssi INT,
    is_simulated BOOLEAN DEFAULT FALSE,
    recorded_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT fk_readings_device FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE
);

-- Indexes for fast queries
CREATE INDEX idx_readings_recorded_at ON sensor_readings (recorded_at DESC);
CREATE INDEX idx_readings_device_id ON sensor_readings (device_id);
CREATE INDEX idx_readings_water_status ON sensor_readings (water_status);

-- 3. Leakage Events Table
CREATE TABLE leakage_events (
    id SERIAL PRIMARY KEY,
    device_id VARCHAR(50) NOT NULL,
    event_type VARCHAR(50) NOT NULL DEFAULT 'WATER_LEAKAGE',
    description TEXT,
    detected_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    resolved_at TIMESTAMP WITH TIME ZONE,
    duration_seconds INT,
    status VARCHAR(20) DEFAULT 'ACTIVE' CHECK (status IN ('ACTIVE', 'RESOLVED')),
    CONSTRAINT fk_events_device FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE
);

CREATE INDEX idx_events_detected_at ON leakage_events (detected_at DESC);
CREATE INDEX idx_events_status ON leakage_events (status);

-- 4. System Activity Logs Table
CREATE TABLE system_logs (
    id SERIAL PRIMARY KEY,
    log_level VARCHAR(20) DEFAULT 'INFO',
    event_category VARCHAR(50) NOT NULL,
    message TEXT NOT NULL,
    device_id VARCHAR(50),
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_logs_created_at ON system_logs (created_at DESC);

-- Seed initial device record for ESP32
INSERT INTO devices (id, device_name, device_status, last_seen, created_at)
VALUES ('ESP32_LEAK_01', 'ESP32 Water Leakage Monitor #1', 'offline', CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)
ON CONFLICT (id) DO NOTHING;

-- Seed initial system log
INSERT INTO system_logs (log_level, event_category, message, device_id)
VALUES ('INFO', 'SYSTEM_STARTUP', 'Smart Water Leakage Database initialized successfully.', 'ESP32_LEAK_01');

