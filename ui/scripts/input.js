let array_input = [];
class INPUT {
	constructor(_name, _object_input, _type, _value, _title) {
		this.name = _name;
		this.object_input_div = _object_input;
		this.object_input = _object_input.firstChild;
		this.type = _type;
		this.value = _value;
		this.title = _title;

		// validator ip
		if (this.type == "ip") {
			this.ipRE = /^(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$/;
		}

		if (!RUN_CPP) {
			this.array_callback_submit = [];
			this.size_callback = 0;
		}
	}

	show() {
		this.object_input_div.classList.add("show");
	}

	hide() {
		this.object_input_div.classList.remove("show");
	}

	blur() {
		this.object_input.value = "";
		this.object_input.placeholder = this.title + ": " + this.value;
	}

	_blur() {
		this.object_input.addEventListener("blur", () => this.blur());
	}

	_focus() {
		this.object_input.value = this.value;
		this.object_input.addEventListener("focus", () => {
			this.object_input.value = this.value;
		});
	}

	_keyup() {
		this.object_input.addEventListener("keyup", event => {
			event.preventDefault();
			if (event.keyCode === 13) // enter
			{
				if (this.type == "ip") {
					if (!this.ipRE.test(this.object_input.value)) {
						this.object_input.value = this.value;
						return;
					}
				}

				if(this.type == "number")
				{
					const trimmed = this.object_input.value.trim();
					if(!trimmed)
					{
						this.object_input.value = this.value;
						return;
					}

					const num = parseInt(trimmed, 10);
					if((isNaN(num)) && num == trimmed || num < Number.MIN_SAFE_INTEGER || num > Number.MAX_SAFE_INTEGER)
					{
						this.object_input.value = this.value;
						return;
					}
				}

				this.value = this.object_input.value;
				this.object_input.blur();

				if (RUN_CPP)
					CPPInputEventSubmit(this.name, this.value);
				else {
					this.array_callback_submit = this.array_callback_submit.filter(json_data => {
						if (json_data.func(this.value) === true)
							json_data.remove = true;

						if (json_data.remove) {
							this.size_callback--;
							return false;
						}
						return true;
					})
				}
			}
		});
	}

	addCallbackSubmit(_function, _remove) {
		if (RUN_CPP)
			return;

		if (typeof _function !== "function") {
			console.error("The _function parameter is not a function || name:", this.name);
			return;
		}

		this.array_callback_submit[++this.size_callback] = {
			func: _function,
			remove: _remove
		};
	}

	removeCallbackSubmit(_function) {
		if (RUN_CPP)
			return;

		if (typeof _function !== "function") {
			console.error("The _function parameter is not a function || name:", this.name);
			return;
		}

		this.array_callback_submit = this.array_callback_submit.filter(json_data => {
			if (json_data.func === _function) {
				this.size_callback--;
				return false;
			}

			return true;
		});
	}
}

/**
 * Returns an object of the INPUT class.
 * @param {*} _name It is a unique element name, in fact, a kind of identifier, it can be any name, it is necessary for convenient management of the element in JS and C++.
 * @returns Returns an object of the INPUT class, if it does not exist by name, it returns undefined with an error output.
 */
function getInput(_name) {
	const input = array_input[_name];

	if (input === undefined) {
		console.error("Couldn't find an element with that name:", _name, "Make sure that the element exists.")
		return undefined;
	}

	return input;
}

function setInputValue(_name, _new_value) {
	const input = getInput(_name);
	if (input === undefined)
		return false;

	input.value = _new_value;
	input.blur();
	return true;
}

function getInputValue(_name) {
	const input = getInput(_name);

	if (input === undefined)
		return undefined;

	return input.value;
}

/**
 * 
 * @param {*} _name It is a unique element name, in fact, a kind of identifier, it can be any name, it is necessary for convenient management of the element in JS and C++.
 * @param {*} _remove This parameter determines whether to remove the function from the Submit event if true is called 1 time.
 * @param {*} _function The Submit call event function, if it returns true, the remove parameter will be set to true, and the function will fire 1 time.
 * @returns Returns false if an error occurs, and true if successful.
 */
function addInputEventSubmit(_name, _remove, _function) {
	if (RUN_CPP)
		return;

	const input = getInput(_name);

	if (input === undefined)
		return false;

	input.addCallbackSubmit(_function, _remove);

	return true;
}

/**
 * 
 * @param {*} _name It is a unique element name, in fact, a kind of identifier, it can be any name, it is necessary for convenient management of the element in JS and C++.
 * @param {*} _function Event function that needs to be deleted.
 * @returns Returns false if an error occurs, and true if successful.
 */
function removeInputEventSubmit(_name, _function) {
	if (RUN_CPP)
		return;

	const input = getInput(_name);

	if (input === undefined)
		return false;

	input.removeCallbackSubmit(_function);

	return true;
}

/**
 * 
 * @param {*} _selector The selector of the element to add the created element to.
 * @param {*} _name It is a unique element name, in fact, a kind of identifier, it can be any name, it is necessary for convenient management of the element in JS and C++.
 * @param {*} _type Input type as a string.
 * @param {*} _value The basic default value.
 * @param {*} _title Title text.
 * @param {*} _description Description text.
 * @param {*} _first The element at the beginning of the block.
 * @returns Returns false if an error occurs, and true if successful.
 */
function createInput(_selector, _name, _type, _value, _title, _description, _first) {
	const element = document.querySelector(_selector);

	if (!element) {
		console.error("Couldn't find the selector:", _selector, "to add a input inside it.");
		return false;
	}

	if (array_input[_name] !== undefined) {
		console.error("Element:", _name, "it already exists, create a input with a different name.");
		return false;
	}

	const div = document.createElement("div");
	div.classList.add("input");
	div.classList.add("show");

	if (_first)
		element.insertBefore(div, element.firstChild);
	else
		element.appendChild(div);

	const input = document.createElement("input");
	input.classList.add("check");
	if (_type === "ip") {
		input.setAttribute("name", "ip");
		input.setAttribute("type", "text");
		input.setAttribute("minlength", "7");
		input.setAttribute("maxlength", "15");
		input.setAttribute("size", "15");
	}
	else {
		input.setAttribute("name", _type);
		input.setAttribute("type", _type);
	}

	input.setAttribute("id", _name);
	input.setAttribute("placeholder", _title + ": " + _value);
	div.appendChild(input);

	const p_description = document.createElement("p");
	p_description.append(_description);
	p_description.classList.add("description");
	div.appendChild(p_description);

	showDescriptionWindow(div, p_description);

	let obj_input = array_input[_name] = new INPUT(_name, div, _type, _value, _title);

	input.addEventListener("focus", () => {
		obj_input._focus();
		obj_input._blur();
		obj_input._keyup();
	}, { once: true });

	return true;
}

/**
 * Removing element input. 
 * @param {*} _name It is a unique element name, in fact, a kind of identifier, it can be any name, it is necessary for convenient management of the element in JS and C++.
 * @returns Returns false if an error occurs, and true if successful.
 */
function removeInput(_name) {
	const input = getInput(_name);
	if (input === undefined)
		return false;

	input.object_input_div.remove();

	array_input[_name] = undefined;
	return true;
}

/**
 * Shows the element. When creating the element, it has the "show" status by default.
 * @param {*} _name It is a unique element name, in fact, a kind of identifier, it can be any name, it is necessary for convenient management of the element in JS and C++.
 * @returns Returns false if an error occurs, and true if successful.
 */
function showInput(_name) {
	const input = getInput(_name);
	if (input === undefined)
		return false;

	input.show();
	return true;
}

/**
 * Hides the element. the function deletes the status of the "show" element.
 * @param {*} _name It is a unique element name, in fact, a kind of identifier, it can be any name, it is necessary for convenient management of the element in JS and C++.
 * @returns Returns false if an error occurs, and true if successful.
 */
function hideInput(_name) {
	const input = getInput(_name);
	if (input === undefined)
		return false;

	input.hide();
	return true;
}