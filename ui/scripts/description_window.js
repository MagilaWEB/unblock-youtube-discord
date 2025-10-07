// Description pop-up window
function showDescriptionWindow(element, description) {
	const IsValidElement = event => {
		if ((!event.srcElement) || (!event.srcElement.parentElement))
			return false;

		if (event.srcElement === element)
			return true;

		return event.srcElement.parentElement === element;
	}

	element.onmousemove = event => {
		if (IsValidElement(event)) {
			const description_style = description.currentStyle || window.getComputedStyle(description);
			const margin_left = parseInt(description_style.marginLeft);

			let left_block_difference = document.body.clientWidth - description.clientWidth - margin_left;
			left_block_difference -= event.clientX;

			if (left_block_difference > 0)
				description.style.left = event.clientX;
			else
				description.style.left = document.body.clientWidth - description.clientWidth - margin_left;

			description.style.top = event.clientY;
		}
	};

	element.onmouseover = event => {
		if (IsValidElement(event)) {
			description.style.left = event.clientX;
			description.classList.add("description_active");
		}
	};

	element.onmouseout = event => {
		if (IsValidElement(event)) {
			description.classList.remove("description_active");
		}
	}
}