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

// window.addEventListener("keydown", (e) =>
//     console.info("keydown", e.code, e.key, e.keyCode, e.which)
// );

const _BUTTON_SIZE = 60;
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

function _addKeyboardStyles() {
    const _keyboardStyleSheet = document.createElement("style");
    _keyboardStyleSheet.innerText = _keyboardStyles;
    document.head.appendChild(_keyboardStyleSheet);
}
_addKeyboardStyles();

function _addKeyCallback(parentEl, kText, callback) {
    const el = document.createElement("div");
    el.className = "keyboard_button";
    el.innerHTML = kText;
    parentEl.appendChild(el);
    el.onclick = (e) => {
        callback();
        e.preventDefault();
        e.stopPropagation();
    };
    return el;
}
function _addKey(parentEl, kText, keyCode) {
    return _addKeyCallback(parentEl, kText, () => _sendKey(keyCode));
}

let _keyboardShiftPressed = false;

function _addShiftKey(parentEl) {
    const _keyboardShiftClassName = "keyboard_shift_button";

    const el = _addKeyCallback(parentEl, "shift", () => {
        if (_keyboardShiftPressed) {
            _keyboardShiftPressed = false;
            _sendKeyEvent(16, "keyup");
        } else {
            _keyboardShiftPressed = true;
            _sendKeyEvent(16, "keydown");
        }
        document
            .querySelectorAll("." + _keyboardShiftClassName)
            .forEach((elem) => {
                // @ts-ignore
                elem.style.backgroundColor = _keyboardShiftPressed
                    ? "#ededed"
                    : "";
            });
    });
    el.className = (el.className || "") + " " + _keyboardShiftClassName;
    return el;
}

const _keyboardMainDivId = "onscreen-keyboard-id";
function _removeKeyboardElement() {
    const el = document.querySelector("#" + _keyboardMainDivId);
    if (el) {
        el.parentElement?.removeChild(el);
    }
}

function _createKeyboardElement() {
    _removeKeyboardElement();

    _keyboardShiftPressed = false;

    const div = document.createElement("div");
    div.className = "keyboard";
    div.id = _keyboardMainDivId;

    for (const rowId of [0, 1, 2, 3]) {
        const rowDiv = document.createElement("div");
        rowDiv.className = "keyboard_row";

        if (rowId === 1) {
            rowDiv.style.paddingLeft = `${_BUTTON_SIZE / 2}px`;
        } else if (rowId === 2) {
            _addShiftKey(rowDiv).style.width = `${_BUTTON_SIZE * 1.4}px`;
        } else if (rowId === 3) {
            rowDiv.style.paddingLeft = `${_BUTTON_SIZE * 4}px`;
        }

        const keysRows = [
            ["QWERTYUIOP".split(""), [81, 87, 69, 82, 84, 89, 85, 73, 79, 80]],
            ["ASDFGHJKL".split(""), [65, 83, 68, 70, 71, 72, 74, 75, 76]],
            ["ZXCVBNM,.".split(""), [90, 88, 67, 86, 66, 78, 77, 188, 190]],
        ];
        const keyRow = keysRows[rowId];

        if (keyRow) {
            for (let i = 0; i < keyRow[0].length; i++) {
                _addKey(rowDiv, keyRow[0][i], keyRow[1][i]);
            }
        }

        if (rowId === 0) {
            _addKey(rowDiv, "<-", 8).style.width = `${_BUTTON_SIZE * 1.2}px`;
        } else if (rowId === 1) {
            _addKey(rowDiv, "enter", 13).style.width = `${_BUTTON_SIZE * 1.6}px`;
        } else if (rowId === 2) {
            _addShiftKey(rowDiv).style.width = `${_BUTTON_SIZE * 1.4}px`;
        } else if (rowId === 3) {
            _addKey(rowDiv, "&nbsp;", 32).style.width = `${_BUTTON_SIZE * 5}px`;
        }

        div.appendChild(rowDiv);
    }
    document.body.appendChild(div);

    let posX = (window.innerWidth - div.clientWidth) / 2;
    let posY = window.innerHeight - div.clientHeight - _BUTTON_SIZE / 2;
    div.style.left = `${posX}px`;
    div.style.top = `${posY}px`;

    let touchX = 0;
    let touchY = 0;
    const onMove = (e) => {
        posX = posX + (e.touches[0].screenX - touchX);
        posY = posY + (e.touches[0].screenY - touchY);
        div.style.left = `${posX}px`;
        div.style.top = `${posY}px`;
        touchX = e.touches[0].screenX;
        touchY = e.touches[0].screenY;
    };
    div.ontouchstart = (e) => {
        touchX = e.touches[0].screenX;
        touchY = e.touches[0].screenY;
        window.addEventListener("touchmove", onMove);
    };
    window.addEventListener("touchend", () => {
        window.removeEventListener("touchmove", onMove);
    });
}

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
    if (isTouchDevice()) {
        _createKeyboardElement();
    }
}
function stopTextInput() {
    _removeKeyboardElement();
}
