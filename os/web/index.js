// @ts-check
/// <reference path="types.d.ts" />
/// <reference path="asyncfetchfs.js" />
/// <reference path="tar.js" />
/// <reference path="config.js" />
/// <reference path="consts.js" />

/**
 *
 * @param {Error} err
 */
function setErrorState(err) {
    // @ts-ignore
    document.getElementById(
        "error_text"
    ).innerText = `\n\n${err.message}\n${err.stack}`;
}

// Error.stackTraceLimit = 10000;

window.addEventListener("error", (err) => {
    console.info("error", err);
    setErrorState(err.error);
});
window.addEventListener("unhandledrejection", (err) => {
    console.info("unhandledrejection", err);
});

var Module = typeof Module !== "undefined" ? Module : {};
Module["canvas"] = document.getElementById("canvas");

Module["setStatus"] = (msg) => msg && console.info("setStatus", msg);

if (!Module["preRun"]) Module["preRun"] = [];
if (!Module["preInit"]) Module["preInit"] = [];

Module["preInit"].push(() => {
    addRunDependency("initialize-filesystems");
});

/**
 *
 * @param {string} folderName
 * @param {FileTransformer} fileTransformer
 */
async function initFilesystem(folderName, fileTransformer) {
    setStatusText("Fetching files index");

    const indexRawData = await fetch(
        GAME_PATH +
            folderName +
            "/index.txt" +
            (configuration.useGzip ? ".gz" : "")
    ).then((x) => x.arrayBuffer());
    const indexUnpacked = configuration.useGzip
        ? pako.inflate(new Uint8Array(indexRawData))
        : new Uint8Array(indexRawData);
    const indexRaw = new TextDecoder("windows-1251").decode(indexUnpacked);

    const filesIndex = indexRaw
        .split("\n")
        .map((x) => x.trim())
        .filter((x) => x)
        .map((line) => {
            const m = line.match(/^(\d+)\s+(.{64})\s+(.+)$/);
            if (!m) {
                throw new Error(`Wrong line ${line}`);
            }
            const sizeStr = m[1];
            const sha256hash = m[2];
            const fName = m[3];

            return {
                name: fName.startsWith("./") ? fName.slice(2) : fName,
                size: parseInt(sizeStr),
                sha256hash,
                /** @type {null | Uint8Array} */
                contents: null,
            };
        });

    setStatusText("Mounting file systems");

    FS.chdir("/");

    FS.mkdir(folderName);

    /** @type { AsyncFetchFsConfig} */
    const asyncFetchFsConfig = {
        files: filesIndex,
        options: {
            pathPrefix: GAME_PATH + folderName + "/",
            useGzip: configuration.useGzip,
            onFetching: setStatusText,
            fileTransformer,
        },
    };
    FS.mount(ASYNCFETCHFS, asyncFetchFsConfig, "/" + folderName);

    FS.mount(IDBFS, {}, "/" + folderName + "/data/SAVEGAME");

    FS.mount(MEMFS, {}, "/" + folderName + "/data/MAPS");
    FS.mount(MEMFS, {}, "/" + folderName + "/data/proto/items");
    FS.mount(MEMFS, {}, "/" + folderName + "/data/proto/critters");

    FS.chdir("/" + folderName);

    await new Promise((resolve) => {
        // The FS.syncfs do not understand nested mounts so we need to find mount node directly
        IDBFS.syncfs(
            FS.lookupPath("/" + folderName + "/data/SAVEGAME").node.mount,
            true,
            () => {
                resolve(null);
            }
        );
    });

    // To save do this:
    // IDBFS.syncfs(FS.lookupPath('/app/data/SAVEGAME').node.mount, false, () => console.info('saved'))
}

Module["onRuntimeInitialized"] = () => {};

/**
 * @param {string | null} text
 */
function setStatusText(text) {
    const statusTextEl = document.getElementById("status_text");
    if (!statusTextEl) {
        throw new Error(`Element not found`);
    }
    statusTextEl.innerHTML = text || "";
    statusTextEl.style.opacity = `${text ? 1 : 0}`;
}

Module["onAbort"] = (what) => {
    console.warn("aborted!", what);
};

Module["onExit"] = (code) => {
    console.info(`Exited with code ${code}`);
    document.exitPointerLock();
    document.exitFullscreen().catch((e) => {});
    if (code === 0) {
        setStatusText(`Exited with code ${code}`);
        window.location.reload();
    } else {
        setErrorState(new Error(`Exited with code ${code}`));
    }
};

function resizeCanvas() {
    const canvas = /** @type {HTMLCanvasElement | null} */ (
        document.getElementById("canvas")
    );
    if (!canvas) {
        throw new Error(`Canvas element is not found`);
    }
    if (!canvas.parentElement) {
        throw new Error(`Canvas element have no parent`);
    }
    let parentWidth = canvas.parentElement.clientWidth;
    let parentHeight = canvas.parentElement.clientHeight;

    if (parentWidth / parentHeight > canvas.width / canvas.height) {
        parentWidth = (parentHeight * canvas.width) / canvas.height;
    } else {
        parentHeight = parentWidth / (canvas.width / canvas.height);
    }
    canvas.style.width = `${parentWidth}px`;
    canvas.style.height = `${parentHeight}px`;
}
resizeCanvas();
window.addEventListener("resize", resizeCanvas);

setStatusText("Loading emscripten");

Module["instantiateWasm"] = (info, receiveInstance) => {
    (async () => {
        setStatusText("Loading WASM binary");

        const arrayBuffer = await fetchArrayBufProgress(
            wasmBinaryFile,
            false,
            (loaded, total) =>
                setStatusText(
                    `WASM binary loading ${Math.floor((loaded / total) * 100)}%`
                )
        );

        // await new Promise(r => setTimeout(r, 10000));

        setStatusText("Instantiating WebAssembly");
        const inst = await WebAssembly.instantiate(arrayBuffer, info);
        setStatusText("");
        receiveInstance(inst.instance, inst.module);
    })().catch((e) => {
        console.warn(e);
        setErrorState(e);
    });
};

function addRightMouseButtonWorkaround() {
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
addRightMouseButtonWorkaround();

function addBackquoteAsEscape() {
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
addBackquoteAsEscape();

function addHotkeysForFullscreen() {
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
addHotkeysForFullscreen();
