let array_botton = [];

/**
 * Registers a callback function to be called when the button is pressed.
 * @param {*} _name It is a unique element name, in fact, a kind of identifier, it can be any name, it is necessary for convenient management of the element in JS and C++.
 * @param {*} _function A callback function, a button-pressing event.
 * @returns Returns false if an error occurs, and true if successful.
 */
function addBottonEventClick(_name, _function)
{
	const botton = array_botton[_name];

	if(botton === undefined)
	{
		console.error("bottonEvent Couldn't find an element with that name:", _name, "Make sure that the element exists.")
		return false;
	}

	const a = botton.firstChild;
	if(a)
	{
		if(typeof _function !== "function")
		{
			console.error("bottonEvent The _function parameter is not a function || name:", _name)
			return false;
		}
		a.addEventListener("click", _function);
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
function createBotton(_selector, _name, _title, _first) {
	const element = document.querySelector(_selector);

	if (!element) {
		console.error("createCheckBox Couldn't find the selector:", _selector, "to add a checkbox inside it.");
		return false;
	}

	if (array_botton[_name] !== undefined) {
		console.error("createCheckBox Element:", _name, "it already exists, create a checkbox with a different name.");
		return false;
	}

	const div = document.createElement("div");
	div.id = _name;
	div.classList.add("button");
	if(_first)
		element.insertBefore(div, element.firstChild);
	else
		element.appendChild(div);

	const button = document.createElement("button");
	button.append(_title);
	div.appendChild(button);

	array_botton[_name] = div;
	return true;
}