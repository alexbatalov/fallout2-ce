export function addRightMouseButtonWorkaround() {
    // The game will act weird if no "mouseup" event received for right button
    // This can happen if pointer is not locked (for example, because user pressed Escape button)
    //   and user performs a right-click on the game field which brings context menu.
    //

    const canvas = document.getElementById("canvas");
    if (!canvas) {
        throw new Error(`Canvas element is not found`);
    }
    const RIGHT_BUTTON_ID = 2;

    let isRightDown = false;

    canvas.addEventListener("mousedown", (event) => {
        if (event.button === RIGHT_BUTTON_ID) {
            isRightDown = true;
            return;
        }
    });
    canvas.addEventListener("mouseup", (event) => {
        if (event.button === RIGHT_BUTTON_ID) {
            isRightDown = false;
            return;
        }
        if (isRightDown && !(event.buttons & RIGHT_BUTTON_ID)) {
            const fakeEvent = new MouseEvent("mouseup", {
                button: RIGHT_BUTTON_ID,
                buttons: 0,

                ctrlKey: false,
                shiftKey: false,
                altKey: false,
                metaKey: false,

                bubbles: true,
                cancelable: true,

                screenX: event.screenX,
                screenY: event.screenY,
                clientX: event.clientX,
                clientY: event.clientY,
                movementX: event.movementX,
                movementY: event.movementY,
            });
            isRightDown = false;

            canvas.dispatchEvent(fakeEvent);
        }
    });
}

export function addBackquoteAsEscape() {
    window.addEventListener("keyup", (e) => {
        if (e.code === "Backquote") {
            for (const eventName of ["keydown", "keyup"]) {
                const fakeEvent = new KeyboardEvent(eventName, {
                    ctrlKey: false,
                    shiftKey: false,
                    altKey: false,
                    metaKey: false,

                    bubbles: true,
                    cancelable: true,

                    code: "Escape",
                    key: "Escape",
                    keyCode: 27,
                    which: 27,
                });
                window.dispatchEvent(fakeEvent);
            }
        }
    });
}

export function addHotkeysForFullscreen() {
    // Emscripten prevents defaults for F11
    window.addEventListener("keyup", (e) => {
        if (
            (e.key === "F11" &&
                !e.ctrlKey &&
                !e.shiftKey &&
                !e.metaKey &&
                !e.altKey) ||
            (e.key === "f" &&
                e.ctrlKey &&
                !e.shiftKey &&
                !e.metaKey &&
                !e.altKey)
        ) {
            document.body.requestFullscreen({
                navigationUI: "hide",
            });
        }
    });
}
