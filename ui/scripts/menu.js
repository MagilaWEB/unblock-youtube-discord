const list_nav = document.querySelectorAll(".nav li a");
list_nav.forEach(element => {
	eventClick(element);
});

function eventClick(element) {
	element.addEventListener("click", (ev) => {
		// Prevent the real fragment navigation: WebView2 reports it as
		// NavigationStarting, saucer then marks the DOM as not-loaded and no
		// DOMContentLoaded ever restores it (same document) — every later
		// C++->JS call would queue forever with no error. Tab switching and
		// URL cosmetics are handled manually below.
		if (ev) ev.preventDefault();
		const hash = document.querySelector(element.hash);
		if (hash) {
			list_nav.forEach(element => {
				let hash_element = document.querySelector(element.hash);
				if (hash_element)
					hash_element.classList.remove("active");
			});

			hash.classList.add("active");

			let active_teg_a = document.querySelector("#active");
			if (active_teg_a)
				active_teg_a.removeAttribute("id");

			element.id = "active";
			try {
				if (element.hash) history.pushState(null, "", element.hash);
			} catch (_) {}
			setTimeout(() => {
				const rect = hash.getBoundingClientRect();
				const scrollTop = window.scrollY || window.pageYOffset;
				const targetY = rect.top + scrollTop - 30;
				window.scrollTo({ top: targetY, behavior: 'smooth' });
			}, 0);
		}
	})
}