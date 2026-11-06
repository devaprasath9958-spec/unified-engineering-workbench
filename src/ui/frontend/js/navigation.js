/**
 * UNIFIED ENGINEERING WORKBENCH - NAVIGATION & SHELL ROUTING (STAGE 9.3)
 * Handles view switching across the 5 mandatory engineering sections,
 * keyboard accessibility, ARIA tablist patterns, dialog triggers, and Escape dismissal.
 */

const NavigationManager = {
    initialized: false,
    currentView: 'overview',
    lastActiveElement: null,

    viewTitles: {
        'overview': 'System Overview',
        'geometry': 'Geometry & CAD Parsers',
        'thermal': '1D Thermal FEM Analysis',
        'performance': 'Multicore Performance Benchmark',
        'hardware': 'Hardware & Protocol Integration',
        'backprop': 'Backpropagation & Numerical Verification'
    },

    init() {
        if (this.initialized) return;
        this.initialized = true;

        const navContainer = document.querySelector('.sidebar-nav');
        const navItems = Array.from(document.querySelectorAll('.nav-item'));

        // 1. Wire sidebar navigation items click
        navItems.forEach((btn, index) => {
            btn.addEventListener('click', () => {
                const viewId = btn.getAttribute('data-view');
                if (viewId) {
                    this.switchView(viewId);
                    this.closeMobileMenu();
                }
            });
        });

        // 2. Keyboard navigation within sidebar nav tabs (WAI-ARIA Tab Pattern)
        if (navContainer) {
            navContainer.addEventListener('keydown', (e) => {
                const currentFocused = document.activeElement;
                const currentIndex = navItems.indexOf(currentFocused);
                if (currentIndex === -1) return;

                let nextIndex = -1;

                if (e.key === 'ArrowDown' || e.key === 'ArrowRight') {
                    e.preventDefault();
                    nextIndex = (currentIndex + 1) % navItems.length;
                } else if (e.key === 'ArrowUp' || e.key === 'ArrowLeft') {
                    e.preventDefault();
                    nextIndex = (currentIndex - 1 + navItems.length) % navItems.length;
                } else if (e.key === 'Home') {
                    e.preventDefault();
                    nextIndex = 0;
                } else if (e.key === 'End') {
                    e.preventDefault();
                    nextIndex = navItems.length - 1;
                }

                if (nextIndex !== -1) {
                    const targetBtn = navItems[nextIndex];
                    targetBtn.focus();
                    const viewId = targetBtn.getAttribute('data-view');
                    if (viewId) {
                        this.switchView(viewId);
                    }
                }
            });
        }

        // 3. Mobile hamburger menu toggle & backdrop
        const menuToggle = document.getElementById('menu-toggle-btn');
        const backdrop = document.getElementById('mobile-backdrop');
        const sidebar = document.getElementById('app-sidebar');

        if (menuToggle && sidebar) {
            menuToggle.addEventListener('click', () => {
                const isOpen = sidebar.classList.toggle('open');
                menuToggle.setAttribute('aria-expanded', isOpen ? 'true' : 'false');
                if (backdrop) backdrop.classList.toggle('active', isOpen);
            });
        }

        if (backdrop && sidebar) {
            backdrop.addEventListener('click', () => {
                this.closeMobileMenu();
            });
        }

        // 4. Modal dialog open/close triggers
        document.querySelectorAll('[data-open-dialog]').forEach(btn => {
            btn.addEventListener('click', () => {
                const dialogId = btn.getAttribute('data-open-dialog');
                this.openDialog(dialogId, btn);
            });
        });

        document.querySelectorAll('[data-close-dialog]').forEach(btn => {
            btn.addEventListener('click', () => {
                const dialog = btn.closest('.dialog-overlay');
                if (dialog) this.closeDialog(dialog);
            });
        });

        document.querySelectorAll('.dialog-overlay').forEach(overlay => {
            overlay.addEventListener('click', (e) => {
                if (e.target === overlay) {
                    this.closeDialog(overlay);
                }
            });
        });

        // 5. Global Escape key handler
        window.addEventListener('keydown', (e) => {
            if (e.key === 'Escape') {
                const openDialog = document.querySelector('.dialog-overlay.active');
                if (openDialog) {
                    e.preventDefault();
                    this.closeDialog(openDialog);
                    return;
                }
                const openSidebar = document.querySelector('#app-sidebar.open');
                if (openSidebar) {
                    e.preventDefault();
                    this.closeMobileMenu();
                }
            }
        });

        // 6. Initialize default view
        this.switchView(this.currentView);
    },

    switchView(viewId) {
        if (!this.viewTitles[viewId]) return;
        this.currentView = viewId;

        // Update nav items
        const navItems = document.querySelectorAll('.nav-item');
        navItems.forEach(btn => {
            const matches = btn.getAttribute('data-view') === viewId;
            btn.classList.toggle('active', matches);
            btn.setAttribute('aria-selected', matches ? 'true' : 'false');
            btn.setAttribute('tabindex', matches ? '0' : '-1');
        });

        // Update view panels
        const panels = document.querySelectorAll('.view-panel');
        panels.forEach(panel => {
            const matches = panel.id === `view-${viewId}`;
            panel.classList.toggle('active', matches);
            panel.setAttribute('aria-hidden', matches ? 'false' : 'true');
        });

        // Update breadcrumb
        const breadcrumb = document.getElementById('view-breadcrumb');
        if (breadcrumb) {
            breadcrumb.textContent = this.viewTitles[viewId];
        }

        // Notify active screen controllers
        if (viewId === 'geometry' && window.GeometryView && typeof window.GeometryView.onActivated === 'function') {
            window.GeometryView.onActivated();
        } else if (viewId === 'thermal' && window.ThermalView && typeof window.ThermalView.onActivated === 'function') {
            window.ThermalView.onActivated();
        } else if (viewId === 'performance' && window.PerfView && typeof window.PerfView.onActivated === 'function') {
            window.PerfView.onActivated();
        } else if (viewId === 'hardware' && window.HardwareView && typeof window.HardwareView.onActivated === 'function') {
            window.HardwareView.onActivated();
        } else if (viewId === 'backprop' && window.BackpropView && typeof window.BackpropView.onActivated === 'function') {
            window.BackpropView.onActivated();
        }
    },

    openDialog(dialogId, triggerEl) {
        const dialog = document.getElementById(dialogId);
        if (!dialog) return;

        this.lastActiveElement = triggerEl || document.activeElement;
        dialog.classList.add('active');
        dialog.setAttribute('aria-hidden', 'false');

        // Focus the first focusable button inside the dialog
        const firstFocusable = dialog.querySelector('button, [href], input, select, textarea, [tabindex]:not([tabindex="-1"])');
        if (firstFocusable) {
            firstFocusable.focus();
        }
    },

    closeDialog(dialog) {
        if (!dialog) return;
        dialog.classList.remove('active');
        dialog.setAttribute('aria-hidden', 'true');

        if (this.lastActiveElement && typeof this.lastActiveElement.focus === 'function') {
            this.lastActiveElement.focus();
            this.lastActiveElement = null;
        }
    },

    closeMobileMenu() {
        const sidebar = document.getElementById('app-sidebar');
        const backdrop = document.getElementById('mobile-backdrop');
        const menuToggle = document.getElementById('menu-toggle-btn');
        if (sidebar) sidebar.classList.remove('open');
        if (backdrop) backdrop.classList.remove('active');
        if (menuToggle) menuToggle.setAttribute('aria-expanded', 'false');
    }
};
