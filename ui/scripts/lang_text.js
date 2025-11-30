/**
 * Converts the localization id to text from the localization file.
 */

if (RUN_CPP) {
    const list_tex = document.querySelectorAll(".text");
    list_tex.forEach(element => {
        element.innerHTML = CPPLangText(element.innerHTML);
        element.textContent = element.innerHTML;
    });
}