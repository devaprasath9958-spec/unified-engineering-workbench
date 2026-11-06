/**
 * UNIFIED ENGINEERING WORKBENCH - SHARED COMPONENTS LOGIC (STAGE 9.3)
 * Non-blocking Toast Manager, Alert handlers, and form submission protection.
 * Strictly eliminates browser window.alert() in favor of workbench UI components.
 */

const ToastManager = {
    container: null,

    ensureContainer() {
        if (!this.container) {
            this.container = document.getElementById('toast-container');
            if (!this.container) {
                this.container = document.createElement('div');
                this.container.id = 'toast-container';
                this.container.className = 'toast-container';
                this.container.setAttribute('aria-live', 'polite');
                this.container.setAttribute('aria-atomic', 'true');
                document.body.appendChild(this.container);
            }
        }
        return this.container;
    },

    /**
     * Show a non-blocking toast notification.
     * @param {'info'|'success'|'warning'|'error'} type 
     * @param {string} title 
     * @param {string} message 
     * @param {number} durationMs 
     */
    show(type = 'info', title = '', message = '', durationMs = 4000) {
        const container = this.ensureContainer();

        const toast = document.createElement('div');
        toast.className = `toast toast-${type}`;
        toast.setAttribute('role', type === 'error' ? 'alert' : 'status');

        let iconSvg = '';
        if (window.WorkbenchIcons) {
            if (type === 'success') iconSvg = WorkbenchIcons.check;
            else if (type === 'warning') iconSvg = WorkbenchIcons.alertTriangle;
            else if (type === 'error') iconSvg = WorkbenchIcons.alertCircle;
            else iconSvg = WorkbenchIcons.info;
        }

        toast.innerHTML = `
            <div class="toast-icon">${iconSvg}</div>
            <div class="toast-content">
                ${title ? `<div class="toast-title">${this.escapeHtml(title)}</div>` : ''}
                ${message ? `<div class="toast-message">${this.escapeHtml(message)}</div>` : ''}
            </div>
            <button type="button" class="toast-close" aria-label="Close notification">
                ${window.WorkbenchIcons ? WorkbenchIcons.close : '×'}
            </button>
        `;

        const closeBtn = toast.querySelector('.toast-close');
        let dismissTimer = null;

        const dismiss = () => {
            if (dismissTimer) clearTimeout(dismissTimer);
            toast.style.opacity = '0';
            toast.style.transform = 'translateY(8px)';
            setTimeout(() => {
                if (toast.parentNode) {
                    toast.parentNode.removeChild(toast);
                }
            }, 150);
        };

        if (closeBtn) {
            closeBtn.addEventListener('click', dismiss);
        }

        if (durationMs > 0) {
            dismissTimer = setTimeout(dismiss, durationMs);
        }

        container.appendChild(toast);
        return toast;
    },

    info(title, message, durationMs) {
        return this.show('info', title, message, durationMs);
    },

    success(title, message, durationMs) {
        return this.show('success', title, message, durationMs);
    },

    warning(title, message, durationMs) {
        return this.show('warning', title, message, durationMs);
    },

    error(title, message, durationMs) {
        return this.show('error', title, message, durationMs);
    },

    escapeHtml(str) {
        if (!str) return '';
        return String(str)
            .replace(/&/g, '&amp;')
            .replace(/</g, '&lt;')
            .replace(/>/g, '&gt;')
            .replace(/"/g, '&quot;')
            .replace(/'/g, '&#039;');
    }
};

const ComponentsManager = {
    init() {
        // 1. Wire dismissible inline alert buttons
        document.querySelectorAll('.alert-dismiss').forEach(btn => {
            btn.addEventListener('click', () => {
                const alertEl = btn.closest('.alert');
                if (alertEl) {
                    alertEl.style.opacity = '0';
                    setTimeout(() => {
                        if (alertEl.parentNode) alertEl.parentNode.removeChild(alertEl);
                    }, 150);
                }
            });
        });

        // 2. Prevent accidental form submit reloads across all forms
        document.querySelectorAll('form').forEach(form => {
            form.addEventListener('submit', (e) => {
                e.preventDefault();
            });
        });

        // 3. Prevent accidental submit on enter in standard form inputs
        document.querySelectorAll('input:not([type="submit"]):not([type="button"])').forEach(input => {
            input.addEventListener('keydown', (e) => {
                if (e.key === 'Enter') {
                    e.preventDefault();
                }
            });
        });
    }
};

// Auto-initialize when DOM is ready
document.addEventListener('DOMContentLoaded', () => {
    ComponentsManager.init();
});
