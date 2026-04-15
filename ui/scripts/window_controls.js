const minimizeBtn = document.getElementById('minimizeBtn');
const maximizeBtn = document.getElementById('maximizeBtn');
const closeBtn = document.getElementById('closeBtn');

if (minimizeBtn) {
    minimizeBtn.addEventListener('click', () => {
        if (WINDOW_MINIMIZE) {
            WINDOW_MINIMIZE();
        } else {
            console.warn("window.ultralightMinimize is not defined.");
        }
    });
}

if (maximizeBtn) {
    let maximized = false;
    maximizeBtn.addEventListener('click', () => {
        if (WINDOW_MAXIMIZE) {
            if (!maximized) {
                maximized = true;
                maximizeBtn.textContent = '\u2750'; // ?
                maximizeBtn.classList.add('maximized');
                WINDOW_MAXIMIZE();
            } else if(WINDOW_RESTORE) {
                maximized = false;
                maximizeBtn.textContent = '\u25A1'; // ?
                maximizeBtn.classList.remove('maximized');
                WINDOW_RESTORE();
            }
        } else {
            console.warn("window.ultralightMaximize is not defined.");
        }
    });
}

if (closeBtn) {
    closeBtn.addEventListener('click', () => {
        if (WINDOW_CLOSE) {
            WINDOW_CLOSE();
        } else {
            console.warn("window.ultralightClose is not defined.");
        }
    });
}

const windowControls = document.querySelector('.window-controls');
let isDragging = false;

if (windowControls) {
    window.addEventListener('mousedown', (e) => {
        if (e.target === windowControls || e.target.closest('.text_version')) {
            isDragging = true;
            if (START_MOVE_WINDOW) {
                START_MOVE_WINDOW();
            } else {
                console.warn('MOVE_WINDOW not available');
            }
            e.preventDefault();
        }
    });

    window.addEventListener('mousemove', (e) => {
        if (!isDragging) return;
       if (e.target === windowControls || e.target.closest('.text_version')) {
            if (MOVE_WINDOW) {
                MOVE_WINDOW();
            } else {
                console.warn('MOVE_WINDOW not available');
            }
        }
        else
            isDragging = false;
    });

    windowControls.addEventListener('mouseup', () => {
        isDragging = false;
    });
}