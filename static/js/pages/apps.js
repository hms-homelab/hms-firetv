/**
 * ============================================================================
 * Colada Lightning - Apps Page
 * ============================================================================
 * App management per device: list, add, edit, delete, toggle favorite
 */

const AppsPage = {
    currentDevice: null,
    apps: [],
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

            // Load apps for device
            const response = await API.apps.list(deviceId);
            this.apps = response.apps || [];

            this.renderAppManagement();
        } catch (error) {
            Toast.error(`Failed to load apps: ${error.message}`);
            Router.showError('Failed to load apps');
        }
    },

    renderNoDevices() {
        Router.setPageHeader('Apps', 'No devices available');
        Router.setPageActions('');
        Router.render(`
            <div class="empty-state">
                <span class="mdi mdi-television empty-state-icon"></span>
                <h3 class="text-xl text-gray-300 mb-2">No Devices Found</h3>
                <p class="text-gray-400 mb-6">Add a Fire TV device first to manage its apps</p>
                <button onclick="Router.navigate('/devices')" class="btn btn-primary">
                    <span class="mdi mdi-plus mr-2"></span>
                    Add Device
                </button>
            </div>
        `);
    },

    renderAppManagement() {
        Router.setPageHeader(`Apps - ${this.currentDevice.name}`, `Manage apps for ${this.currentDevice.device_id}`);
        Router.setPageActions(`
            <select onchange="AppsPage.switchDevice(this.value)" class="form-select mr-3" style="width: auto;">
                ${this.devices.map(d => `
                    <option value="${d.device_id}" ${d.device_id === this.currentDevice.device_id ? 'selected' : ''}>
                        ${d.name}
                    </option>
                `).join('')}
            </select>
            <button onclick="AppsPage.addDefaultApps()" class="btn btn-secondary mr-2">
                <span class="mdi mdi-download mr-2"></span>
                Add Defaults
            </button>
            <button onclick="AppsPage.showAddAppModal()" class="btn btn-primary">
                <span class="mdi mdi-plus mr-2"></span>
                Add App
            </button>
        `);

        if (this.apps.length === 0) {
            Router.render(`
                <div class="empty-state">
                    <span class="mdi mdi-application empty-state-icon"></span>
                    <h3 class="text-xl text-gray-300 mb-2">No Apps Configured</h3>
                    <p class="text-gray-400 mb-6">Add apps to create Home Assistant button entities</p>
                    <div class="flex gap-3 justify-center">
                        <button onclick="AppsPage.addDefaultApps()" class="btn btn-secondary">
                            <span class="mdi mdi-download mr-2"></span>
                            Add Default Apps
                        </button>
                        <button onclick="AppsPage.showAddAppModal()" class="btn btn-primary">
                            <span class="mdi mdi-plus mr-2"></span>
                            Add Custom App
                        </button>
                    </div>
                </div>
            `);
            return;
        }

        const html = `
            <div class="mb-6 bg-blue-500 bg-opacity-10 border border-blue-500 rounded-lg p-4">
                <div class="flex items-start">
                    <span class="mdi mdi-information text-2xl text-blue-500 mr-3"></span>
                    <div>
                        <p class="text-gray-200 font-medium">Home Assistant Integration</p>
                        <p class="text-sm text-gray-400 mt-1">
                            All apps create button entities in Home Assistant automatically via MQTT Discovery.
                            Use the favorite toggle to organize your apps.
                        </p>
                    </div>
                </div>
            </div>

            <div class="grid-container">
                ${this.apps.map(app => this.renderAppCard(app)).join('')}
            </div>
        `;

        Router.render(html);
    },

    renderAppCard(app) {
        const isFavorite = app.is_favorite || false;
        const lastLaunched = app.last_launched ? new Date(app.last_launched).toLocaleString() : 'Never';

        return `
            <div class="app-card">
                <div class="flex items-start justify-between mb-4">
                    <div class="app-icon">
                        <span class="mdi ${app.icon_url || 'mdi-application'}"></span>
                    </div>
                    <button onclick="AppsPage.toggleFavorite('${app.package_name}', ${!isFavorite})"
                            class="text-2xl ${isFavorite ? 'text-orange-500' : 'text-gray-600'} hover:text-orange-500 transition-colors">
                        <span class="mdi mdi-star${isFavorite ? '' : '-outline'}"></span>
                    </button>
                </div>

                <h3 class="text-xl font-semibold text-gray-100 mb-2">${app.app_name}</h3>
                <p class="text-xs text-gray-500 mb-4 font-mono truncate">${app.package_name}</p>

                <div class="flex items-center text-sm text-gray-400 mb-4">
                    <span class="mdi mdi-clock-outline mr-2"></span>
                    <span>Last launched: ${lastLaunched}</span>
                </div>

                <div class="bg-green-500 bg-opacity-10 border border-green-500 rounded p-2 mb-4">
                    <p class="text-xs text-green-400 flex items-center">
                        <span class="mdi mdi-check-circle mr-1"></span>
                        Creates HA button entity
                    </p>
                </div>

                <div class="divider"></div>

                <div class="flex gap-2">
                    <button onclick="AppsPage.launchApp('${app.package_name}')"
                            class="btn btn-primary btn-sm flex-1">
                        <span class="mdi mdi-play mr-1"></span>
                        Launch
                    </button>
                    <button onclick="AppsPage.showEditAppModal('${app.package_name}')"
                            class="btn btn-secondary btn-sm">
                        <span class="mdi mdi-pencil"></span>
                    </button>
                    <button onclick="AppsPage.confirmDeleteApp('${app.package_name}')"
                            class="btn btn-danger btn-sm">
                        <span class="mdi mdi-delete"></span>
                    </button>
                </div>
            </div>
        `;
    },

    switchDevice(deviceId) {
        window.location.hash = `/apps?device=${deviceId}`;
        this.render();
    },

    showAddAppModal() {
        Modal.show('Add App', `
            <form id="add-app-form" onsubmit="AppsPage.handleAddApp(event)">
                <div class="form-group">
                    <label class="form-label">Package Name</label>
                    <input type="text" name="package_name" class="form-input"
                           placeholder="com.example.app" required>
                    <p class="form-help">Android package name (e.g., com.netflix.ninja)</p>
                </div>

                <div class="form-group">
                    <label class="form-label">App Name</label>
                    <input type="text" name="app_name" class="form-input"
                           placeholder="Netflix" required>
                    <p class="form-help">Friendly name for display</p>
                </div>

                <div class="form-group">
                    <label class="form-label">Icon (MDI)</label>
                    <input type="text" name="icon_url" class="form-input"
                           placeholder="mdi:netflix" value="mdi:application">
                    <p class="form-help">
                        Material Design Icon (e.g., mdi:netflix, mdi:youtube)
                        <a href="https://pictogrammers.com/library/mdi/" target="_blank" class="text-orange-500 hover:text-orange-400">Browse icons</a>
                    </p>
                </div>

                <div class="form-group">
                    <label class="flex items-center cursor-pointer">
                        <input type="checkbox" name="is_favorite" class="form-checkbox mr-3">
                        <span class="form-label mb-0">Mark as Favorite</span>
                    </label>
                    <p class="form-help mt-2">Organize your most-used apps</p>
                </div>
            </form>
        `, {
            buttons: [
                { text: 'Cancel', className: 'btn btn-secondary', onClick: 'Modal.close()' },
                { text: 'Add App', className: 'btn btn-primary', onClick: 'document.getElementById("add-app-form").requestSubmit()' }
            ]
        });
    },

    async handleAddApp(event) {
        event.preventDefault();
        Modal.close();

        const formData = new FormData(event.target);
        const app = {
            package_name: formData.get('package_name'),
            app_name: formData.get('app_name'),
            icon_url: formData.get('icon_url') || 'mdi:application',
            is_favorite: formData.get('is_favorite') === 'on'
        };

        try {
            Toast.info('Adding app...');
            await API.apps.add(this.currentDevice.device_id, app);
            Toast.success(`${app.app_name} added successfully. HA button entity created.`);
            this.render();
        } catch (error) {
            Toast.error(`Failed to add app: ${error.message}`);
        }
    },

    showEditAppModal(packageName) {
        const app = this.apps.find(a => a.package_name === packageName);
        if (!app) return;

        Modal.show('Edit App', `
            <form id="edit-app-form" onsubmit="AppsPage.handleEditApp(event, '${packageName}')">
                <div class="form-group">
                    <label class="form-label">Package Name</label>
                    <input type="text" class="form-input" value="${app.package_name}" disabled>
                    <p class="form-help">Package name cannot be changed</p>
                </div>

                <div class="form-group">
                    <label class="form-label">App Name</label>
                    <input type="text" name="app_name" class="form-input"
                           value="${app.app_name}" required>
                </div>

                <div class="form-group">
                    <label class="form-label">Icon (MDI)</label>
                    <input type="text" name="icon_url" class="form-input"
                           value="${app.icon_url || 'mdi:application'}">
                </div>

                <div class="form-group">
                    <label class="flex items-center cursor-pointer">
                        <input type="checkbox" name="is_favorite" class="form-checkbox mr-3"
                               ${app.is_favorite ? 'checked' : ''}>
                        <span class="form-label mb-0">Mark as Favorite</span>
                    </label>
                </div>
            </form>
        `, {
            buttons: [
                { text: 'Cancel', className: 'btn btn-secondary', onClick: 'Modal.close()' },
                { text: 'Save Changes', className: 'btn btn-primary', onClick: 'document.getElementById("edit-app-form").requestSubmit()' }
            ]
        });
    },

    async handleEditApp(event, packageName) {
        event.preventDefault();
        Modal.close();

        const formData = new FormData(event.target);
        const updates = {
            app_name: formData.get('app_name'),
            icon_url: formData.get('icon_url'),
            is_favorite: formData.get('is_favorite') === 'on'
        };

        try {
            await API.apps.update(this.currentDevice.device_id, packageName, updates);
            Toast.success('App updated successfully');
            this.render();
        } catch (error) {
            Toast.error(`Failed to update app: ${error.message}`);
        }
    },

    async toggleFavorite(packageName, isFavorite) {
        try {
            await API.apps.update(this.currentDevice.device_id, packageName, { is_favorite: isFavorite });
            Toast.success(isFavorite ? 'Added to favorites' : 'Removed from favorites');
            this.render();
        } catch (error) {
            Toast.error(`Failed to update favorite: ${error.message}`);
        }
    },

    async launchApp(packageName) {
        try {
            Toast.info('Launching app...');
            await API.commands.launchApp(this.currentDevice.device_id, packageName);
            Toast.success('App launch command sent');
        } catch (error) {
            Toast.error(`Failed to launch app: ${error.message}`);
        }
    },

    async addDefaultApps() {
        try {
            Toast.info('Adding default apps...');
            const result = await API.apps.addDefaults(this.currentDevice.device_id);
            Toast.success(`Added ${result.apps_added} default apps. HA button entities created.`);
            this.render();
        } catch (error) {
            Toast.error(`Failed to add default apps: ${error.message}`);
        }
    },

    confirmDeleteApp(packageName) {
        const app = this.apps.find(a => a.package_name === packageName);
        if (!app) return;

        Modal.confirm(
            `Are you sure you want to remove <strong>${app.app_name}</strong>? The HA button entity will be removed.`,
            () => this.deleteApp(packageName)
        );
    },

    async deleteApp(packageName) {
        try {
            Toast.info('Removing app...');
            await API.apps.delete(this.currentDevice.device_id, packageName);
            Toast.success('App removed successfully. HA button entity removed.');
            this.render();
        } catch (error) {
            Toast.error(`Failed to remove app: ${error.message}`);
        }
    }
};
