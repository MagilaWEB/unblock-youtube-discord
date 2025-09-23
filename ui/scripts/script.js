let list_nav = document.querySelectorAll(".nav li a");
console.log(list_nav);
list_nav.forEach((element) => {
    if (element.hash != "#home") {
        let hom = document.querySelector(element.hash);
        if (hom)
            hom.style.display = "none";
    }

    element.addEventListener("click", () => {
        let hash = document.querySelector(element.hash);
        if (hash) {
            list_nav.forEach((element) => {
                let hash_element = document.querySelector(element.hash);
                if (hash_element)
                    hash_element.style.display = "none";
            });

            let active_teg_a = document.querySelector("#active");
            if(active_teg_a)
                active_teg_a.removeAttribute("id");

            hash.removeAttribute("style");

            element.id = "active";
        }

    })
    console.dir(element);
});