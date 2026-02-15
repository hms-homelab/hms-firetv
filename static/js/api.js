/**
 * ============================================================================
 * Colada Lightning - API Client
 * ============================================================================
 * Simple fetch wrapper for all API calls
 */

const API = {
    baseURL: '/api',

    /**
     * Make an API request
     * @param {string} endpoint - API endpoint (e.g., '/devices')
     * @param {Object} options - Fetch options
     * @returns {Promise} Response data
     */
    async request(endpoint, options = {}) {
        const url = `${this.baseURL}${endpoint}`;

        const config = {
            headers: {
                'Content-Type': 'application/json',
                ...options.headers
            },
            ...options
        };

        try {
            const response = await fetch(url, config);

            // Handle non-JSON responses (e.g., 204 No Content)
            if (response.status === 204) {
                return null;
            }

            const data = await response.json();

            if (!response.ok) {
                throw new Error(data.detail || `API Error: ${response.statusText}`);
            }

            return data;
        } catch (error) {
            console.error(`API Request Failed [${options.method || 'GET'} ${endpoint}]:`, error);
            throw error;
        }
    },

    /**
     * ========================================================================
     * Device Endpoints
     * ========================================================================
     */
    devices: {
        /**
         * Get all devices
         */
        async list() {
            return API.request('/devices');
        },

        /**
         * Get a specific device
         * @param {string} deviceId - Device ID
         */
        async get(deviceId) {
            return API.request(`/devices/${deviceId}`);
        },

        /**
         * Create a new device
         * @param {Object} device - Device data {device_id, name, ip_address}
         */
        async create(device) {
            return API.request('/devices', {
                method: 'POST',
                body: JSON.stringify(device)
            });
        },

        /**
         * Update a device
         * @param {string} deviceId - Device ID
         * @param {Object} updates - Device updates
         */
        async update(deviceId, updates) {
            return API.request(`/devices/${deviceId}`, {
                method: 'PUT',
                body: JSON.stringify(updates)
            });
        },

        /**
         * Delete a device
         * @param {string} deviceId - Device ID
         */
        async delete(deviceId) {
            return API.request(`/devices/${deviceId}`, {
                method: 'DELETE'
            });
        },

        /**
         * Start pairing process (display PIN on TV)
         * @param {string} deviceId - Device ID
         */
        async startPairing(deviceId) {
            return API.request(`/devices/${deviceId}/pair/start`, {
                method: 'POST'
            });
        },

        /**
         * Verify pairing PIN
         * @param {string} deviceId - Device ID
         * @param {string} pin - PIN code
         */
        async verifyPairing(deviceId, pin) {
            return API.request(`/devices/${deviceId}/pair/verify`, {
                method: 'POST',
                body: JSON.stringify({ pin })
            });
        },

        /**
         * Get pairing status
         * @param {string} deviceId - Device ID
         */
        async getPairingStatus(deviceId) {
            return API.request(`/devices/${deviceId}/pair/status`);
        },

        /**
         * Reset pairing (unpair device)
         * @param {string} deviceId - Device ID
         */
        async resetPairing(deviceId) {
            return API.request(`/devices/${deviceId}/pair/reset`, {
                method: 'POST'
            });
        },

        /**
         * Get device status
         * @param {string} deviceId - Device ID
         */
        async getStatus(deviceId) {
            return API.request(`/devices/${deviceId}/status`);
        },

        /**
         * Get device command history
         * @param {string} deviceId - Device ID
         * @param {number} limit - Number of records to return
         */
        async getHistory(deviceId, limit = 50) {
            return API.request(`/devices/${deviceId}/history?limit=${limit}`);
        }
    },

    /**
     * ========================================================================
     * App Endpoints
     * ========================================================================
     */
    apps: {
        /**
         * Get all apps for a device
         * @param {string} deviceId - Device ID
         */
        async list(deviceId) {
            return API.request(`/devices/${deviceId}/apps`);
        },

        /**
         * Get a specific app
         * @param {string} deviceId - Device ID
         * @param {string} packageName - App package name
         */
        async get(deviceId, packageName) {
            return API.request(`/devices/${deviceId}/apps/${encodeURIComponent(packageName)}`);
        },

        /**
         * Add an app to a device
         * @param {string} deviceId - Device ID
         * @param {Object} app - App data {package_name, app_name, icon_url, is_favorite}
         */
        async add(deviceId, app) {
            return API.request(`/devices/${deviceId}/apps`, {
                method: 'POST',
                body: JSON.stringify(app)
            });
        },

        /**
         * Update an app
         * @param {string} deviceId - Device ID
         * @param {string} packageName - App package name
         * @param {Object} updates - App updates
         */
        async update(deviceId, packageName, updates) {
            return API.request(`/devices/${deviceId}/apps/${encodeURIComponent(packageName)}`, {
                method: 'PUT',
                body: JSON.stringify(updates)
            });
        },

        /**
         * Delete an app from a device
         * @param {string} deviceId - Device ID
         * @param {string} packageName - App package name
         */
        async delete(deviceId, packageName) {
            return API.request(`/devices/${deviceId}/apps/${encodeURIComponent(packageName)}`, {
                method: 'DELETE'
            });
        },

        /**
         * Add popular apps to a device (bulk add)
         * @param {string} deviceId - Device ID
         * @param {string} category - App category (e.g., 'streaming', 'music')
         */
        async addPopular(deviceId, category = 'streaming') {
            return API.request(`/devices/${deviceId}/apps/bulk`, {
                method: 'POST',
                body: JSON.stringify({ category })
            });
        },

        /**
         * Get popular apps catalog
         * @param {string} category - App category filter (optional)
         */
        async getPopular(category = null) {
            const query = category ? `?category=${category}` : '';
            return API.request(`/apps/popular${query}`);
        },

        /**
         * Toggle app favorite status
         * @param {string} deviceId - Device ID
         * @param {string} packageName - App package name
         * @param {boolean} is_favorite - Favorite status
         */
        async toggleFavorite(deviceId, packageName, is_favorite) {
            return API.request(`/devices/${deviceId}/apps/${encodeURIComponent(packageName)}/favorite`, {
                method: 'POST',
                body: JSON.stringify({ is_favorite })
            });
        }
    },

    /**
     * ========================================================================
     * Command Endpoints
     * ========================================================================
     */
    commands: {
        /**
         * Launch an app
         * @param {string} deviceId - Device ID
         * @param {string} packageName - App package name
         */
        async launchApp(deviceId, packageName) {
            return API.request(`/devices/${deviceId}/app`, {
                method: 'POST',
                body: JSON.stringify({ package: packageName })
            });
        },

        /**
         * Send media control command
         * @param {string} deviceId - Device ID
         * @param {string} action - Media action (play, pause, stop, etc.)
         */
        async mediaControl(deviceId, action) {
            return API.request(`/devices/${deviceId}/media`, {
                method: 'POST',
                body: JSON.stringify({ action })
            });
        },

        /**
         * Send navigation command
         * @param {string} deviceId - Device ID
         * @param {string} action - Navigation action (dpad_up, dpad_down, etc.)
         */
        async navigation(deviceId, action) {
            return API.request(`/devices/${deviceId}/navigation`, {
                method: 'POST',
                body: JSON.stringify({ action })
            });
        },

        /**
         * Send keyboard text input
         * @param {string} deviceId - Device ID
         * @param {string} text - Text to type
         */
        async keyboard(deviceId, text) {
            return API.request(`/devices/${deviceId}/keyboard`, {
                method: 'POST',
                body: JSON.stringify({ text })
            });
        }
    },

    /**
     * ========================================================================
     * System Endpoints
     * ========================================================================
     */
    system: {
        /**
         * Get health status
         */
        async health() {
            return fetch('/health').then(r => r.json());
        },

        /**
         * Get detailed status
         */
        async status() {
            return fetch('/status').then(r => r.json());
        }
    },

    /**
     * ========================================================================
     * Stats Endpoints
     * ========================================================================
     */
    stats: {
        /**
         * Get overall system statistics
         */
        async getOverall() {
            return API.request('/stats');
        },

        /**
         * Get per-device statistics
         */
        async getDevices() {
            return API.request('/stats/devices');
        }
    }
};
