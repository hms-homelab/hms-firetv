-- ==============================================================================
-- HMS FireTV Database Schema
-- ==============================================================================
--
-- PostgreSQL schema for Fire TV device management with REST API support
--

-- ==============================================================================
-- 1. Devices Table
-- ==============================================================================
CREATE TABLE IF NOT EXISTS fire_tv_devices (
    id SERIAL PRIMARY KEY,
    device_id VARCHAR(100) UNIQUE NOT NULL,
    name VARCHAR(200) NOT NULL,
    ip_address VARCHAR(50) NOT NULL,
    api_key VARCHAR(100) NOT NULL DEFAULT '0987654321',
    client_token VARCHAR(500),
    pin_code VARCHAR(10),
    pin_expires_at TIMESTAMP,
    status VARCHAR(20) DEFAULT 'offline',
    adb_enabled BOOLEAN DEFAULT false,
    last_seen_at TIMESTAMP,
    created_at TIMESTAMP DEFAULT NOW(),
    updated_at TIMESTAMP DEFAULT NOW()
);

-- Indexes for devices table
CREATE INDEX IF NOT EXISTS idx_fire_tv_devices_device_id ON fire_tv_devices(device_id);
CREATE INDEX IF NOT EXISTS idx_fire_tv_devices_ip_address ON fire_tv_devices(ip_address);
CREATE INDEX IF NOT EXISTS idx_fire_tv_devices_status ON fire_tv_devices(status);

COMMENT ON TABLE fire_tv_devices IS 'Fire TV devices registered in the system';
COMMENT ON COLUMN fire_tv_devices.device_id IS 'Unique identifier for the device (e.g., livingroom_firetv)';
COMMENT ON COLUMN fire_tv_devices.name IS 'Human-readable device name';
COMMENT ON COLUMN fire_tv_devices.ip_address IS 'Local network IP address';
COMMENT ON COLUMN fire_tv_devices.api_key IS 'Lightning API key (default: 0987654321)';
COMMENT ON COLUMN fire_tv_devices.client_token IS 'Pairing token from successful authentication';
COMMENT ON COLUMN fire_tv_devices.pin_code IS 'Temporary PIN for pairing process';
COMMENT ON COLUMN fire_tv_devices.pin_expires_at IS 'Expiration time for PIN';
COMMENT ON COLUMN fire_tv_devices.status IS 'Device status: online, offline, pairing';

-- ==============================================================================
-- 2. Device Apps Table
-- ==============================================================================
CREATE TABLE IF NOT EXISTS device_apps (
    id SERIAL PRIMARY KEY,
    device_id VARCHAR(100) NOT NULL REFERENCES fire_tv_devices(device_id) ON DELETE CASCADE,
    package_name VARCHAR(200) NOT NULL,
    app_name VARCHAR(200) NOT NULL,
    icon_url VARCHAR(500),
    is_favorite BOOLEAN DEFAULT false,
    sort_order INT DEFAULT 0,
    created_at TIMESTAMP DEFAULT NOW(),
    updated_at TIMESTAMP DEFAULT NOW(),
    UNIQUE(device_id, package_name)
);

-- Indexes for device_apps table
CREATE INDEX IF NOT EXISTS idx_device_apps_device_id ON device_apps(device_id);
CREATE INDEX IF NOT EXISTS idx_device_apps_package_name ON device_apps(package_name);
CREATE INDEX IF NOT EXISTS idx_device_apps_favorite ON device_apps(is_favorite) WHERE is_favorite = true;

COMMENT ON TABLE device_apps IS 'Apps installed on each Fire TV device';
COMMENT ON COLUMN device_apps.package_name IS 'Android package name (e.g., com.netflix.ninja)';
COMMENT ON COLUMN device_apps.app_name IS 'Human-readable app name';
COMMENT ON COLUMN device_apps.is_favorite IS 'User-marked favorite for quick access';
COMMENT ON COLUMN device_apps.sort_order IS 'Custom sort order for UI';

-- ==============================================================================
-- 3. Command History Table
-- ==============================================================================
CREATE TABLE IF NOT EXISTS command_history (
    id SERIAL PRIMARY KEY,
    device_id VARCHAR(100) NOT NULL REFERENCES fire_tv_devices(device_id) ON DELETE CASCADE,
    command_type VARCHAR(50) NOT NULL,
    command_data JSONB,
    success BOOLEAN NOT NULL,
    response_time_ms INT,
    error_message TEXT,
    created_at TIMESTAMP DEFAULT NOW()
);

-- Indexes for command_history table
CREATE INDEX IF NOT EXISTS idx_command_history_device_id ON command_history(device_id);
CREATE INDEX IF NOT EXISTS idx_command_history_created_at ON command_history(created_at DESC);
CREATE INDEX IF NOT EXISTS idx_command_history_success ON command_history(success);
CREATE INDEX IF NOT EXISTS idx_command_history_command_type ON command_history(command_type);

COMMENT ON TABLE command_history IS 'Audit log of all commands sent to devices';
COMMENT ON COLUMN command_history.command_type IS 'Type: navigation, media, volume, app, pairing';
COMMENT ON COLUMN command_history.command_data IS 'JSON payload of the command';
COMMENT ON COLUMN command_history.response_time_ms IS 'Command execution time in milliseconds';

-- ==============================================================================
-- 4. Popular Apps Pre-populated Data
-- ==============================================================================
-- Popular streaming apps that can be pre-loaded for new devices
CREATE TABLE IF NOT EXISTS popular_apps (
    id SERIAL PRIMARY KEY,
    package_name VARCHAR(200) UNIQUE NOT NULL,
    app_name VARCHAR(200) NOT NULL,
    icon_url VARCHAR(500),
    category VARCHAR(50),
    created_at TIMESTAMP DEFAULT NOW()
);

COMMENT ON TABLE popular_apps IS 'Catalog of popular Fire TV apps for quick setup';

-- Insert popular apps
INSERT INTO popular_apps (package_name, app_name, category) VALUES
    ('com.netflix.ninja', 'Netflix', 'streaming'),
    ('com.amazon.avod.thirdpartyclient', 'Prime Video', 'streaming'),
    ('com.google.android.youtube.tv', 'YouTube', 'streaming'),
    ('com.disney.disneyplus', 'Disney+', 'streaming'),
    ('com.hulu.plus', 'Hulu', 'streaming'),
    ('com.hbo.hbonow', 'HBO Max', 'streaming'),
    ('com.spotify.tv.android', 'Spotify', 'music'),
    ('com.plexapp.android', 'Plex', 'media'),
    ('com.amazon.firetv.youtube', 'YouTube TV', 'streaming'),
    ('com.amazon.venezia', 'Silk Browser', 'utility'),
    ('com.apple.atve.androidtv.appletv', 'Apple TV+', 'streaming'),
    ('com.amazon.ssm', 'Settings', 'system')
ON CONFLICT (package_name) DO NOTHING;

-- ==============================================================================
-- 5. Device Stats View (for dashboard)
-- ==============================================================================
CREATE OR REPLACE VIEW device_stats AS
SELECT
    d.device_id,
    d.name,
    d.status,
    d.last_seen_at,
    COUNT(DISTINCT a.id) AS app_count,
    COUNT(DISTINCT h.id) FILTER (WHERE h.created_at > NOW() - INTERVAL '24 hours') AS commands_24h,
    COUNT(DISTINCT h.id) FILTER (WHERE h.success = true AND h.created_at > NOW() - INTERVAL '24 hours') AS successful_commands_24h,
    ROUND(AVG(h.response_time_ms) FILTER (WHERE h.created_at > NOW() - INTERVAL '24 hours'), 2) AS avg_response_time_ms_24h,
    MAX(h.created_at) AS last_command_at
FROM fire_tv_devices d
LEFT JOIN device_apps a ON d.device_id = a.device_id
LEFT JOIN command_history h ON d.device_id = h.device_id
GROUP BY d.device_id, d.name, d.status, d.last_seen_at;

COMMENT ON VIEW device_stats IS 'Aggregated device statistics for dashboard';

-- ==============================================================================
-- 6. Update Trigger (auto-update updated_at timestamp)
-- ==============================================================================
CREATE OR REPLACE FUNCTION update_updated_at_column()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = NOW();
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

-- Apply trigger to tables with updated_at column
DROP TRIGGER IF EXISTS update_fire_tv_devices_updated_at ON fire_tv_devices;
CREATE TRIGGER update_fire_tv_devices_updated_at
    BEFORE UPDATE ON fire_tv_devices
    FOR EACH ROW
    EXECUTE FUNCTION update_updated_at_column();

DROP TRIGGER IF EXISTS update_device_apps_updated_at ON device_apps;
CREATE TRIGGER update_device_apps_updated_at
    BEFORE UPDATE ON device_apps
    FOR EACH ROW
    EXECUTE FUNCTION update_updated_at_column();

-- ==============================================================================
-- 7. Example Data (optional - comment out for production)
-- ==============================================================================
-- Example device (for testing)
/*
INSERT INTO fire_tv_devices (device_id, name, ip_address, api_key)
VALUES ('livingroom_firetv', 'Living Room Fire TV', '192.168.1.100', '0987654321')
ON CONFLICT (device_id) DO NOTHING;

-- Example apps for the device
INSERT INTO device_apps (device_id, package_name, app_name, is_favorite, sort_order)
SELECT 'livingroom_firetv', package_name, app_name, false, 0
FROM popular_apps
WHERE category = 'streaming'
ON CONFLICT (device_id, package_name) DO NOTHING;
*/

-- ==============================================================================
-- 8. Cleanup Old Data Function (optional maintenance)
-- ==============================================================================
CREATE OR REPLACE FUNCTION cleanup_old_command_history(days INT DEFAULT 30)
RETURNS INT AS $$
DECLARE
    deleted_count INT;
BEGIN
    DELETE FROM command_history
    WHERE created_at < NOW() - INTERVAL '1 day' * days;

    GET DIAGNOSTICS deleted_count = ROW_COUNT;
    RETURN deleted_count;
END;
$$ LANGUAGE plpgsql;

COMMENT ON FUNCTION cleanup_old_command_history IS 'Delete command history older than specified days (default 30)';

-- ==============================================================================
-- Schema Complete
-- ==============================================================================
-- To run cleanup: SELECT cleanup_old_command_history(30);
-- To view stats: SELECT * FROM device_stats;
