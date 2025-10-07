const list_nav = document.querySelectorAll(".nav li a");
list_nav.forEach(element => {
	if (element.hash == "#home") {
		let hom = document.querySelector(element.hash);
		if (hom)
			hom.classList.add("active");
	}

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
});