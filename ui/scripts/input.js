let array_input = [];


class INPUT {
    constructor(_name, _object_input, _value, _title) {
        this.name = _name;
        this.object_input_div = _object_input;
        this.object_input = _object_input.firstChild;
        this.value = _value;
        this.title = _title;
        this.array_callback_sumbit = [];
        this.size_callback = 0;
    }

    _blur() {
        this.object_input.addEventListener("blur", () => {
            //this.value = this.object_input.value;
            this.object_input.value = "";
            this.object_input.placeholder = this.title + ": " + this.value;
        });
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
                this.value = this.object_input.value;
                this.object_input.blur();

                this.array_callback_sumbit = this.array_callback_sumbit.filter(json_data => {
                    if (json_data.func(this.value) === true)
                        json_data.remove = true;

                    if (json_data.remove) {
                        this.size_callback--;
                        return false;
                    }
                    return true;
                })
            }
        });
    }

    addCallbackSumbit(_function, _remove) {
        if(typeof _function !== "function")
		{
			console.error("addCallbackSumbit The _function parameter is not a function || name:", this.name);
			return;
		}

        this.array_callback_sumbit[++this.size_callback] = {
            func: _function,
            remove: _remove
        };
    }

    removeCallbackSumbit(_function) {
        if(typeof _function !== "function")
		{
			console.error("removeCallbackSumbit The _function parameter is not a function || name:", this.name);
			return;
		}

        this.array_callback_sumbit = this.array_callback_sumbit.filter(json_data => {
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
 * @param {*} _name_function_dbg The message in which function the error occurred is necessary for c++.
 * @returns Returns an object of the INPUT class, if it does not exist by name, it returns undefined with an error output.
 */
function getInput(_name, _name_function_dbg) {
    const input = array_input[_name];

    if (_name_function_dbg === undefined)
        _name_function_dbg = "getInput";

    if (input === undefined) {
        console.error(_name_function_dbg, "Couldn't find an element with that name:", _name, "Make sure that the element exists.")
        return undefined;
    }

    return input;
}

/**
 * 
 * @param {*} _name It is a unique element name, in fact, a kind of identifier, it can be any name, it is necessary for convenient management of the element in JS and C++.
 * @param {*} _remove This parameter determines whether to remove the function from the Sumbit event if true is called 1 time.
 * @param {*} _function The Sumbit call event function, if it returns true, the remove parameter will be set to true, and the function will fire 1 time.
 * @returns Returns false if an error occurs, and true if successful.
 */
function addInputEventSumbit(_name, _remove, _function) {
    const input = getInput(_name, "addInputEventSumbit");

    if (input === undefined)
        return false;

    input.addCallbackSumbit(_function, _remove);

    return true;
}

/**
 * 
 * @param {*} _name It is a unique element name, in fact, a kind of identifier, it can be any name, it is necessary for convenient management of the element in JS and C++.
 * @param {*} _function Event function that needs to be deleted.
 * @returns Returns false if an error occurs, and true if successful.
 */
function removeInputEventSumbit(_name, _function) {
    const input = getInput(_name, "removeInputEventSumbit");

    if (input === undefined)
        return false;

    input.removeCallbackSumbit(_function);

    return true;
}

/**
 * 
 * @param {*} _selector The selector of the element to add the created element to.
 * @param {*} _name It is a unique element name, in fact, a kind of identifier, it can be any name, it is necessary for convenient management of the element in JS and C++.
 * @param {*} _type 
 * @param {*} _value 
 * @param {*} _title Title text.
 * @param {*} _description Description text.
 * @returns Returns false if an error occurs, and true if successful.
 */
function createInput(_selector, _name, _type, _value, _title, _description) {
    const element = document.querySelector(_selector);

    if (!element) {
        console.error("createInput Couldn't find the selector:", _selector, "to add a checkbox inside it.");
        return false;
    }

    if (array_input[_name] !== undefined) {
        console.error("createInput Element:", _name, "it already exists, create a checkbox with a different name.");
        return false;
    }

    const div = document.createElement("div");
    div.classList.add("input");
    element.appendChild(div);

    const input = document.createElement("input");
    input.classList.add("check");
    input.setAttribute("name", _type);
    input.setAttribute("type", _type);
    input.setAttribute("id", _name);
    input.setAttribute("placeholder", _title + ": " + _value);
    div.appendChild(input);

    const p_description = document.createElement("p");
    p_description.append(_description);
    p_description.classList.add("description");
    div.appendChild(p_description);

    showDescriptionWindow(div, p_description);

    let obj_input = array_input[_name] = new INPUT(_name, div, _value, _title);

    input.addEventListener("focus", () => {
        obj_input._focus();
        obj_input._blur();
        obj_input._keyup();
    }, { once: true });

    return true;
}