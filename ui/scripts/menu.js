const list_nav = document.querySelectorAll(".nav li a");
list_nav.forEach(element => {
	if (element.hash == "#home") {
		let hom = document.querySelector(element.hash);
		if (hom)
			hom.classList.add("active");
	}

	eventClick(element);
});

const version_app_text = document.querySelector(".text_version .text");
if (version_app_text) {
	if (RUN_CPP) {
		version_app_text.innerHTML = "Version:" + VERSION_APP;
		version_app_text.innerText = version_app_text.innerHTML;
	}
	else {
		const text_demo = "DEMO_TEXT    version:1.3.3"
		version_app_text.innerHTML = text_demo;
		version_app_text.innerText = text_demo;
	}
}

function eventClick(element) {
	element.addEventListener("click", () => {
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
		}
	})
}