let array_select = [];

class SELECT {
    constructor(_name, _div, _label, _select) {
        this.name = _name;
        this.div = _div;
        this.label = _label;
        this.select = _select;
        this.option_selected = null;
        this.option_index = 1;
        this.array_option = [];
        this.option_size = 0;

        this.select_mouseover = false;
        this.select_active = false;
        this.label.addEventListener("click", () => {
            this.label.focus();

            if (!this.select_active) {
                this.select.classList.add("select_active");
                this.select_active = true;
            }
        });

        this.label.addEventListener("blur", () => {
            if (this.select_mouseover)
                return;

            this.select.classList.remove("select_active");
            this.select_active = false;
        });

        this.select.addEventListener("mouseover", () => {
            this.select_mouseover = true;
        });

        this.select.addEventListener("mouseout", () => {
            this.select_mouseover = false;
        });

        if (!RUN_CPP) {
            this.array_callbacks = [];
            this.callback_size = 0;
        }
    }

    show() {
        this.div.classList.add("show");
    }

    hide() {
        this.div.classList.remove("show");
    }

    addOption(_value, _title, _selected) {
        const option = document.createElement("div");
        option.classList.add("option");
        option.append(_title);
        option.value = _value;

        option.addEventListener("click", () => {
            if (!this.select_active || !this.select_mouseover)
                return;

            this.select.classList.remove("select_active");

            this.select.classList.remove("select_active");
            this.select_active = false;
            this.label.removeChild(this.label.firstChild);
            this.label.append(option.innerHTML);

            this.option_index = option.value;
            this.option_selected = option;

            this.eventChange();

            this.select_active = false;
        });

        if (this.option_size === 0) {
            this.option_selected = option;

            if (this.label.firstChild)
                this.label.removeChild(this.label.firstChild);

            this.label.append(_title);
        }

        if (_selected) {
            this.label.removeChild(this.label.firstChild);
            this.label.append(_title);

            this.option_selected = option;
            this.select.appendChild(option);
            this.array_option[this.option_index = ++this.option_size] = option;
            return
        }

        this.select.appendChild(option);
        this.array_option[++this.option_size] = option;
    }

    eventChange() {
        if (RUN_CPP)
            CPPSelectEventChange(this.name, this.option_index, this.option_selected.innerHTML);
        else {
            this.array_callbacks = this.array_callbacks.filter(fn => {
                if (fn(this.option_index, this.option_selected.innerHTML)) {
                    this.callback_size--;
                    return false;
                }
                return true;
            })
        }
    }

    addEventChange(_function) {
        if (!RUN_CPP)
            this.array_callbacks[++this.callback_size] = _function;
    }

    setValue(_value) {
        if (this.option_size === 0) {
            console.error(_name, "It is impossible to get the value of the selected option from the Select menu, the options have not been created!")
            return false;
        }

        if (typeof _value === "number") {
            this.option_selected = this.array_option[_value];
            this.option_index = this.option_selected.value;

            if (this.label.firstChild)
                this.label.removeChild(this.label.firstChild);
            this.label.append(this.option_selected.innerHTML);

            return true;
        }

        this.array_option.forEach((option, index) => {
            if (option.innerHTML == _value) {
                this.option_selected = option;
                this.option_index = index;

                if (this.label.firstChild)
                    this.label.removeChild(this.label.firstChild);
                this.label.append(this.option_selected.innerHTML);
            }
        });

        return true;
    }

    getValue() {
        if (this.option_size === 0) {
            console.error(_name, "It is impossible to get the value of the selected option from the Select menu, the options have not been created!")
            return undefined;
        }

        if (!this.option_selected)
            return this.array_option[1].innerHTML;

        return this.option_selected.innerHTML;
    }

    clear() {
        this.array_option.forEach(option => {
            option.remove();
        });

        this.array_option = [];
        this.option_size = 0;
    }
}

function getSelect(_name) {
    const select = array_select[_name];
    if (select === undefined) {
        console.error("Couldn't find an element with that name:", _name, "Make sure that the element exists.")
        return undefined;
    }

    return select;
}

/**
 * Sets the option based on the passed value.
 * @param {*} _name It is a unique element name, in fact, a kind of identifier, it can be any name, it is necessary for convenient management of the element in JS and C++.
 * @param {*} _value The value by which the option will be selected, if it is incorrect, an error will occur.
 * @returns Returns false if an error occurs, and true if successful.
 */
function setSelectSelectedOption(_name, _value) {
    const select = getSelect(_name)
    if (select === undefined)
        return false;

    return select.setValue(_value);
}

/**
 * Returns the value of the selected option.
 * @param {*} _name It is a unique element name, in fact, a kind of identifier, it can be any name, it is necessary for convenient management of the element in JS and C++.
 * @returns Returns the value of the selected option, if the error is then "undefined".
 */
function getSelectSelectedOption(_name) {
    const select = getSelect(_name)
    if (select === undefined)
        return undefined;

    return select.getValue();
}

/**
 * Registers the option selection event.
 * @param {*} _name It is a unique element name, in fact, a kind of identifier, it can be any name, it is necessary for convenient management of the element in JS and C++.
 * @param {*} _function A callback function that will be called when an option is selected.
 * @returns Returns false if an error occurs, and true if successful.
 */
function addSelectEventChange(_name, _function) {
    const select = getSelect(_name)
    if (select === undefined)
        return false;

    select.addEventChange(_function);

    return true;
}

/**
 * Creates an option inside the "select option".
 * @param {*} _name It is a unique element name, in fact, a kind of identifier, it can be any name, it is necessary for convenient management of the element in JS and C++.
 * @param {*} _value The value of the option.
 * @param {*} _title Title text.
 * @param {*} _selected Whether this option is selected by default is bool[true/false].
 * @returns Returns false if an error occurs, and true if successful.
 */
function createSelectOption(_name, _value, _title, _selected) {
    const select = getSelect(_name)
    if (select === undefined)
        return false;

    select.addOption(_value, _title, _selected);

    return true;
}

/**
 * 
 * @param {*} _selector The selector of the element to add the created element to.
 * @param {*} _name It is a unique element name, in fact, a kind of identifier, it can be any name, it is necessary for convenient management of the element in JS and C++.
 * @param {*} _title Title text.
 * @param {*} _description Description text.
 * @param {*} _first The element at the beginning of the block.
 * @returns Returns false if an error occurs, and true if successful.
 */
function createListSelect(_selector, _name, _title, _description, _first) {
    const element = document.querySelector(_selector);

    if (!element) {
        console.error("Couldn't find the selector:", _selector, "to add a select inside it.");
        return false;
    }

    if (array_button[_name] !== undefined) {
        console.error("Element:", _name, "it already exists, create a select with a different name.");
        return false;
    }

    const div = document.createElement("div");
    div.classList.add("select_list");
    div.classList.add("show");

    if (_first)
        element.insertBefore(div, element.firstChild);
    else
        element.appendChild(div);

    const label = document.createElement("div");
    label.setAttribute("tabindex", "0");
    label.classList.add("label");
    div.appendChild(label);

    const select = document.createElement("div");
    select.setAttribute("id", _name);
    select.classList.add("select");
    div.appendChild(select);

    const p_title = document.createElement("p");
    p_title.append(_title);
    p_title.classList.add("title");
    div.appendChild(p_title);

    const p_description = document.createElement("p");
    p_description.append(_description);
    p_description.classList.add("description");
    div.appendChild(p_description);

    showDescriptionWindow(div, p_description);

    array_select[_name] = new SELECT(_name, div, label, select);

    return true;
}

/**
 * Deletes all options.
 * @param {*} _name It is a unique element name, in fact, a kind of identifier, it can be any name, it is necessary for convenient management of the element in JS and C++.
 * @returns Returns false if an error occurs, and true if successful.
 */
function clearSelect(_name) {
    const select = getSelect(_name)
    if (select === undefined)
        return false;

    select.clear();
    return true;
}

/**
 * Complete removal of an element.
 * @param {*} _name It is a unique element name, in fact, a kind of identifier, it can be any name, it is necessary for convenient management of the element in JS and C++.
 * @returns Returns false if an error occurs, and true if successful.
 */
function removeListSelect(_name) {
    const select = getSelect(_name)
    if (select === undefined)
        return false;

    select.div.remove();
    array_select[_name] = undefined;

    return true;
}

/**
 * Shows the element. When creating the element, it has the "show" status by default.
 * @param {*} _name It is a unique element name, in fact, a kind of identifier, it can be any name, it is necessary for convenient management of the element in JS and C++.
 * @returns Returns false if an error occurs, and true if successful.
 */
function showListSelect(_name) {
    const select = getSelect(_name)
    if (select === undefined)
        return false;

    select.show();
    return true;
}

/**
 * Hides the element. the function deletes the status of the "show" element.
 * @param {*} _name It is a unique element name, in fact, a kind of identifier, it can be any name, it is necessary for convenient management of the element in JS and C++.
 * @returns Returns false if an error occurs, and true if successful.
 */
function hideListSelect(_name) {
    const select = getSelect(_name)
    if (select === undefined)
        return false;

    select.hide();
    return true;
}