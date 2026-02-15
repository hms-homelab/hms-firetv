/**
 * ============================================================================
 * Colada Lightning - Client-Side Router
 * ============================================================================
 * Simple hash-based SPA routing
 */

const Router = {
    routes: {},
    currentRoute: null,

    /**
     * Register a route handler
     * @param {string} path - Route path (e.g., '/', '/devices')
     * @param {Function} handler - Async function to render the page
     */
    register(path, handler) {
        this.routes[path] = handler;
    },

    /**
     * Navigate to a route
     * @param {string} path - Route path
     */
    async navigate(path) {
        // Update URL hash without triggering hashchange
        if (window.location.hash !== `#${path}`) {
            window.location.hash = path;
        }

        // Find matching route
        const handler = this.routes[path] || this.routes['/404'];

        if (!handler) {
            console.error(`No handler found for route: ${path}`);
            return;
        }

        // Update current route
        this.currentRoute = path;

        // Update active nav link
        this.updateNavigation(path);

        try {
            // Execute route handler
            await handler();
        } catch (error) {
            console.error(`Error rendering route ${path}:`, error);
            this.showError('Failed to load page. Please try again.');
        }
    },

    /**
     * Update navigation active state
     * @param {string} path - Current route path
     */
    updateNavigation(path) {
        // Remove active class from all nav links
        document.querySelectorAll('.nav-link').forEach(link => {
            link.classList.remove('active');
        });

        // Add active class to current nav link
        const activeLink = document.querySelector(`.nav-link[data-route="${path}"]`);
        if (activeLink) {
            activeLink.classList.add('active');
        }
    },

    /**
     * Set page title and subtitle
     * @param {string} title - Page title
     * @param {string} subtitle - Page subtitle
     */
    setPageHeader(title, subtitle = '') {
        document.getElementById('page-title').textContent = title;
        document.getElementById('page-subtitle').textContent = subtitle;
    },

    /**
     * Set page action buttons
     * @param {string} html - HTML for action buttons
     */
    setPageActions(html) {
        document.getElementById('page-actions').innerHTML = html;
    },

    /**
     * Render content to main content area
     * @param {string} html - HTML content
     */
    render(html) {
        document.getElementById('content').innerHTML = html;
    },

    /**
     * Show loading spinner
     */
    showLoading() {
        this.render(`
            <div class="flex items-center justify-center h-full">
                <div class="text-center">
                    <div class="spinner w-16 h-16 mx-auto"></div>
                    <p class="mt-4 text-gray-400">Loading...</p>
                </div>
            </div>
        `);
    },

    /**
     * Show error message
     * @param {string} message - Error message
     */
    showError(message) {
        this.render(`
            <div class="flex items-center justify-center h-full">
                <div class="text-center max-w-md">
                    <span class="mdi mdi-alert-circle text-6xl text-red-500"></span>
                    <p class="mt-4 text-xl text-gray-300">Error</p>
                    <p class="mt-2 text-gray-400">${message}</p>
                    <button onclick="location.reload()" class="btn btn-primary mt-6">
                        Reload Page
                    </button>
                </div>
            </div>
        `);
    },

    /**
     * Initialize router
     */
    init() {
        // Handle hash changes
        window.addEventListener('hashchange', () => {
            const path = window.location.hash.slice(1) || '/';
            this.navigate(path);
        });

        // Handle nav link clicks
        document.querySelectorAll('.nav-link').forEach(link => {
            link.addEventListener('click', (e) => {
                e.preventDefault();
                const path = link.getAttribute('data-route');
                this.navigate(path);
            });
        });

        // Navigate to initial route
        const initialPath = window.location.hash.slice(1) || '/';
        this.navigate(initialPath);
    }
};

/**
 * ============================================================================
 * Toast Notifications
 * ============================================================================
 */

const Toast = {
    /**
     * Show a toast notification
     * @param {string} message - Toast message
     * @param {string} type - Toast type (success, error, warning, info)
     * @param {number} duration - Duration in milliseconds (0 = permanent)
     */
    show(message, type = 'info', duration = 5000) {
        const container = document.getElementById('toast-container');
        const id = `toast-${Date.now()}`;

        const toast = document.createElement('div');
        toast.id = id;
        toast.className = `toast toast-${type}`;
        toast.innerHTML = `
            <div class="flex items-start">
                <span class="mdi mdi-${this.getIcon(type)} text-xl mr-3"></span>
                <div class="flex-1">
                    <p class="text-gray-100 font-medium">${message}</p>
                </div>
                <button onclick="Toast.remove('${id}')" class="ml-3 text-gray-400 hover:text-gray-200">
                    <span class="mdi mdi-close"></span>
                </button>
            </div>
        `;

        container.appendChild(toast);

        // Auto-remove after duration
        if (duration > 0) {
            setTimeout(() => this.remove(id), duration);
        }

        return id;
    },

    /**
     * Remove a toast
     * @param {string} id - Toast ID
     */
    remove(id) {
        const toast = document.getElementById(id);
        if (toast) {
            toast.style.animation = 'slideIn 0.3s ease reverse';
            setTimeout(() => toast.remove(), 300);
        }
    },

    /**
     * Get icon for toast type
     * @param {string} type - Toast type
     * @returns {string} MDI icon name
     */
    getIcon(type) {
        const icons = {
            success: 'check-circle',
            error: 'alert-circle',
            warning: 'alert',
            info: 'information'
        };
        return icons[type] || 'information';
    },

    success(message, duration) {
        return this.show(message, 'success', duration);
    },

    error(message, duration) {
        return this.show(message, 'error', duration);
    },

    warning(message, duration) {
        return this.show(message, 'warning', duration);
    },

    info(message, duration) {
        return this.show(message, 'info', duration);
    }
};

/**
 * ============================================================================
 * Modal Helper
 * ============================================================================
 */

const Modal = {
    /**
     * Show a modal
     * @param {string} title - Modal title
     * @param {string} content - Modal content HTML
     * @param {Object} options - Modal options
     */
    show(title, content, options = {}) {
        const container = document.getElementById('modal-container');
        const {
            buttons = [],
            size = 'md',
            closable = true
        } = options;

        let buttonsHtml = '';
        if (buttons.length > 0) {
            buttonsHtml = buttons.map(btn => {
                const className = btn.className || 'btn btn-secondary';
                const onclick = btn.onClick ? `onclick="${btn.onClick}"` : '';
                return `<button class="${className}" ${onclick}>${btn.text}</button>`;
            }).join('');
        }

        container.innerHTML = `
            <div class="modal-backdrop fixed inset-0 flex items-center justify-center p-4" onclick="Modal.close(event)">
                <div class="modal" onclick="event.stopPropagation()">
                    <div class="modal-header flex items-center justify-between">
                        <h3 class="modal-title">${title}</h3>
                        ${closable ? '<button onclick="Modal.close()" class="text-gray-400 hover:text-gray-200"><span class="mdi mdi-close text-2xl"></span></button>' : ''}
                    </div>
                    <div class="modal-body">
                        ${content}
                    </div>
                    ${buttonsHtml ? `<div class="modal-footer">${buttonsHtml}</div>` : ''}
                </div>
            </div>
        `;

        container.classList.remove('hidden');
    },

    /**
     * Close the modal
     */
    close(event) {
        // Only close if clicking backdrop
        if (event && event.target.classList.contains('modal-backdrop')) {
            const container = document.getElementById('modal-container');
            container.classList.add('hidden');
            container.innerHTML = '';
        } else if (!event) {
            // Programmatic close
            const container = document.getElementById('modal-container');
            container.classList.add('hidden');
            container.innerHTML = '';
        }
    },

    /**
     * Show confirmation dialog
     * @param {string} message - Confirmation message
     * @param {Function} onConfirm - Callback when confirmed
     */
    confirm(message, onConfirm) {
        this.show('Confirm Action', `<p class="text-gray-300">${message}</p>`, {
            buttons: [
                {
                    text: 'Cancel',
                    className: 'btn btn-secondary',
                    onClick: 'Modal.close()'
                },
                {
                    text: 'Confirm',
                    className: 'btn btn-danger',
                    onClick: `Modal.close(); (${onConfirm.toString()})()`
                }
            ]
        });
    }
};
