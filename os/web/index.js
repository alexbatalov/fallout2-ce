// @ts-check
// @filename: types.d.ts
// @filename: asyncfetchfs.js

var Module = typeof Module !== "undefined" ? Module : {};
Module["canvas"] = document.getElementById("canvas");

Module["setStatus"] = (msg) => msg && console.info(msg);

if (!Module["preRun"]) Module["preRun"] = [];

Module["preRun"].push(() => {
    addRunDependency("initialize-filesystems");
    setStatusText("Initializing filesystem");

    (async () => {
        setStatusText("Fetching index");

        const indexRaw = await fetch("./index.txt").then((x) => x.text());

        const filesIndex = indexRaw
            .split("\n")
            .map((x) => x.trim())
            .filter((x) => x)
            .map((line) => {
                const [sizeStr, fname] = line.split("\t");
                return {
                    name: fname,
                    size: parseInt(sizeStr),
                };
            });

        FS.mkdir("app");

        FS.mount(
            ASYNCFETCHFS,
            {
                files: filesIndex,
                pathPrefix: "game/",
                useGzip: true,
                onFetching: setStatusText,
            },
            "/app"
        );

        FS.mount(IDBFS, {}, "/app/data/SAVEGAME");

        FS.mount(MEMFS, {}, "/app/data/MAPS");
        FS.mount(MEMFS, {}, "/app/data/proto/items");
        FS.mount(MEMFS, {}, "/app/data/proto/critters");

        FS.chdir("/app");

        await new Promise((resolve) => {
            // The FS.syncfs do not understand nested mounts so we need to find mount node directly
            IDBFS.syncfs(
                FS.lookupPath("/app/data/SAVEGAME").node.mount,
                true,
                () => {
                    resolve(null);
                }
            );
        });

        setStatusText("Starting");

        removeRunDependency("initialize-filesystems");
    })().catch((e) => {
        setStatusText(`Error ${e.name} ${e.message}`);
    });

    // To save do this:
    // IDBFS.syncfs(FS.lookupPath('/app/data/SAVEGAME').node.mount, false, () => console.info('saved'))
});

Module["onRuntimeInitialized"] = () => {};

function setStatusText(text) {
    const statusTextEl = document.getElementById("status_text");
    if (!statusTextEl) {
        throw new Error(`Element not found`);
    }
    statusTextEl.innerHTML = text;
    statusTextEl.style.opacity = `${text ? 1 : 0}`;
}

Module["onAbort"] = (what) => {
    console.info("aborted!", what);
};

Module["onExit"] = (code) => {
    console.info(`Exited with code ${code}`);
    setStatusText(`Exited with code ${code}`);
    document.exitPointerLock();
    document.exitFullscreen().catch((e) => {});
};

function resizeCanvas() {
    const canvas = document.getElementById("canvas");
    if (!canvas) {
        throw new Error(`Canvas element is not found`);
    }
    if (!canvas.parentElement) {
        throw new Error(`Canvas element have no parent`);
    }
    let width = canvas.parentElement.clientWidth;
    let height = canvas.parentElement.clientHeight;
    if (width / height > 640 / 480) {
        width = (height * 640) / 480;
    } else {
        height = width / (640 / 480);
    }
    canvas.style.width = `${width}px`;
    canvas.style.height = `${height}px`;
}
resizeCanvas();
window.addEventListener("resize", resizeCanvas);

document.body.addEventListener(
    "click",
    () => {
        // TODO Enable me later
        // document.body.requestFullscreen({
        //     navigationUI: "hide",
        // });
    },
    { once: true }
);

window.addEventListener("error", (errevent) => {
    const error = errevent.error;
    if (!error) {
        return;
    }
    setStatusText(`${error.name} ${error.message}`);
});
window.addEventListener("unhandledrejection", (err) => {
    setStatusText(err.reason);
});

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

        setStatusText("Instantiating WebAssembly");
        const inst = await WebAssembly.instantiate(arrayBuffer, info);
        setStatusText("Waiting for dependencies");
        receiveInstance(inst.instance, inst.module);
    })().catch((e) => {
        console.info(e);
        setStatusText(`Error: ${e.name} ${e.message}`);
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
