/**
 * ============================================================================
 * Colada Lightning - Remote Control Page
 * ============================================================================
 * Visual remote control with D-pad, keyboard input, and media controls
 */

const RemotePage = {
    currentDevice: null,
    devices: [],

    async render() {
        // Get device ID from URL query parameter or use first device
        const urlParams = new URLSearchParams(window.location.hash.split('?')[1]);
        let deviceId = urlParams.get('device');

        Router.showLoading();

        try {
            // Load devices
            this.devices = await API.devices.list();

            if (this.devices.length === 0) {
                this.renderNoDevices();
                return;
            }

            // If no device specified, use first paired device
            if (!deviceId) {
                const pairedDevice = this.devices.find(d => d.client_token && d.client_token !== '');
                deviceId = pairedDevice ? pairedDevice.device_id : this.devices[0].device_id;
            }

            this.currentDevice = this.devices.find(d => d.device_id === deviceId);

            if (!this.currentDevice) {
                Toast.error('Device not found');
                Router.navigate('/devices');
                return;
            }

            // Check if device is paired
            if (!this.currentDevice.client_token || this.currentDevice.client_token === '') {
                this.renderUnpairedDevice();
                return;
            }

            this.renderRemoteControl();
        } catch (error) {
            Toast.error(`Failed to load remote: ${error.message}`);
            Router.showError('Failed to load remote control');
        }
    },

    renderNoDevices() {
        Router.setPageHeader('Remote Control', 'No devices available');
        Router.setPageActions('');
        Router.render(`
            <div class="empty-state">
                <span class="mdi mdi-remote empty-state-icon"></span>
                <h3 class="text-xl text-gray-300 mb-2">No Devices Found</h3>
                <p class="text-gray-400 mb-6">Add a Fire TV device to use the remote control</p>
                <button onclick="Router.navigate('/devices')" class="btn btn-primary">
                    <span class="mdi mdi-plus mr-2"></span>
                    Add Device
                </button>
            </div>
        `);
    },

    renderUnpairedDevice() {
        Router.setPageHeader(`Remote - ${this.currentDevice.name}`, 'Device not paired');
        Router.setPageActions('');
        Router.render(`
            <div class="empty-state">
                <span class="mdi mdi-link-variant-off empty-state-icon text-orange-500"></span>
                <h3 class="text-xl text-gray-300 mb-2">Device Not Paired</h3>
                <p class="text-gray-400 mb-6">
                    <strong>${this.currentDevice.name}</strong> must be paired before using the remote control
                </p>
                <button onclick="Router.navigate('/devices')" class="btn btn-primary">
                    <span class="mdi mdi-link-variant mr-2"></span>
                    Go to Devices & Pair
                </button>
            </div>
        `);
    },

    renderRemoteControl() {
        Router.setPageHeader(`Remote - ${this.currentDevice.name}`, 'Fire TV Virtual Remote');
        Router.setPageActions(`
            <select onchange="RemotePage.switchDevice(this.value)" class="form-select" style="width: auto;">
                ${this.devices.filter(d => d.client_token && d.client_token !== '').map(d => `
                    <option value="${d.device_id}" ${d.device_id === this.currentDevice.device_id ? 'selected' : ''}>
                        ${d.name}
                    </option>
                `).join('')}
            </select>
        `);

        const html = `
            <div class="max-w-6xl mx-auto">
                <!-- Keyboard Input Section -->
                <div class="card mb-6">
                    <h2 class="text-xl font-semibold text-gray-100 mb-4 flex items-center">
                        <span class="mdi mdi-keyboard text-orange-500 mr-2"></span>
                        Keyboard Input
                    </h2>
                    <p class="text-sm text-gray-400 mb-4">
                        Type text to send to the Fire TV (useful for search, login, etc.)
                    </p>
                    <form onsubmit="RemotePage.sendKeyboard(event)" class="flex gap-3">
                        <input
                            type="text"
                            id="keyboard-input"
                            class="form-input flex-1"
                            placeholder="Type text here..."
                            autocomplete="off">
                        <button type="submit" class="btn btn-primary">
                            <span class="mdi mdi-send mr-2"></span>
                            Send
                        </button>
                    </form>
                </div>

                <!-- Remote Control Grid -->
                <div class="grid grid-cols-1 lg:grid-cols-2 gap-6">
                    <!-- D-Pad Navigation -->
                    <div class="card">
                        <h3 class="text-lg font-semibold text-gray-100 mb-6 text-center">
                            Navigation
                        </h3>
                        <div class="flex flex-col items-center">
                            <!-- D-Pad Container -->
                            <div class="relative" style="width: 280px; height: 280px;">
                                <!-- Up -->
                                <button
                                    onclick="RemotePage.sendNavigation('dpad_up')"
                                    class="absolute top-0 left-1/2 transform -translate-x-1/2
                                           w-20 h-20 rounded-t-2xl bg-gray-700 hover:bg-orange-600
                                           transition-all active:scale-95 flex items-center justify-center
                                           border-2 border-gray-600">
                                    <span class="mdi mdi-chevron-up text-4xl text-gray-200"></span>
                                </button>

                                <!-- Left -->
                                <button
                                    onclick="RemotePage.sendNavigation('dpad_left')"
                                    class="absolute left-0 top-1/2 transform -translate-y-1/2
                                           w-20 h-20 rounded-l-2xl bg-gray-700 hover:bg-orange-600
                                           transition-all active:scale-95 flex items-center justify-center
                                           border-2 border-gray-600">
                                    <span class="mdi mdi-chevron-left text-4xl text-gray-200"></span>
                                </button>

                                <!-- Center (Select) -->
                                <button
                                    onclick="RemotePage.sendNavigation('select')"
                                    class="absolute top-1/2 left-1/2 transform -translate-x-1/2 -translate-y-1/2
                                           w-24 h-24 rounded-full bg-orange-600 hover:bg-orange-700
                                           transition-all active:scale-95 flex items-center justify-center
                                           border-4 border-orange-500 shadow-lg shadow-orange-500/50">
                                    <span class="text-sm font-bold text-white">SELECT</span>
                                </button>

                                <!-- Right -->
                                <button
                                    onclick="RemotePage.sendNavigation('dpad_right')"
                                    class="absolute right-0 top-1/2 transform -translate-y-1/2
                                           w-20 h-20 rounded-r-2xl bg-gray-700 hover:bg-orange-600
                                           transition-all active:scale-95 flex items-center justify-center
                                           border-2 border-gray-600">
                                    <span class="mdi mdi-chevron-right text-4xl text-gray-200"></span>
                                </button>

                                <!-- Down -->
                                <button
                                    onclick="RemotePage.sendNavigation('dpad_down')"
                                    class="absolute bottom-0 left-1/2 transform -translate-x-1/2
                                           w-20 h-20 rounded-b-2xl bg-gray-700 hover:bg-orange-600
                                           transition-all active:scale-95 flex items-center justify-center
                                           border-2 border-gray-600">
                                    <span class="mdi mdi-chevron-down text-4xl text-gray-200"></span>
                                </button>
                            </div>

                            <!-- System Navigation Buttons -->
                            <div class="grid grid-cols-3 gap-3 mt-8 w-full max-w-sm">
                                <button onclick="RemotePage.sendNavigation('back')" class="btn btn-secondary flex flex-col items-center py-4">
                                    <span class="mdi mdi-arrow-left text-2xl mb-1"></span>
                                    <span class="text-xs">Back</span>
                                </button>
                                <button onclick="RemotePage.sendNavigation('home')" class="btn btn-primary flex flex-col items-center py-4">
                                    <span class="mdi mdi-home text-2xl mb-1"></span>
                                    <span class="text-xs">Home</span>
                                </button>
                                <button onclick="RemotePage.sendNavigation('menu')" class="btn btn-secondary flex flex-col items-center py-4">
                                    <span class="mdi mdi-menu text-2xl mb-1"></span>
                                    <span class="text-xs">Menu</span>
                                </button>
                            </div>
                        </div>
                    </div>

                    <!-- Media Controls & Power -->
                    <div class="space-y-6">
                        <!-- Media Controls -->
                        <div class="card">
                            <h3 class="text-lg font-semibold text-gray-100 mb-6 text-center">
                                Media Controls
                            </h3>
                            <div class="grid grid-cols-3 gap-3">
                                <button onclick="RemotePage.sendMedia('scan', 'back')" class="btn btn-secondary flex flex-col items-center py-6">
                                    <span class="mdi mdi-rewind text-3xl mb-2"></span>
                                    <span class="text-xs">Rewind</span>
                                </button>
                                <button onclick="RemotePage.sendMedia('play')" class="btn btn-primary flex flex-col items-center py-6">
                                    <span class="mdi mdi-play-pause text-3xl mb-2"></span>
                                    <span class="text-xs">Play/Pause</span>
                                </button>
                                <button onclick="RemotePage.sendMedia('scan', 'forward')" class="btn btn-secondary flex flex-col items-center py-6">
                                    <span class="mdi mdi-fast-forward text-3xl mb-2"></span>
                                    <span class="text-xs">Forward</span>
                                </button>
                            </div>
                        </div>

                        <!-- Volume Controls -->
                        <div class="card">
                            <h3 class="text-lg font-semibold text-gray-100 mb-6 text-center">
                                Volume
                            </h3>
                            <div class="grid grid-cols-3 gap-3">
                                <button onclick="RemotePage.sendNavigation('volume_down')" class="btn btn-secondary flex flex-col items-center py-6">
                                    <span class="mdi mdi-volume-minus text-3xl mb-2"></span>
                                    <span class="text-xs">Volume -</span>
                                </button>
                                <button onclick="RemotePage.sendNavigation('mute')" class="btn btn-secondary flex flex-col items-center py-6">
                                    <span class="mdi mdi-volume-mute text-3xl mb-2"></span>
                                    <span class="text-xs">Mute</span>
                                </button>
                                <button onclick="RemotePage.sendNavigation('volume_up')" class="btn btn-secondary flex flex-col items-center py-6">
                                    <span class="mdi mdi-volume-plus text-3xl mb-2"></span>
                                    <span class="text-xs">Volume +</span>
                                </button>
                            </div>
                        </div>

                        <!-- Power Controls -->
                        <div class="card">
                            <h3 class="text-lg font-semibold text-gray-100 mb-6 text-center">
                                Power
                            </h3>
                            <div class="grid grid-cols-2 gap-3">
                                <button onclick="RemotePage.sendNavigation('sleep')" class="btn btn-danger flex flex-col items-center py-6">
                                    <span class="mdi mdi-power-sleep text-3xl mb-2"></span>
                                    <span class="text-xs">Sleep</span>
                                </button>
                                <button onclick="RemotePage.sendNavigation('wake')" class="btn btn-success flex flex-col items-center py-6">
                                    <span class="mdi mdi-power text-3xl mb-2"></span>
                                    <span class="text-xs">Wake</span>
                                </button>
                            </div>
                        </div>
                    </div>
                </div>

                <!-- Info Box -->
                <div class="bg-blue-500 bg-opacity-10 border border-blue-500 rounded-lg p-4 mt-6">
                    <div class="flex items-start">
                        <span class="mdi mdi-information text-2xl text-blue-500 mr-3"></span>
                        <div>
                            <p class="text-gray-200 font-medium">Remote Control Tips</p>
                            <ul class="text-sm text-gray-400 mt-2 space-y-1">
                                <li>• Use the D-pad to navigate menus and content</li>
                                <li>• Press SELECT to confirm selections</li>
                                <li>• Use keyboard input for search and login fields</li>
                                <li>• All commands are sent directly to your Fire TV</li>
                            </ul>
                        </div>
                    </div>
                </div>
            </div>
        `;

        Router.render(html);
    },

    switchDevice(deviceId) {
        window.location.hash = `/remote?device=${deviceId}`;
        this.render();
    },

    async sendNavigation(action) {
        try {
            await API.commands.navigation(this.currentDevice.device_id, action);
            // Visual feedback
            const actionName = action.replace('dpad_', '').replace('_', ' ').toUpperCase();
            Toast.success(`Sent: ${actionName}`, 1000);
        } catch (error) {
            Toast.error(`Failed to send command: ${error.message}`);
        }
    },

    async sendMedia(action, direction = null) {
        try {
            if (action === 'scan' && direction) {
                // For scan commands, we need to call the media endpoint with direction
                await API.commands.mediaControl(this.currentDevice.device_id, action);
            } else {
                await API.commands.mediaControl(this.currentDevice.device_id, action);
            }
            const actionName = action.toUpperCase();
            Toast.success(`Sent: ${actionName}`, 1000);
        } catch (error) {
            Toast.error(`Failed to send command: ${error.message}`);
        }
    },

    async sendKeyboard(event) {
        event.preventDefault();

        const input = document.getElementById('keyboard-input');
        const text = input.value.trim();

        if (!text) {
            Toast.warning('Please enter some text');
            return;
        }

        try {
            Toast.info('Sending text...');
            await API.commands.keyboard(this.currentDevice.device_id, text);
            Toast.success(`Text sent: "${text}"`);
            input.value = ''; // Clear input
        } catch (error) {
            Toast.error(`Failed to send text: ${error.message}`);
        }
    }
};
