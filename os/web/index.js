// @ts-check
// @filename: types.d.ts
// @filename: asyncfetchfs.js

var Module = typeof Module !== "undefined" ? Module : {};
Module["canvas"] = document.getElementById("canvas");

Module["setStatus"] = (msg) => msg && console.info(msg);

if (!Module["preRun"]) Module["preRun"] = [];

async function doBackgroundFilesPreload(archiveUrl) {
    const preloadFilesTar = await fetchArrayBufProgress(
        archiveUrl,
        true,
        () => {}
    );

    let buf = new Uint8Array(preloadFilesTar);
    console.info(`Preload archive downloaded size=${buf.length}`);

    const started = new Date();
    let totalCount = 0;
    let tooLateCount = 0;
    let giveABreakCounter = 0;
    while (1) {
        const [tarFile, rest] = tarReadFile(buf);
        if (!tarFile) {
            break;
        }

        giveABreakCounter += buf.length - rest.length;
        buf = rest;

        let fPath = tarFile.path;
        let fData = tarFile.data;

        if (!fData){
            continue
        };

        if (fPath.endsWith(".gz") && fData[0] == 0x1f && fData[1] == 0x8b) {
            // Ooh, packed again
            fData = pako.inflate(fData);
            fPath = fPath.slice(0, -3);
        }

        let lookup;
        try {
            lookup = FS.lookupPath(`/app/` + fPath);
        } catch (e) {
            console.warn(`File ${fPath} is not found`, e);
            continue;
        }

        const node = lookup.node;
        if ("contents" in node) {
            if (!node.contents) {
                if (node.size !== fData.length) {
                    throw new Error(`File ${fPath} size differs`);
                }
                node.contents = fData;
            } else {
                tooLateCount += 1;
            }
            totalCount++;
        } else {
            console.warn(`File ${fPath} have no contents, it is on asyncfetchfs?`);
        }

        if (giveABreakCounter > 1000000) {
            giveABreakCounter = 0;
            await new Promise((resolve) => setTimeout(resolve, 1));
        }
    }
    console.info(
        `Preload done, preloaded ${totalCount} files (late ${tooLateCount})` +
            `in ${(new Date().getTime() - started.getTime()) / 1000}s`
    );
}

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
                    /** @type {null | Uint8Array} */
                    contents: null,
                };
            });

        setStatusText("Loading pre-loaded files");

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

        doBackgroundFilesPreload("./preloadfiles.tar.gz").catch((e) => {
            console.warn(e);
            setStatusText(`Preloading files error: ${e.name} ${e.message}`);
        });
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
    console.warn("aborted!", what);
};

Module["onExit"] = (code) => {
    console.info(`Exited with code ${code}`);
    setStatusText(`Exited with code ${code}`);
    document.exitPointerLock();
    document.exitFullscreen().catch((e) => {});
    if (code === 0){
        window.location.href = './menu.html'
    }
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
        console.warn(e);
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
