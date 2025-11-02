if (RUN_CPP) {
    const list_tex = document.querySelectorAll(".text");
    list_tex.forEach(element => {
        element.innerHTML = CPPLangText(element.innerHTML);
        element.textContent = element.innerHTML;
    });
}