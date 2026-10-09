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

        // About tab: show the app version injected by C++ (window.VERSION_APP).
        const version = document.getElementById("about_version_value");
        if (version && window.VERSION_APP)
            version.textContent = "v" + window.VERSION_APP;
    })();
}