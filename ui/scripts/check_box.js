let array_check_box = []; //  array objects check box.

/**
 * Get check_box element.
 * @param {*} _name It is a unique element name, in fact, a kind of identifier, it can be any name, it is necessary for convenient management of the element in JS and C++.
 * @returns returns the div check_box element. In case of an error, it returns undefined.
 */
function getCheckBox(_name) {
	const check_box = array_check_box[_name];
	if (check_box === undefined) {
		console.error("Couldn't find an element with that name:", _name, "Make sure that the element exists.");
		return undefined;
	}

	return check_box;
}

/**
 * Extracts the checkbox input element from the DOM tree.
 * @param {*} _name It is a unique element name, in fact, a kind of identifier, it can be any name, it is necessary for convenient management of the element in JS and C++.
 * @returns Returns undefined in case of an error, otherwise an element from the DOM tree.
 */
function getCheckBoxInput(_name) {
	const check_box = getCheckBox(_name);
	if (check_box === undefined)
		return undefined;

	const check = check_box.firstChild;
	if (check) {
		const input = check.firstChild;
		if (input)
			return input;
	}

	return undefined;
}

// Check the status of the checkbox
/**
 * Returns the flag status, if it is set, then the value is true.
 * @param {*} _name It is a unique element name, in fact, a kind of identifier, it can be any name, it is necessary for convenient management of the element in JS and C++.
 * @returns Even in case of an error, it returns false.
 */
function getCheckBoxState(_name) {
	let input = getCheckBoxInput(_name);
	if (input !== undefined)
		return !!input.checked;

	return false;
}

/**
 * 
 * @param {*} _name It is a unique element name, in fact, a kind of identifier, it can be any name, it is necessary for convenient management of the element in JS and C++.
 * @param {*} _state Set the status of the checkbox forcibly (true or false).
 */
function setCheckBoxState(_name, _state) {
	let input = getCheckBoxInput(_name);
	if (input !== undefined)
		input.checked = _state;
}

// The event of clicking the checkbox
/**
 * Registers the Check event at the checkbox, triggered when clicking on the checkbox.
 * @param {*} _name It is a unique element name, in fact, a kind of identifier, it can be any name, it is necessary for convenient management of the element in JS and C++.
 * @param {*} _function A callback function, a check box status change event.
 */
function addCheckBoxEventCheck(_name, _function) {

	if(_function !== undefined)
	{
		if (typeof _function !== "function") {
			console.error("The _function parameter is not a function || name:", _name);
			return;
		}
	}

	let input = getCheckBoxInput(_name);
	if (input !== undefined) {
		if(_function !== undefined)
		{
			input.addEventListener("change", () => {
				_function(!!input.checked)
			});
		}
		else
		{
			const JsCheckBoxEventClick = () =>
			{
				if(CPPCheckBoxEventClick(_name, !!input.checked))
					a.removeEventListener("click", JsCheckBoxEventClick)
			};

			input.addEventListener("change", JsCheckBoxEventClick);
		}
	}
}

/**
 * Creates a checkbox element in the specified selector.
 * @param {*} _selector The selector of the element to add the created element to.
 * @param {*} _name It is a unique element name, in fact, a kind of identifier, it can be any name, it is necessary for convenient management of the element in JS and C++.
 * @param {*} _title Title text.
 * @param {*} _description Description text.
 * @param {*} _first The element at the beginning of the block.
 * @returns Returns false if an error occurs, and true if successful.
 */
function createCheckBox(_selector, _name, _title, _description, _first) {
	const element = document.querySelector(_selector);

	if (!element) {
		console.error("Couldn't find the selector:", _selector, "to add a checkbox inside it.");
		return false;
	}

	if (array_check_box[_name] !== undefined) {
		console.error("Element:", _name, "it already exists, create a checkbox with a different name.");
		return false;
	}

	const div = document.createElement("div");
	div.classList.add("check_box");
	div.classList.add("show");
	
	if(_first)
		element.insertBefore(div, element.firstChild);
	else
		element.appendChild(div);

	const div_check = document.createElement("div");
	div_check.classList.add("check");
	div.appendChild(div_check);

	const input = document.createElement("input");
	input.setAttribute("type", "checkbox");
	input.setAttribute("id", _name);
	input.setAttribute("name", "ckeck");
	div_check.appendChild(input);

	const label = document.createElement("label");
	label.setAttribute("for", _name);
	div_check.appendChild(label);

	const p_title = document.createElement("p");
	p_title.append(_title);
	p_title.classList.add("title");
	div.appendChild(p_title);

	const p_description = document.createElement("p");
	p_description.append(_description);
	p_description.classList.add("description");
	div.appendChild(p_description);

	showDescriptionWindow(div, p_description);

	array_check_box[_name] = div;

	return true;
}

/**
 * Deletes the ñheckBox permanently.
 * @param {*} _name It is a unique element name, in fact, a kind of identifier, it can be any name, it is necessary for convenient management of the element in JS and C++.
 * @returns Returns false if an error occurs, and true if successful.
 */
function removeCheckBox(_name) {
	const check_box = getCheckBox(_name);
	if (check_box === undefined)
		return false;

	check_box.remove();
	array_check_box[_name] = undefined;
	return true
}

/**
 * Shows the element. When creating the element, it has the "show" status by default.
 * @param {*} _name It is a unique element name, in fact, a kind of identifier, it can be any name, it is necessary for convenient management of the element in JS and C++.
 * @returns Returns false if an error occurs, and true if successful.
 */
function showCheckBox(_name)
{
	const check_box = getCheckBox(_name);
	if (check_box === undefined)
		return false;

	check_box.classList.add("show");
	return true;
}

/**
 * Hides the element. the function deletes the status of the "show" element.
 * @param {*} _name It is a unique element name, in fact, a kind of identifier, it can be any name, it is necessary for convenient management of the element in JS and C++.
 * @returns Returns false if an error occurs, and true if successful.
 */
function hideCheckBox(_name)
{
	const check_box = getCheckBox(_name);
	if (check_box === undefined)
		return false;

	check_box.classList.remove("show");
	return true;
}