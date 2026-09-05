// sdl3 intercepts all keyboard events even if they coincide with the browser's "open dev console" shortcut.
// here we remedy this behavior for the most common of the browser-console shortcuts, and allow it to open normally.
window.addEventListener('keydown', function (event) {
    const shortcuts = [
        { key: 'I', ctrl: true, shift: true },
        { key: 'i', ctrl: true, shift: true },
        { key: 'C', ctrl: true, shift: true },
        { key: 'c', ctrl: true, shift: true },
        { key: 'F12' },
    ];

    for (const shortcut of shortcuts) {
        const ctrl = !("ctrl" in shortcut) || shortcut.ctrl && event.ctrlKey;
        const shift = !("shift" in shortcut) || shortcut.shift && event.shiftKey;
        const key = shortcut.key === event.key;

        if (ctrl && shift && key) {
            event.stopImmediatePropagation();
            return;
        }
    }
}, true);
