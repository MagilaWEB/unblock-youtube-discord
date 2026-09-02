/**
 * Converts the localization id to text from the localization file.
 */

if (RUN_CPP) {
    (async () => {
        const list_tex = document.querySelectorAll(".text");
        for (const element of list_tex) {
            element.innerHTML = await saucer.exposed.CPPLangText(element.innerHTML);
            element.textContent = element.innerHTML;
        }
    })();
}