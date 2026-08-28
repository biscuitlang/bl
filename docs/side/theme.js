(() => {
	const THEME_KEY = "bl-theme";
	const LIGHT_MEDIA = window.matchMedia && window.matchMedia("(prefers-color-scheme: light)");
	const root = document.documentElement;

	function storedTheme() {
		try {
			return localStorage.getItem(THEME_KEY);
		} catch (_) {
			return null;
		}
	}

	function preferredTheme() {
		return LIGHT_MEDIA && LIGHT_MEDIA.matches ? "light" : "dark";
	}

	function labelFor(theme) {
		return theme === "light" ? "Dark" : "Light";
	}

	function apply(theme) {
		root.setAttribute("data-theme", theme);
		const button = document.getElementById("bl-theme-toggle");
		if (button) {
			const target = labelFor(theme);
			button.title = "Switch to " + target + " theme";
		}
	}

	apply(storedTheme() || preferredTheme());

	if (LIGHT_MEDIA) {
		LIGHT_MEDIA.addEventListener("change", () => {
			if (!storedTheme()) apply(preferredTheme());
		});
	}

	document.addEventListener("DOMContentLoaded", () => {
		const button = document.getElementById("bl-theme-toggle");
		if (!button) return;

		apply(root.getAttribute("data-theme"));

		button.addEventListener("click", () => {
			const next = root.getAttribute("data-theme") === "dark" ? "light" : "dark";
			try {
				localStorage.setItem(THEME_KEY, next);
			} catch (_) {}
			apply(next);
		});
	});
})();