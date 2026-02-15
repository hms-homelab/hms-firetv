/**
 * ============================================================================
 * Colada Lightning - Main Application Initialization
 * ============================================================================
 * Initializes router and registers all page routes
 */

// Wait for DOM to be ready
document.addEventListener('DOMContentLoaded', () => {
    console.log('Colada Lightning UI Initializing...');

    // Register all routes
    Router.register('/', () => DashboardPage.render());
    Router.register('/remote', () => RemotePage.render());
    Router.register('/devices', () => DevicesPage.render());
    Router.register('/apps', () => AppsPage.render());
    Router.register('/settings', () => SettingsPage.render());

    // 404 handler
    Router.register('/404', () => {
        Router.setPageHeader('Page Not Found', 'The page you are looking for does not exist');
        Router.setPageActions('');
        Router.render(`
            <div class="flex items-center justify-center h-full">
                <div class="text-center">
                    <span class="mdi mdi-alert-circle text-6xl text-orange-500"></span>
                    <h2 class="text-3xl font-bold text-gray-100 mt-4">404 - Page Not Found</h2>
                    <p class="text-gray-400 mt-2">The page you are looking for does not exist.</p>
                    <button onclick="Router.navigate('/')" class="btn btn-primary mt-6">
                        <span class="mdi mdi-home mr-2"></span>
                        Go to Dashboard
                    </button>
                </div>
            </div>
        `);
    });

    // Initialize router (this will navigate to initial route)
    Router.init();

    console.log('Colada Lightning UI Ready');
});

// Global error handler
window.addEventListener('error', (event) => {
    console.error('Global error:', event.error);
    Toast.error(`An error occurred: ${event.error?.message || 'Unknown error'}`);
});

// Unhandled promise rejection handler
window.addEventListener('unhandledrejection', (event) => {
    console.error('Unhandled promise rejection:', event.reason);
    Toast.error(`Request failed: ${event.reason?.message || 'Unknown error'}`);
});
