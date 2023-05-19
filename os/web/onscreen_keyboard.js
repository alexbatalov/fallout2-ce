// @ts-check

/**
 * @param {number} keyCode
 * @param {"keyup"|"keydown"} eventName
 */
function _sendKeyEvent(keyCode, eventName) {
    const fakeEvent = new KeyboardEvent(eventName, {
        ctrlKey: false,
        shiftKey: false,
        altKey: false,
        metaKey: false,

        bubbles: true,
        cancelable: true,

        // SDL do not cares about those two
        code: undefined,
        key: undefined,
        keyCode: keyCode,
        which: keyCode,
    });
    window.dispatchEvent(fakeEvent);
}

function _sendKey(keyCode) {
    _sendKeyEvent(keyCode, "keydown");
    _sendKeyEvent(keyCode, "keyup");
}

// shift = 16

window.addEventListener("keydown", (e) =>
    console.info("keydown", e.code, e.key, e.keyCode, e.which)
);

const _BUTTON_SIZE = 50;
const _keyboardStyles = `
.keyboard_button {
    margin: 4px;
    border-radius: 4px;
    background-color: white;
    color: black;
    font-family: monospace;
    width: ${_BUTTON_SIZE}px;
    height: ${_BUTTON_SIZE}px;
    line-height: ${_BUTTON_SIZE}px;
    text-align: center;
}
.keyboard_row {
    display: flex;
    margin: 10px;
}
.keyboard {
    position: fixed;
    opacity: 0.9;
    background-color: #cfcfcf;
    padding: 10px;
}
`;

function _addKeyCallback(parentEl, kText, callback) {
    const el = document.createElement("div");
    el.className = "keyboard_button";
    el.innerHTML = kText;
    parentEl.appendChild(el);
    el.onclick = callback;
    return el;
}
function _addKey(parentEl, kText, keyCode) {
    return _addKeyCallback(parentEl, kText, () => _sendKey(keyCode));
}

let _keyboardShiftPressed = false;
function _addShiftKey(parentEl) {
    const className = "keyboard_shift_button";
    const el = _addKeyCallback(parentEl, "⇑", () => {
        if (_keyboardShiftPressed) {
            _keyboardShiftPressed = false;
            _sendKeyEvent(16, "keyup");
        } else {
            _keyboardShiftPressed = true;
            _sendKeyEvent(16, "keydown");
        }
        document.querySelectorAll("." + className).forEach((elem) => {
            // @ts-ignore
            elem.style.backgroundColor = _keyboardShiftPressed ? "#ededed" : "";
        });
    });
    el.className = (el.className || "") + " keyboard_shift_button";
    return el;
}

function _createKeyboardElement() {
    const _keyboardStyleSheet = document.createElement("style");
    _keyboardStyleSheet.innerText = _keyboardStyles;
    document.head.appendChild(_keyboardStyleSheet);

    const div = document.createElement("div");
    div.style.left = "0px";
    div.style.top = "200px";
    div.className = "keyboard";

    for (const rowId of [0, 1, 2, 3]) {
        const keysRows = [
            ["QWERTYUIOP".split(""), [81, 87, 69, 82, 84, 89, 85, 73, 79, 80]],
            ["ASDFGHJKL".split(""), [65, 83, 68, 70, 71, 72, 74, 75, 76]],
            ["ZXCVBNM,.".split(""), [90, 88, 67, 86, 66, 78, 77, 188, 190]],
        ];
        const keyRow = keysRows[rowId];

        if (!keyRow) {
            continue;
        }

        const rowDiv = document.createElement("div");
        rowDiv.className = "keyboard_row";

        if (rowId === 1) {
            rowDiv.style.paddingLeft = `${_BUTTON_SIZE / 2}px`;
        } else if (rowId === 2) {
            _addShiftKey(rowDiv).style.width = `${_BUTTON_SIZE * 1.4}px`;
        }

        for (let i = 0; i < keyRow[0].length; i++) {
            _addKey(rowDiv, keyRow[0][i], keyRow[1][i]);
        }
        if (rowId === 0) {
            _addKey(rowDiv, "←", 8).style.width = `${_BUTTON_SIZE * 1.2}px`;
        } else if (rowId === 1) {
            _addKey(rowDiv, "⏎", 13).style.width = `${_BUTTON_SIZE * 1.6}px`;
        } else if (rowId === 2) {
            _addShiftKey(rowDiv).style.width = `${_BUTTON_SIZE * 1.4}px`;
        }

        div.appendChild(rowDiv);
    }

    document.body.appendChild(div);
}
_createKeyboardElement();

function isTouchDevice() {
    return (
        !!(
            typeof window !== "undefined" &&
            ("ontouchstart" in window ||
                // @ts-ignore
                (window.DocumentTouch &&
                    typeof document !== "undefined" &&
                    // @ts-ignore
                    document instanceof window.DocumentTouch))
        ) ||
        !!(
            typeof navigator !== "undefined" &&
            // @ts-ignore
            (navigator.maxTouchPoints || navigator.msMaxTouchPoints)
        )
    );
}

function startTextInput() {
    // TODO
}
function stopTextInput() {
    // TODO
}
