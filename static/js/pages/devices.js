/**
 * ============================================================================
 * Colada Lightning - Devices Page
 * ============================================================================
 * Device management: list, create, pair, edit, delete
 */

const DevicesPage = {
    devices: [],

    async render() {
        Router.setPageHeader('Devices', 'Manage your Fire TV devices');
        Router.setPageActions(`
            <button onclick="DevicesPage.showAddDeviceModal()" class="btn btn-primary">
                <span class="mdi mdi-plus mr-2"></span>
                Add Device
            </button>
        `);

        Router.showLoading();

        try {
            this.devices = await API.devices.list();
            this.renderDeviceList();
        } catch (error) {
            Toast.error(`Failed to load devices: ${error.message}`);
            Router.showError('Failed to load devices');
        }
    },

    renderDeviceList() {
        if (this.devices.length === 0) {
            Router.render(`
                <div class="empty-state">
                    <span class="mdi mdi-television empty-state-icon"></span>
                    <h3 class="text-xl text-gray-300 mb-2">No Devices Found</h3>
                    <p class="text-gray-400 mb-6">Add your first Fire TV device to get started</p>
                    <button onclick="DevicesPage.showAddDeviceModal()" class="btn btn-primary">
                        <span class="mdi mdi-plus mr-2"></span>
                        Add Device
                    </button>
                </div>
            `);
            return;
        }

        const html = `
            <div class="grid-container">
                ${this.devices.map(device => this.renderDeviceCard(device)).join('')}
            </div>
        `;

        Router.render(html);
    },

    renderDeviceCard(device) {
        const statusBadge = this.getStatusBadge(device.status);
        const lastSeen = device.last_seen ? new Date(device.last_seen).toLocaleString() : 'Never';
        const isPaired = device.client_token && device.client_token !== '';

        return `
            <div class="device-card">
                <div class="flex items-start justify-between mb-4">
                    <div class="device-icon">
                        <span class="mdi mdi-television"></span>
                    </div>
                    <div>${statusBadge}</div>
                </div>

                <h3 class="text-xl font-semibold text-gray-100 mb-2">${device.name}</h3>
                <p class="text-sm text-gray-400 mb-4">
                    <span class="mdi mdi-identifier mr-1"></span>
                    ${device.device_id}
                </p>

                <div class="space-y-2 mb-4 text-sm">
                    <div class="flex items-center text-gray-400">
                        <span class="mdi mdi-ip mr-2"></span>
                        <span>${device.ip_address}</span>
                    </div>
                    <div class="flex items-center text-gray-400">
                        <span class="mdi mdi-clock-outline mr-2"></span>
                        <span>Last seen: ${lastSeen}</span>
                    </div>
                    <div class="flex items-center ${isPaired ? 'text-green-500' : 'text-orange-500'}">
                        <span class="mdi mdi-${isPaired ? 'check-circle' : 'alert-circle'} mr-2"></span>
                        <span>${isPaired ? 'Paired' : 'Not Paired'}</span>
                    </div>
                </div>

                <div class="divider"></div>

                <div class="flex flex-wrap gap-2">
                    ${!isPaired ? `
                        <button onclick="DevicesPage.startPairing('${device.device_id}')" class="btn btn-primary btn-sm flex-1">
                            <span class="mdi mdi-link-variant mr-1"></span>
                            Pair
                        </button>
                    ` : `
                        <button onclick="Router.navigate('/apps?device=${device.device_id}')" class="btn btn-primary btn-sm flex-1">
                            <span class="mdi mdi-application mr-1"></span>
                            Apps
                        </button>
                    `}
                    <button onclick="DevicesPage.showDeviceMenu('${device.device_id}')" class="btn btn-secondary btn-sm">
                        <span class="mdi mdi-dots-vertical"></span>
                    </button>
                </div>
            </div>
        `;
    },

    getStatusBadge(status) {
        const badges = {
            online: '<span class="badge badge-success">Online</span>',
            offline: '<span class="badge badge-gray">Offline</span>',
            pairing: '<span class="badge badge-warning">Pairing</span>',
            error: '<span class="badge badge-danger">Error</span>'
        };
        return badges[status] || '<span class="badge badge-gray">Unknown</span>';
    },

    showAddDeviceModal() {
        Modal.show('Add Fire TV Device', `
            <form id="add-device-form" onsubmit="DevicesPage.handleAddDevice(event)">
                <div class="form-group">
                    <label class="form-label">Device ID</label>
                    <input type="text" name="device_id" class="form-input"
                           placeholder="living_room" required
                           pattern="[a-z0-9_]+"
                           title="Lowercase letters, numbers, and underscores only">
                    <p class="form-help">Unique identifier (e.g., living_room, bedroom_tv)</p>
                </div>

                <div class="form-group">
                    <label class="form-label">Device Name</label>
                    <input type="text" name="name" class="form-input"
                           placeholder="Living Room Fire TV" required>
                    <p class="form-help">Friendly name for display</p>
                </div>

                <div class="form-group">
                    <label class="form-label">IP Address</label>
                    <input type="text" name="ip_address" class="form-input"
                           placeholder="192.168.1.100" required
                           pattern="^(?:[0-9]{1,3}\\.){3}[0-9]{1,3}$"
                           title="Valid IPv4 address">
                    <p class="form-help">Fire TV device IP address on your network</p>
                </div>
            </form>
        `, {
            buttons: [
                { text: 'Cancel', className: 'btn btn-secondary', onClick: 'Modal.close()' },
                { text: 'Create Device', className: 'btn btn-primary', onClick: 'document.getElementById("add-device-form").requestSubmit()' }
            ]
        });
    },

    async handleAddDevice(event) {
        event.preventDefault();
        Modal.close();

        const formData = new FormData(event.target);
        const device = {
            device_id: formData.get('device_id'),
            name: formData.get('name'),
            ip_address: formData.get('ip_address')
        };

        try {
            Toast.info('Creating device...');
            await API.devices.create(device);
            Toast.success(`Device ${device.name} created successfully`);

            // Ask if user wants to pair now
            Modal.show('Device Created', `
                <p class="text-gray-300 mb-4">
                    Device <strong>${device.name}</strong> has been created successfully.
                </p>
                <p class="text-gray-400">
                    Would you like to pair it now? This will display a PIN on your Fire TV screen.
                </p>
            `, {
                buttons: [
                    { text: 'Later', className: 'btn btn-secondary', onClick: 'Modal.close(); DevicesPage.render()' },
                    { text: 'Pair Now', className: 'btn btn-primary', onClick: `Modal.close(); DevicesPage.startPairing('${device.device_id}')` }
                ]
            });
        } catch (error) {
            Toast.error(`Failed to create device: ${error.message}`);
        }
    },

    async startPairing(deviceId) {
        Modal.show('Pairing Device', `
            <div class="text-center">
                <div class="spinner w-16 h-16 mx-auto mb-4"></div>
                <p class="text-gray-300">Starting pairing process...</p>
                <p class="text-sm text-gray-400 mt-2">Please wait</p>
            </div>
        `, { closable: false });

        try {
            const result = await API.devices.startPairing(deviceId);
            const pin = result.pin || 'UNKNOWN';

            this.showPairingPIN(deviceId, pin);
        } catch (error) {
            Modal.close();
            Toast.error(`Failed to start pairing: ${error.message}`);
        }
    },

    showPairingPIN(deviceId, pin) {
        let countdown = 300; // 5 minutes

        Modal.show('Enter PIN on Fire TV', `
            <div class="text-center">
                <p class="text-gray-300 mb-4">A PIN is now displayed on your Fire TV screen.</p>
                <p class="text-sm text-gray-400 mb-6">Enter this PIN on your TV:</p>

                <div class="bg-gray-800 border-2 border-orange-500 rounded-lg p-8 mb-6">
                    <div class="text-6xl font-bold text-orange-500 tracking-widest">${pin}</div>
                </div>

                <p class="text-sm text-gray-400 mb-2">
                    <span class="mdi mdi-timer-outline mr-1"></span>
                    PIN expires in: <span id="pin-countdown" class="text-orange-500">5:00</span>
                </p>

                <div class="mt-6 space-y-3">
                    <p class="text-xs text-gray-500">
                        After entering the PIN on your TV, click "Complete Pairing" below.
                    </p>
                </div>
            </div>
        `, {
            closable: false,
            buttons: [
                { text: 'Cancel', className: 'btn btn-secondary', onClick: 'Modal.close(); DevicesPage.render()' },
                { text: 'Complete Pairing', className: 'btn btn-primary', onClick: `DevicesPage.verifyPairing('${deviceId}', '${pin}')` }
            ]
        });

        // Start countdown timer
        const timer = setInterval(() => {
            countdown--;
            const minutes = Math.floor(countdown / 60);
            const seconds = countdown % 60;
            const display = `${minutes}:${seconds.toString().padStart(2, '0')}`;

            const el = document.getElementById('pin-countdown');
            if (el) {
                el.textContent = display;
            }

            if (countdown <= 0) {
                clearInterval(timer);
                Modal.close();
                Toast.error('PIN expired. Please try pairing again.');
                this.render();
            }
        }, 1000);
    },

    async verifyPairing(deviceId, pin) {
        Modal.show('Verifying Pairing', `
            <div class="text-center">
                <div class="spinner w-16 h-16 mx-auto mb-4"></div>
                <p class="text-gray-300">Verifying PIN...</p>
                <p class="text-sm text-gray-400 mt-2">Please wait</p>
            </div>
        `, { closable: false });

        try {
            await API.devices.verifyPairing(deviceId, pin);
            Modal.close();
            Toast.success('Device paired successfully!');
            this.render();
        } catch (error) {
            Modal.close();
            Toast.error(`Pairing failed: ${error.message}`);
        }
    },

    showDeviceMenu(deviceId) {
        const device = this.devices.find(d => d.device_id === deviceId);
        if (!device) return;

        Modal.show(`${device.name} - Actions`, `
            <div class="space-y-2">
                <button onclick="Modal.close(); DevicesPage.republishDiscovery('${deviceId}')"
                        class="w-full btn btn-secondary text-left flex items-center">
                    <span class="mdi mdi-refresh mr-2"></span>
                    Republish MQTT Discovery
                </button>

                <button onclick="Modal.close(); DevicesPage.showEditDevice('${deviceId}')"
                        class="w-full btn btn-secondary text-left flex items-center">
                    <span class="mdi mdi-pencil mr-2"></span>
                    Edit Device
                </button>

                <button onclick="Modal.close(); DevicesPage.confirmDelete('${deviceId}')"
                        class="w-full btn btn-danger text-left flex items-center">
                    <span class="mdi mdi-delete mr-2"></span>
                    Delete Device
                </button>
            </div>
        `, {
            buttons: [
                { text: 'Close', className: 'btn btn-secondary', onClick: 'Modal.close()' }
            ]
        });
    },

    async republishDiscovery(deviceId) {
        try {
            Toast.info('Republishing MQTT discovery...');
            await API.devices.republishDiscovery(deviceId);
            Toast.success('MQTT discovery republished successfully');
        } catch (error) {
            Toast.error(`Failed to republish discovery: ${error.message}`);
        }
    },

    showEditDevice(deviceId) {
        const device = this.devices.find(d => d.device_id === deviceId);
        if (!device) return;

        Modal.show('Edit Device', `
            <form id="edit-device-form" onsubmit="DevicesPage.handleEditDevice(event, '${deviceId}')">
                <div class="form-group">
                    <label class="form-label">Device Name</label>
                    <input type="text" name="name" class="form-input"
                           value="${device.name}" required>
                </div>

                <div class="form-group">
                    <label class="form-label">IP Address</label>
                    <input type="text" name="ip_address" class="form-input"
                           value="${device.ip_address}" required
                           pattern="^(?:[0-9]{1,3}\\.){3}[0-9]{1,3}$">
                </div>
            </form>
        `, {
            buttons: [
                { text: 'Cancel', className: 'btn btn-secondary', onClick: 'Modal.close()' },
                { text: 'Save Changes', className: 'btn btn-primary', onClick: 'document.getElementById("edit-device-form").requestSubmit()' }
            ]
        });
    },

    async handleEditDevice(event, deviceId) {
        event.preventDefault();
        Modal.close();

        const formData = new FormData(event.target);
        const updates = {
            name: formData.get('name'),
            ip_address: formData.get('ip_address')
        };

        try {
            await API.devices.update(deviceId, updates);
            Toast.success('Device updated successfully');
            this.render();
        } catch (error) {
            Toast.error(`Failed to update device: ${error.message}`);
        }
    },

    confirmDelete(deviceId) {
        const device = this.devices.find(d => d.device_id === deviceId);
        if (!device) return;

        Modal.confirm(
            `Are you sure you want to delete <strong>${device.name}</strong>? This action cannot be undone.`,
            () => this.deleteDevice(deviceId)
        );
    },

    async deleteDevice(deviceId) {
        try {
            Toast.info('Deleting device...');
            await API.devices.delete(deviceId);
            Toast.success('Device deleted successfully');
            this.render();
        } catch (error) {
            Toast.error(`Failed to delete device: ${error.message}`);
        }
    }
};
