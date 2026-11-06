/**
 * UNIFIED ENGINEERING WORKBENCH - THEME MANAGER
 * Handles Light and Dark theme switching with localStorage persistence.
 */

const ThemeManager = {
    STORAGE_KEY: 'workbench-theme',

    init() {
        const savedTheme = localStorage.getItem(this.STORAGE_KEY);
        let theme = 'light';

        if (savedTheme === 'light' || savedTheme === 'dark') {
            theme = savedTheme;
        } else if (window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches) {
            theme = 'dark';
        }

        this.applyTheme(theme);

        const toggleBtn = document.getElementById('theme-toggle-btn');
        if (toggleBtn) {
            toggleBtn.addEventListener('click', () => {
                this.toggle();
            });
        }
    },

    getCurrentTheme() {
        return document.documentElement.getAttribute('data-theme') || 'light';
    },

    applyTheme(theme) {
        document.documentElement.setAttribute('data-theme', theme);
        localStorage.setItem(this.STORAGE_KEY, theme);

        const iconEl = document.getElementById('theme-icon');
        const labelEl = document.getElementById('theme-label');

        if (theme === 'dark') {
            if (iconEl) iconEl.innerHTML = WorkbenchIcons.sun;
            if (labelEl) labelEl.textContent = 'Light Mode';
        } else {
            if (iconEl) iconEl.innerHTML = WorkbenchIcons.moon;
            if (labelEl) labelEl.textContent = 'Dark Mode';
        }

        if (window.ChartEngine && typeof window.ChartEngine.redrawAll === 'function') {
            window.ChartEngine.redrawAll();
        }
    },

    toggle() {
        const nextTheme = this.getCurrentTheme() === 'dark' ? 'light' : 'dark';
        this.applyTheme(nextTheme);
    }
};
