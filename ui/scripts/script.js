
// menu
const list_nav = document.querySelectorAll(".nav li a");
list_nav.forEach((element) => {
    if (element.hash == "#home") {
        let hom = document.querySelector(element.hash);
        if (hom)
            hom.classList.add("active");
    }

    element.addEventListener("click", () => {
        const hash = document.querySelector(element.hash);
        if (hash) {
            list_nav.forEach((element) => {
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

// Checkboxes, description pop-up window
document.querySelectorAll(".check_box").forEach((element) => {
    const description = element.querySelector(".description");

    const IsValidElement = (event) => {
        if ((!event.srcElement) || (!event.srcElement.parentElement))
            return false;

        if (event.srcElement === element)
            return true;

        return event.srcElement.parentElement === element;
    }

    element.onmousemove = (event) => {
        if (IsValidElement(event)) {
            const descriptionStyle = description.currentStyle || window.getComputedStyle(description);
            const marginLeft = parseInt(descriptionStyle.marginLeft);
            let left_block_difference = document.body.clientWidth - description.clientWidth - marginLeft;
            left_block_difference -= event.clientX;

            if (left_block_difference > 0)
                description.style.left = event.clientX;
            else
                description.style.left = document.body.clientWidth - description.clientWidth - marginLeft;

            description.style.top = event.clientY;
        }
    };

    element.onmouseover = (event) => {
        if (IsValidElement(event)) {
            description.style.left = event.clientX;
            description.classList.add("description_active");
        }
    };

    element.onmouseout = (event) => {
        if (IsValidElement(event)) {
            description.classList.remove("description_active");
        }
    }
});