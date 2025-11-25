// Description pop-up window
function showDescriptionWindow(element, description) {
	const IsValidElement = event => {
		if ((!event.srcElement) || (!event.srcElement.parentElement))
			return false;

		if (event.srcElement === element)
			return true;

		return event.srcElement.parentElement === element;
	}

	element.addEventListener("mousemove", event => {
		if (IsValidElement(event)) {
			const description_style = description.currentStyle || window.getComputedStyle(description);
			const margin_left = parseInt(description_style.marginLeft);

			const margin_bottom = parseInt(description_style.marginBottom);
			const indent_px = 5;

			const width_difference = document.body.clientWidth - description.clientWidth - margin_left - indent_px;
			const left_block_difference = width_difference - event.clientX;

			if (left_block_difference > 0)
				description.style.left = event.clientX;
			else
				description.style.left = width_difference;

			const height_difference = document.body.clientHeight - description.clientHeight - margin_bottom - indent_px;
			const bottom_block_difference = event.clientY - height_difference;

			if (bottom_block_difference >= 0)
				description.style.top = height_difference;
			else
				description.style.top = event.clientY;
		}
	});

	element.addEventListener("mouseover", event => {
		if (IsValidElement(event)) {
			description.style.left = event.clientX;
			description.classList.add("description_active");
		}
	});

	element.addEventListener("mouseout", event => {
		if (IsValidElement(event)) {
			description.classList.remove("description_active");
		}
	});
}