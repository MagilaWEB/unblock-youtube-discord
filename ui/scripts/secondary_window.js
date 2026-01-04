let array_secondary_window = [];

/**
 * @param {*} this.type {0} == button OK, {1} == button yes/no, {2} == info wait;
 */

class SecondaryWindow {
	constructor(_name, _div, _content, _elements) {
		this.name = _name;
		this.base_div = _div;
		this.content_div = _content;
		this.elements_div = _elements;
		this.type = 0;

		this.name_button_ok = _name + "ok";
		this.button_ok = null;

		this.name_button_yes = _name + "yes";
		this.button_yes = null;

		this.name_button_no = _name + "no";
		this.button_no = null;

		this.loader = null;
	}

	createElement() {
		if (this.button_ok)
			this.button_ok.remove();

		if (this.button_yes)
			this.button_yes.remove();

		if (this.button_no)
			this.button_no.remove();

		if (this.loader)
			this.loader.remove();

		switch (this.type) {
			case 0:
				if (createButton(".secondary_window .elements", this.name_button_ok, RUN_CPP ? CPPLangText("str_b_secondary_window_ok") : "OK")) {
					this.button_ok = getButton(this.name_button_ok);
					this.elements_div.appendChild(this.button_ok);
					this.elements_div.classList.add("horizontally")

					this.button_ok.addEventListener("click", () => {
						if (!RUN_CPP)
							this.hide();

						if (RUN_CPP)
							CPPSecondaryWindowEventOK(this.name);
					});
				}
				break;
			case 1:
				if (createButton(".secondary_window .elements", this.name_button_yes, RUN_CPP ? CPPLangText("str_b_secondary_window_yes") : "YES")) {
					this.button_yes = getButton(this.name_button_yes);
					this.elements_div.appendChild(this.button_yes);
					this.elements_div.classList.add("horizontally")

					this.button_yes.addEventListener("click", () => {
						if (!RUN_CPP)
							this.hide();

						if (RUN_CPP)
							CPPSecondaryWindowEventYESNO(this.name, true);
					});
				}
				if (createButton(".secondary_window .elements", this.name_button_no, RUN_CPP ? CPPLangText("str_b_secondary_window_no") : "NO")) {
					this.button_no = getButton(this.name_button_no);
					this.elements_div.appendChild(this.button_no);
					this.elements_div.classList.add("horizontally")

					this.button_no.addEventListener("click", () => {
						if (!RUN_CPP)
							this.hide();

						if (RUN_CPP)
							CPPSecondaryWindowEventYESNO(this.name, false);
					});
				}
				break;

			case 2:
				const loader = document.createElement("div");
				loader.classList.add("anim_loader");
				loader.innerHTML = "<div></div><div></div><div></div><div></div><div></div><div></div><div></div><div></div>";
				this.elements_div.appendChild(loader);
				this.loader = loader;

				if (createButton(".secondary_window .elements", this.name_button_no, RUN_CPP ? CPPLangText("str_b_secondary_window_cancel") : "Cancel")) {
					this.button_no = getButton(this.name_button_no);
					this.elements_div.appendChild(this.button_no);
					this.elements_div.classList.add("vertically")

					this.button_no.addEventListener("click", () => {
						if (!RUN_CPP)
							this.hide();

						if (RUN_CPP)
							CPPSecondaryWindowEventCancel(this.name);
					});
				}

				break;
			default:
				break;
		}
	}

	setType(_type) {
		this.type = _type;
		this.createElement();
	}
	show() {
		document.querySelector(".nav").classList.add("blur");
		document.querySelector(".text_version").classList.add("blur");
		document.querySelector("main").classList.add("blur");
		document.querySelector("footer").classList.add("blur");

		this.is_show = true;
		this.base_div.classList.add("show");
	}

	hide() {
		this.base_div.classList.remove("show");
		this.is_show = false;

		document.querySelector(".nav").classList.remove("blur");
		document.querySelector(".text_version").classList.remove("blur");
		document.querySelector("main").classList.remove("blur");
		document.querySelector("footer").classList.remove("blur");
	}
}

function getSecondaryWindow(_name) {
	const secondary_window = array_secondary_window[_name];
	if (secondary_window === undefined) {
		console.error("Couldn't find an element with that name:", _name, "Make sure that the element exists.")
		return undefined;
	}

	return secondary_window;
}


function setTypeSecondaryWindow(_name, _type) {
	const secondary_window = getSecondaryWindow(_name);
	if (secondary_window === undefined)
		return false;

	secondary_window.setType(_type);

	return true;
}

function showSecondaryWindow(_name) {
	const secondary_window = getSecondaryWindow(_name);
	if (secondary_window === undefined)
		return false;

	secondary_window.show();

	return true;
}


function hideSecondaryWindow(_name) {
	const secondary_window = getSecondaryWindow(_name);
	if (secondary_window === undefined)
		return false;

	secondary_window.hide();

	return true;
}

function createSecondaryWindow(_name, _title, _description) {
	const element = document.querySelector("body");

	if (!element) {
		console.error("Couldn't find the selector:", _selector, "to add a button inside it.");
		return false;
	}

	if (array_secondary_window[_name] !== undefined) {
		console.error("Element:", _name, "it already exists, create a button with a different name.");
		return false;
	}

	const div = document.createElement("div");
	div.id = _name;
	div.classList.add("secondary_window");

	element.appendChild(div);

	const content = document.createElement("div");
	content.classList.add("content");
	div.appendChild(content);

	const title = document.createElement("h2");
	title.classList.add("title");
	title.append(_title);
	content.appendChild(title);

	const description = document.createElement("p");
	description.classList.add("description");
	description.append(_description);
	content.appendChild(description);

	const elements = document.createElement("div");
	elements.classList.add("elements");
	content.appendChild(elements);

	array_secondary_window[_name] = new SecondaryWindow(_name, div, content, elements);
	return true;
}

function removeSecondaryWindow(_name) {
	const secondary_window = getSecondaryWindow(_name);
	if (secondary_window === undefined)
		return false;

	secondary_window.base_div.remove();

	array_secondary_window[_name] = undefined;

	return true;
}

function setTitleSecondaryWindow(_name, _title) {
	const secondary_window = getSecondaryWindow(_name);
	if (secondary_window === undefined)
		return false;

	const title = secondary_window.content_div.querySelector(".title");
	title.removeChild(title.firstChild);
	title.append(_title);

	return true;
}

function setDescriptionSecondaryWindow(_name, _description) {
	const secondary_window = getSecondaryWindow(_name);
	if (secondary_window === undefined)
		return false;

	const description = secondary_window.content_div.querySelector(".description");
	description.removeChild(description.firstChild);
	description.append(_description);
	return true;
}