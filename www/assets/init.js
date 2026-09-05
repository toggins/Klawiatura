// sdl3 intercepts all keyboard events even if they coincide with the browser's keyboard shortcuts.
// here we remedy this behavior for (in my opinion) the most common of these shortcuts, and allow them to trigger properly.
window.addEventListener('keydown', function (event) {
    const shortcuts = [
        { key: 'I', ctrl: true, shift: true },
        { key: 'i', ctrl: true, shift: true },
        { key: 'C', ctrl: true, shift: true },
        { key: 'c', ctrl: true, shift: true },
        { key: 'F12' },
        { key: 'F5' }, // refresh page
        { key: 'F11' }, // fullscreen
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
