let array_select = [];

class SELECT {
    constructor(_name, _div, _select) {
        this.name = _name;
        this.div = _div;
        this.select = _select;
        this.option_selected = null;
        this.array_option = [];
        this.option_size = 0;
    }

    addOption(_value, _title, _selected) {
        const option = document.createElement("option");
        option.append(_title);
        option.setAttribute("value", _value);

        if (_selected) {
            if (this.option_selected) {
                console.warn(this.name, "Please note that option has already been selected as the default!");
                this.option_selected.removeAttribute("selected");
            }
            option.setAttribute("selected", "true");
            this.option_selected = option;
        }

        this.select.appendChild(option);

        this.array_option[++this.option_size] = option;
    }

    addEventChange(_function) {
        if (RUN_CPP) {
            const JSSelectEventChange = () => {
                this.option_selected = this.array_option[this.select.selectedIndex];
                if (CPPSelectEventChange(this.name, this.select.value))
                    this.select.removeEventListener("change", JSSelectEventChange)
            };

            this.select.addEventListener("change", JSSelectEventChange);
        }
        else {
            this.select.addEventListener("change", () => {
                this.option_selected = this.array_option[this.select.selectedIndex];
                _function(this.select.value);
            });
        }

    }

    setValue(_value) {
        if (this.option_size === 0) {
            console.error(_name, "It is impossible to get the value of the selected option from the Select menu, the options have not been created!")
            return false;
        }

        if (typeof _value === "number") {
            this.select.selectedIndex = _value;
            this.select.value = _value;
            this.option_selected = this.array_option[_value];
            return true;
        }

        this.array_option.forEach((option, index) => {
            option.removeAttribute("selected");

            if (option.value == _value) {
                option.setAttribute("selected", "true");
                this.select.selectedIndex = index;
                this.select.value = _value;
                this.option_selected = option;
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
            return this.array_option[1].value;

        return this.option_selected.value;
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

function setSelectSelectedOption(_name, _value) {
    const select = getSelect(_name)
    if (select === undefined)
        return false;

    return select.setValue(_value);
}

function getSelectSelectedOption(_name) {
    const select = getSelect(_name)
    if (select === undefined)
        return undefined;

    return select.getValue();
}

function addSelectEventChange(_name, _function) {
    const select = getSelect(_name)
    if (select === undefined)
        return false;

    select.addEventChange(_function);

    return true;
}

function createSelectOption(_name, _value, _title, _selected) {
    const select = getSelect(_name)
    if (select === undefined)
        return false;

    select.addOption(_value, _title, _selected);

    return true;
}

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

    if (_first)
        element.insertBefore(div, element.firstChild);
    else
        element.appendChild(div);

    const select = document.createElement("select");
    select.setAttribute("id", _name);
    select.setAttribute("name", _name);
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

    array_select[_name] = new SELECT(_name, div, select);

    return true;
}

function clearSelect(_name) {
    const select = getSelect(_name)
    if (select === undefined)
        return false;

    select.clear();
    return true;
}

function removeListSelect(_name) {
    const select = getSelect(_name)
    if (select === undefined)
        return false;

    select.div.remove();
    array_select[_name] = undefined;

    return true;
}