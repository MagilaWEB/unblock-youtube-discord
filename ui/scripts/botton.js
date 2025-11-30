let array_button = [];

/**
 * Get button element.
 * @param {*} _name It is a unique element name, in fact, a kind of identifier, it can be any name, it is necessary for convenient management of the element in JS and C++.
 * @returns returns the div button element. In case of an error, it returns undefined.
 */
function getButton(_name)
{
	const button = array_button[_name];
	if(button === undefined)
	{
		console.error("Couldn't find an element with that name:", _name, "Make sure that the element exists.")
		return undefined;
	}

	return button;
}

/**
 * Registers a callback function to be called when the button is pressed.
 * @param {*} _name It is a unique element name, in fact, a kind of identifier, it can be any name, it is necessary for convenient management of the element in JS and C++.
 * @param {*} _function A callback function, a button-pressing event.
 * @returns Returns false if an error occurs, and true if successful.
 */
function addButtonEventClick(_name, _function)
{
	const button = getButton(_name);

	if(button === undefined)
		return false;

	const a = button.firstChild;
	if(a)
	{
		if(RUN_CPP)
		{
			const JSButtonEventClick = () =>
			{
				if(CPPButtonEventClick(_name))
					a.removeEventListener("click", JSButtonEventClick)
			};

			a.addEventListener("click", JSButtonEventClick);
		}
		else
		{
			if(typeof _function !== "function")
			{
				console.error("The _function parameter is not a function || name:", _name)
				return false;
			}
			a.addEventListener("click", _function);
		}
	}

	return true;
}

/**
 * Creating a button.
 * @param {*} _selector The selector of the element to add the created element to.
 * @param {*} _name It is a unique element name, in fact, a kind of identifier, it can be any name, it is necessary for convenient management of the element in JS and C++.
 * @param {*} _title Button text.
 * @param {*} _first The element at the beginning of the block.
 * @returns Returns false if an error occurs, and true if successful.
 */
function createButton(_selector, _name, _title, _first) {
	const element = document.querySelector(_selector);

	if (!element) {
		console.error("Couldn't find the selector:", _selector, "to add a button inside it.");
		return false;
	}

	if (array_button[_name] !== undefined) {
		console.error("Element:", _name, "it already exists, create a button with a different name.");
		return false;
	}

	const div = document.createElement("div");
	div.id = _name;
	div.classList.add("button");
	div.classList.add("show");

	if(_first)
		element.insertBefore(div, element.firstChild);
	else
		element.appendChild(div);

	const button = document.createElement("button");
	button.append(_title);
	div.appendChild(button);

	array_button[_name] = div;
	return true;
}

/**
 * Deletes the button permanently.
 * @param {*} _name It is a unique element name, in fact, a kind of identifier, it can be any name, it is necessary for convenient management of the element in JS and C++.
 * @returns Returns false if an error occurs, and true if successful.
 */
function removeButton(_name)
{
	const button = getButton(_name);

	if(button === undefined)
		return false;

	array_button[_name] = undefined;
	button.remove();
	return true;
}

/**
 * Shows the element. When creating the element, it has the "show" status by default.
 * @param {*} _name It is a unique element name, in fact, a kind of identifier, it can be any name, it is necessary for convenient management of the element in JS and C++.
 * @returns Returns false if an error occurs, and true if successful.
 */
function showButton(_name)
{
	const button = getButton(_name);

	if(button === undefined)
		return false;

	button.classList.add("show");
	return true;
}

/**
 * Hides the element. the function deletes the status of the "show" element.
 * @param {*} _name It is a unique element name, in fact, a kind of identifier, it can be any name, it is necessary for convenient management of the element in JS and C++.
 * @returns Returns false if an error occurs, and true if successful.
 */
function hideButton(_name)
{
	const button = getButton(_name);

	if(button === undefined)
		return false;

	button.classList.remove("show");
	return true;
}

/**
 * Changes the title text of an element.
 * @param {*} _name It is a unique element name, in fact, a kind of identifier, it can be any name, it is necessary for convenient management of the element in JS and C++.
 * @param {*} _title The new title bar for the element
 * @returns Returns false if an error occurs, and true if successful.
 */
function setTitleButton(_name, _title)
{
	const button = getButton(_name);

	if(button === undefined)
		return false;

	button.firstChild.removeChild(button.firstChild.firstChild);
	button.firstChild.append(_title);
	return true;
}