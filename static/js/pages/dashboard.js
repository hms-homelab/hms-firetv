/**
 * ============================================================================
 * Colada Lightning - Dashboard Page
 * ============================================================================
 * Service status, statistics, and quick actions
 */

const DashboardPage = {
    async render() {
        Router.setPageHeader('Dashboard', 'Welcome to Colada Lightning');
        Router.setPageActions('');
        Router.showLoading();

        try {
            const [status, devices] = await Promise.all([
                API.system.status(),
                API.devices.list()
            ]);

            this.renderDashboard(status, devices);
        } catch (error) {
            Toast.error(`Failed to load dashboard: ${error.message}`);
            Router.showError('Failed to load dashboard');
        }
    },

    renderDashboard(status, devices) {
        const { connections, devices: deviceStats, config } = status;

        // Calculate device statistics
        const totalDevices = devices.length;
        const pairedDevices = devices.filter(d => d.client_token && d.client_token !== '').length;
        const onlineDevices = devices.filter(d => d.status === 'online').length;

        const html = `
            <!-- Service Status Cards -->
            <div class="grid grid-cols-1 md:grid-cols-3 gap-6 mb-8">
                <!-- Database Status -->
                <div class="card">
                    <div class="flex items-center justify-between mb-4">
                        <span class="mdi mdi-database text-4xl ${connections.database === 'connected' ? 'text-green-500' : 'text-red-500'}"></span>
                        <span class="badge ${connections.database === 'connected' ? 'badge-success' : 'badge-danger'}">
                            ${connections.database}
                        </span>
                    </div>
                    <h3 class="text-lg font-semibold text-gray-200">Database</h3>
                    <p class="text-sm text-gray-400 mt-1">PostgreSQL</p>
                    <p class="text-xs text-gray-500 mt-2">${config.db_host}</p>
                </div>

                <!-- MQTT Status -->
                <div class="card">
                    <div class="flex items-center justify-between mb-4">
                        <span class="mdi mdi-wifi text-4xl ${connections.mqtt === 'connected' ? 'text-green-500' : 'text-red-500'}"></span>
                        <span class="badge ${connections.mqtt === 'connected' ? 'badge-success' : 'badge-danger'}">
                            ${connections.mqtt}
                        </span>
                    </div>
                    <h3 class="text-lg font-semibold text-gray-200">MQTT Broker</h3>
                    <p class="text-sm text-gray-400 mt-1">EMQX</p>
                    <p class="text-xs text-gray-500 mt-2">${config.mqtt_broker}</p>
                </div>

                <!-- Home Assistant -->
                <div class="card">
                    <div class="flex items-center justify-between mb-4">
                        <span class="mdi mdi-home-assistant text-4xl text-blue-500"></span>
                        <span class="badge badge-info">Available</span>
                    </div>
                    <h3 class="text-lg font-semibold text-gray-200">Home Assistant</h3>
                    <p class="text-sm text-gray-400 mt-1">Integration Ready</p>
                    <p class="text-xs text-gray-500 mt-2">MQTT Discovery Enabled</p>
                </div>
            </div>

            <!-- Device Statistics -->
            <div class="mb-8">
                <h2 class="text-2xl font-semibold text-gray-100 mb-4 flex items-center">
                    <span class="mdi mdi-chart-bar mr-2 text-orange-500"></span>
                    Device Statistics
                </h2>
                <div class="grid grid-cols-1 md:grid-cols-4 gap-6">
                    <div class="card text-center">
                        <div class="text-4xl font-bold text-orange-500 mb-2">${totalDevices}</div>
                        <div class="text-sm text-gray-400">Total Devices</div>
                    </div>
                    <div class="card text-center">
                        <div class="text-4xl font-bold text-green-500 mb-2">${pairedDevices}</div>
                        <div class="text-sm text-gray-400">Paired</div>
                    </div>
                    <div class="card text-center">
                        <div class="text-4xl font-bold text-blue-500 mb-2">${onlineDevices}</div>
                        <div class="text-sm text-gray-400">Online</div>
                    </div>
                    <div class="card text-center">
                        <div class="text-4xl font-bold text-gray-500 mb-2">${totalDevices - pairedDevices}</div>
                        <div class="text-sm text-gray-400">Unpaired</div>
                    </div>
                </div>
            </div>

            <!-- Quick Actions -->
            <div class="mb-8">
                <h2 class="text-2xl font-semibold text-gray-100 mb-4 flex items-center">
                    <span class="mdi mdi-lightning-bolt mr-2 text-orange-500"></span>
                    Quick Actions
                </h2>
                <div class="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4">
                    <button onclick="Router.navigate('/devices')" class="card hover:border-orange-500 transition-all text-left">
                        <div class="flex items-center">
                            <span class="mdi mdi-television text-3xl text-orange-500 mr-4"></span>
                            <div>
                                <h3 class="text-lg font-semibold text-gray-200">Manage Devices</h3>
                                <p class="text-sm text-gray-400">Add, edit, or pair devices</p>
                            </div>
                        </div>
                    </button>

                    <button onclick="Router.navigate('/apps')" class="card hover:border-orange-500 transition-all text-left">
                        <div class="flex items-center">
                            <span class="mdi mdi-application text-3xl text-orange-500 mr-4"></span>
                            <div>
                                <h3 class="text-lg font-semibold text-gray-200">Manage Apps</h3>
                                <p class="text-sm text-gray-400">Configure device apps</p>
                            </div>
                        </div>
                    </button>

                    <a href="/docs" target="_blank" class="card hover:border-orange-500 transition-all text-left">
                        <div class="flex items-center">
                            <span class="mdi mdi-api text-3xl text-orange-500 mr-4"></span>
                            <div>
                                <h3 class="text-lg font-semibold text-gray-200">API Documentation</h3>
                                <p class="text-sm text-gray-400">View REST API docs</p>
                            </div>
                        </div>
                    </a>
                </div>
            </div>

            <!-- Recent Devices -->
            ${devices.length > 0 ? `
                <div class="mb-8">
                    <h2 class="text-2xl font-semibold text-gray-100 mb-4 flex items-center">
                        <span class="mdi mdi-history mr-2 text-orange-500"></span>
                        Your Devices
                    </h2>
                    <div class="table-container">
                        <table class="table">
                            <thead>
                                <tr>
                                    <th>Device</th>
                                    <th>IP Address</th>
                                    <th>Status</th>
                                    <th>Paired</th>
                                    <th>Last Seen</th>
                                    <th>Actions</th>
                                </tr>
                            </thead>
                            <tbody>
                                ${devices.map(device => this.renderDeviceRow(device)).join('')}
                            </tbody>
                        </table>
                    </div>
                </div>
            ` : `
                <div class="card text-center">
                    <span class="mdi mdi-television text-6xl text-gray-600 mb-4"></span>
                    <h3 class="text-xl text-gray-300 mb-2">No Devices Yet</h3>
                    <p class="text-gray-400 mb-6">Add your first Fire TV device to get started</p>
                    <button onclick="Router.navigate('/devices')" class="btn btn-primary">
                        <span class="mdi mdi-plus mr-2"></span>
                        Add Device
                    </button>
                </div>
            `}

            <!-- Info Box -->
            <div class="bg-orange-500 bg-opacity-10 border border-orange-500 rounded-lg p-6">
                <div class="flex items-start">
                    <span class="mdi mdi-fire text-4xl text-orange-500 mr-4"></span>
                    <div class="flex-1">
                        <h3 class="text-xl font-semibold text-gray-100 mb-2">About Colada Lightning</h3>
                        <p class="text-gray-300 mb-3">
                            Colada Lightning enables local control of Amazon Fire TV devices through Home Assistant
                            using the Fire TV Lightning protocol and MQTT Discovery.
                        </p>
                        <ul class="space-y-2 text-sm text-gray-400">
                            <li class="flex items-center">
                                <span class="mdi mdi-check-circle text-green-500 mr-2"></span>
                                No cloud dependencies - fully local control
                            </li>
                            <li class="flex items-center">
                                <span class="mdi mdi-check-circle text-green-500 mr-2"></span>
                                Automatic MQTT Discovery for Home Assistant
                            </li>
                            <li class="flex items-center">
                                <span class="mdi mdi-check-circle text-green-500 mr-2"></span>
                                Remote control buttons and app launchers
                            </li>
                            <li class="flex items-center">
                                <span class="mdi mdi-check-circle text-green-500 mr-2"></span>
                                PIN-based secure pairing
                            </li>
                        </ul>
                    </div>
                </div>
            </div>
        `;

        Router.render(html);
    },

    renderDeviceRow(device) {
        const statusBadge = {
            online: '<span class="badge badge-success">Online</span>',
            offline: '<span class="badge badge-gray">Offline</span>',
            pairing: '<span class="badge badge-warning">Pairing</span>',
            error: '<span class="badge badge-danger">Error</span>'
        }[device.status] || '<span class="badge badge-gray">Unknown</span>';

        const isPaired = device.client_token && device.client_token !== '';
        const pairedBadge = isPaired
            ? '<span class="badge badge-success">Yes</span>'
            : '<span class="badge badge-gray">No</span>';

        const lastSeen = device.last_seen
            ? new Date(device.last_seen).toLocaleString()
            : 'Never';

        return `
            <tr>
                <td>
                    <div class="flex items-center">
                        <span class="mdi mdi-television text-2xl text-orange-500 mr-3"></span>
                        <div>
                            <div class="font-medium text-gray-200">${device.name}</div>
                            <div class="text-xs text-gray-500">${device.device_id}</div>
                        </div>
                    </div>
                </td>
                <td class="text-gray-400">${device.ip_address}</td>
                <td>${statusBadge}</td>
                <td>${pairedBadge}</td>
                <td class="text-gray-400">${lastSeen}</td>
                <td>
                    <div class="flex gap-2">
                        ${!isPaired ? `
                            <button onclick="Router.navigate('/devices')" class="btn btn-primary btn-sm">
                                Pair
                            </button>
                        ` : `
                            <button onclick="Router.navigate('/apps?device=${device.device_id}')" class="btn btn-primary btn-sm">
                                Apps
                            </button>
                        `}
                    </div>
                </td>
            </tr>
        `;
    }
};
