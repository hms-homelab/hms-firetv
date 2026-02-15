/**
 * ============================================================================
 * Colada Lightning - Settings Page
 * ============================================================================
 * Display current configuration (read-only)
 */

const SettingsPage = {
    async render() {
        Router.setPageHeader('Settings', 'View system configuration');
        Router.setPageActions('');
        Router.showLoading();

        try {
            const status = await API.system.status();
            this.renderSettings(status);
        } catch (error) {
            Toast.error(`Failed to load settings: ${error.message}`);
            Router.showError('Failed to load settings');
        }
    },

    renderSettings(status) {
        const { config, connections } = status;

        const html = `
            <div class="max-w-4xl mx-auto">
                <!-- Info Banner -->
                <div class="bg-blue-500 bg-opacity-10 border border-blue-500 rounded-lg p-4 mb-8">
                    <div class="flex items-start">
                        <span class="mdi mdi-information text-2xl text-blue-500 mr-3"></span>
                        <div>
                            <p class="text-gray-200 font-medium">Read-Only Configuration</p>
                            <p class="text-sm text-gray-400 mt-1">
                                Settings are currently loaded from environment variables and cannot be modified through the UI.
                                Changes require container restart.
                            </p>
                        </div>
                    </div>
                </div>

                <!-- Database Configuration -->
                <div class="card mb-6">
                    <div class="flex items-center justify-between mb-6">
                        <h2 class="text-xl font-semibold text-gray-100 flex items-center">
                            <span class="mdi mdi-database text-orange-500 mr-2"></span>
                            Database Configuration
                        </h2>
                        <span class="badge ${connections.database === 'connected' ? 'badge-success' : 'badge-danger'}">
                            ${connections.database}
                        </span>
                    </div>

                    <div class="grid grid-cols-1 md:grid-cols-2 gap-6">
                        <div>
                            <label class="form-label">Host</label>
                            <input type="text" class="form-input" value="${config.db_host}" readonly>
                        </div>
                        <div>
                            <label class="form-label">Port</label>
                            <input type="text" class="form-input" value="5432" readonly>
                        </div>
                        <div>
                            <label class="form-label">Database Name</label>
                            <input type="text" class="form-input" value="firetv" readonly>
                        </div>
                        <div>
                            <label class="form-label">User</label>
                            <input type="text" class="form-input" value="firetv_user" readonly>
                        </div>
                    </div>
                </div>

                <!-- MQTT Configuration -->
                <div class="card mb-6">
                    <div class="flex items-center justify-between mb-6">
                        <h2 class="text-xl font-semibold text-gray-100 flex items-center">
                            <span class="mdi mdi-wifi text-orange-500 mr-2"></span>
                            MQTT Configuration
                        </h2>
                        <span class="badge ${connections.mqtt === 'connected' ? 'badge-success' : 'badge-danger'}">
                            ${connections.mqtt}
                        </span>
                    </div>

                    <div class="grid grid-cols-1 md:grid-cols-2 gap-6">
                        <div>
                            <label class="form-label">Broker Host</label>
                            <input type="text" class="form-input" value="${config.mqtt_broker}" readonly>
                        </div>
                        <div>
                            <label class="form-label">Broker Port</label>
                            <input type="text" class="form-input" value="1883" readonly>
                        </div>
                        <div>
                            <label class="form-label">Discovery Prefix</label>
                            <input type="text" class="form-input" value="homeassistant" readonly>
                            <p class="form-help">MQTT Discovery topic prefix for Home Assistant</p>
                        </div>
                        <div>
                            <label class="form-label">State Topic Prefix</label>
                            <input type="text" class="form-input" value="maestro_hub/colada" readonly>
                            <p class="form-help">Topic prefix for device state and commands</p>
                        </div>
                    </div>
                </div>

                <!-- Application Configuration -->
                <div class="card mb-6">
                    <div class="flex items-center justify-between mb-6">
                        <h2 class="text-xl font-semibold text-gray-100 flex items-center">
                            <span class="mdi mdi-cog text-orange-500 mr-2"></span>
                            Application Configuration
                        </h2>
                        <span class="badge badge-success">Running</span>
                    </div>

                    <div class="grid grid-cols-1 md:grid-cols-2 gap-6">
                        <div>
                            <label class="form-label">API Port</label>
                            <input type="text" class="form-input" value="8888" readonly>
                        </div>
                        <div>
                            <label class="form-label">Device Poll Interval</label>
                            <input type="text" class="form-input" value="${config.poll_interval}s" readonly>
                            <p class="form-help">How often to check device health</p>
                        </div>
                        <div>
                            <label class="form-label">PIN Timeout</label>
                            <input type="text" class="form-input" value="300s" readonly>
                            <p class="form-help">Pairing PIN expiration time</p>
                        </div>
                        <div>
                            <label class="form-label">Command Timeout</label>
                            <input type="text" class="form-input" value="10s" readonly>
                            <p class="form-help">Maximum time for command execution</p>
                        </div>
                    </div>
                </div>

                <!-- Home Assistant Integration -->
                <div class="card mb-6">
                    <div class="flex items-center justify-between mb-6">
                        <h2 class="text-xl font-semibold text-gray-100 flex items-center">
                            <span class="mdi mdi-home-assistant text-orange-500 mr-2"></span>
                            Home Assistant Integration
                        </h2>
                        <span class="badge badge-success">Enabled</span>
                    </div>

                    <div class="space-y-4">
                        <div class="bg-gray-800 rounded-lg p-4">
                            <h3 class="font-medium text-gray-200 mb-2">MQTT Discovery</h3>
                            <p class="text-sm text-gray-400 mb-3">
                                Colada Lightning automatically publishes MQTT Discovery messages for all devices and apps.
                                Entities will appear in Home Assistant without manual configuration.
                            </p>
                            <div class="grid grid-cols-1 md:grid-cols-2 gap-4 text-sm">
                                <div>
                                    <span class="text-gray-400">Device Entities:</span>
                                    <span class="text-gray-200 ml-2">15 remote buttons per device</span>
                                </div>
                                <div>
                                    <span class="text-gray-400">App Entities:</span>
                                    <span class="text-gray-200 ml-2">1 button per app</span>
                                </div>
                            </div>
                        </div>

                        <div class="bg-gray-800 rounded-lg p-4">
                            <h3 class="font-medium text-gray-200 mb-2">MQTT Topics</h3>
                            <div class="space-y-2 text-sm font-mono">
                                <div class="flex items-center justify-between">
                                    <span class="text-gray-400">Discovery:</span>
                                    <span class="text-gray-200">homeassistant/button/colada/{device_id}_{action}/config</span>
                                </div>
                                <div class="flex items-center justify-between">
                                    <span class="text-gray-400">Commands:</span>
                                    <span class="text-gray-200">maestro_hub/colada/{device_id}/{action}</span>
                                </div>
                                <div class="flex items-center justify-between">
                                    <span class="text-gray-400">App Launch:</span>
                                    <span class="text-gray-200">maestro_hub/colada/{device_id}/launch_app</span>
                                </div>
                            </div>
                        </div>
                    </div>
                </div>

                <!-- Docker Information -->
                <div class="card">
                    <h2 class="text-xl font-semibold text-gray-100 mb-4 flex items-center">
                        <span class="mdi mdi-docker text-orange-500 mr-2"></span>
                        Docker Configuration
                    </h2>

                    <div class="bg-gray-800 rounded-lg p-4">
                        <h3 class="font-medium text-gray-200 mb-3">Environment Variables</h3>
                        <p class="text-sm text-gray-400 mb-4">
                            All settings are configured via environment variables in docker-compose.yml.
                            To modify configuration:
                        </p>
                        <ol class="text-sm text-gray-400 space-y-2 list-decimal list-inside">
                            <li>Edit docker-compose.yml in the maestro_hub directory</li>
                            <li>Update the colada-lightning service environment section</li>
                            <li>Restart the container: <code class="text-orange-500">docker compose restart colada-lightning</code></li>
                        </ol>
                    </div>

                    <div class="mt-4 text-sm text-gray-500">
                        <p class="flex items-center">
                            <span class="mdi mdi-information-outline mr-2"></span>
                            Future versions may support live configuration editing through this UI
                        </p>
                    </div>
                </div>
            </div>
        `;

        Router.render(html);
    }
};
