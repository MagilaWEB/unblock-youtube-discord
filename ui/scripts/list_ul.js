let array_list_ul = []; //  array objects ul text.

class UL {
	constructor(_name, _object_div) {
		this.name = _name;
		this.object_div = _object_div;
		this.object_ul = _object_div.querySelector("ul");
		this.array_list_li = [];
		this.li_size = 0;
	}

	show() {
		this.object_div.classList.add("show");
	}

	hide() {
		this.object_div.classList.remove("show");
	}

	addLi(_text, _state) {
		const li = document.createElement("li");

		if (_state !== undefined) {
			if (_state)
				li.classList.add("li_success");
			else
				li.classList.add("li_fail");
		}

		this.object_ul.appendChild(li);

		const p = document.createElement("p");
		p.append(_text);

		li.appendChild(p);

		this.li_size++;

		this.array_list_li[this.li_size] = li;
	}

	getLi(index) {
		if (index > this.li_size) {
			console.error(this.name, "couldn't find the li element by index:", index, "it exceeds the current li_size value:", this.li_size)
			return undefined;
		}

		const li = this.array_list_li[index];

		if (li === undefined) {
			console.error(this.name, "ul has the li element by index:", index, "it may have been deleted.")
			return undefined;
		}

		return li;
	}

	addClass(_name_class) {
		this.object_div.classList.add(_name_class);
	}

	removeClass(_name_class) {
		this.object_div.classList.remove(_name_class);
	}

	clear() {
		this.array_list_li.forEach(element => {
			element.remove();
		})
		this.array_list_li = [];
		this.li_size = 0;
	}

	delete() {
		this.clear();
		this.object_div.remove();
	}
}

/**
 * 
 * @param {*} _name It is a unique element name, in fact, a kind of identifier, it can be any name, it is necessary for convenient management of the element in JS and C++.
 * @returns Returns the ul object for managing the ul element and its child elements of the list li from the DOM tree, in case of an "undefined" error.
 */
function getListUl(_name) {
	const ul_class = array_list_ul[_name];

	if (ul_class === undefined) {
		console.error("The element with the name:", _name, "does not exist, it is not possible to create a li tag.");
		return undefined;
	}

	return ul_class;
}

function addClassListUl(_name, _name_class) {
	const ul_class = getListUl(_name);
	if (ul_class === undefined)
		return false;

	ul_class.addClass(_name_class);
	return true;
}

function removeClassListUl(_name, _name_class) {
	const ul_class = getListUl(_name);
	if (ul_class === undefined)
		return false;

	ul_class.removeClass(_name_class);
	return true;
}

/**
 * Creates a new list item without a specific status.
 * @param {*} _name It is a unique element name, in fact, a kind of identifier, it can be any name, it is necessary for convenient management of the element in JS and C++.
 * @param {*} _text Title text.
 * @returns Returns false if an error occurs, and true if successful.
 */
function createListUlLiAdd(_name, _text) {
	const ul_class = getListUl(_name);

	if (ul_class === undefined)
		return false;

	ul_class.addLi(_text);
	return true;
}

/**
 * Creates a li element with the success status (true success false fail).
 * @param {*} _name It is a unique element name, in fact, a kind of identifier, it can be any name, it is necessary for convenient management of the element in JS and C++.
 * @param {*} _text Title text.
 * @param {*} state true success false fail.
 * @returns Returns false if an error occurs, and true if successful.
 */
function createListUlLiAddSuccess(_name, _text, state) {
	const ul_class = getListUl(_name);

	if (ul_class === undefined)
		return false;

	ul_class.addLi(_text, state);

	return true;
}

/**
 * Creates a base Ul element for the list.
 * @param {*} _selector The selector of the element to add the created element to.
 * @param {*} _name It is a unique element name, in fact, a kind of identifier, it can be any name, it is necessary for convenient management of the element in JS and C++.
 * @param {*} _first The element at the beginning of the block.
 * @returns Returns false if an error occurs, and true if successful.
 */
function createListUl(_selector, _name, _title, _first) {
	const element = document.querySelector(_selector);

	if (!element) {
		console.error("Couldn't find the selector:", _selector, "to add a list ul inside it.");
		return false;
	}

	if (array_list_ul[_name] !== undefined) {
		console.error("The element with the name:", _name, "already exists.");
		return false;
	}

	const div = document.createElement("div");
	div.classList.add("block");
	div.classList.add("show");

	if (_first)
		element.insertBefore(div, element.firstChild);
	else
		element.appendChild(div);

	const h2 = document.createElement("h2");
	h2.append(_title);
	div.appendChild(h2);

	const ul = document.createElement("ul");
	div.appendChild(ul);

	array_list_ul[_name] = new UL(_name, div);

	return true;
}

/**
 * Removes all the li element from the ul element.
 * @param {*} _name It is a unique element name, in fact, a kind of identifier, it can be any name, it is necessary for convenient management of the element in JS and C++.
 * @returns Returns false if an error occurs, and true if successful.
 */
function clearListUl(_name) {
	const ul_class = getListUl(_name);

	if (ul_class === undefined)
		return false;

	ul_class.clear();

	return true;
}

/**
 * Completely removes the ul element with child elements.
 * @param {*} _name It is a unique element name, in fact, a kind of identifier, it can be any name, it is necessary for convenient management of the element in JS and C++.
 * @returns Returns false if an error occurs, and true if successful.
 */
function removeListUl(_name) {
	const ul_class = getListUl(_name);
	if (ul_class === undefined)
		return false;

	ul_class.delete();
	array_list_ul[_name] = undefined;
	return true;
}

/**
 * Shows the element. When creating the element, it has the "show" status by default.
 * @param {*} _name It is a unique element name, in fact, a kind of identifier, it can be any name, it is necessary for convenient management of the element in JS and C++.
 * @returns Returns false if an error occurs, and true if successful.
 */
function showListUl(_name) {
	const ul_class = getListUl(_name);
	if (ul_class === undefined)
		return false;

	ul_class.show();
	return true;
}

/**
 * Hides the element. the function deletes the status of the "show" element.
 * @param {*} _name It is a unique element name, in fact, a kind of identifier, it can be any name, it is necessary for convenient management of the element in JS and C++.
 * @returns Returns false if an error occurs, and true if successful.
 */
function hideListUl(_name) {
	const ul_class = getListUl(_name);
	if (ul_class === undefined)
		return false;

	ul_class.hide();
	return true;
}