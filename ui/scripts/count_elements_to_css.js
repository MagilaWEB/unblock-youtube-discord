const root_style = document.documentElement.style;
function updateCountStartStopButtonToCss()
{
    const list_buttons_start = document.querySelectorAll(".buttons_start .button.show");
    const list_buttons_stop = document.querySelectorAll(".buttons_stop .button.show");
    root_style.setProperty("--js-buttons-start-count", list_buttons_start.length);
    root_style.setProperty("--js-buttons-stop-count", list_buttons_stop.length);
}