var Module = typeof Module !== "undefined" ? Module : {};
Module["canvas"] = document.getElementById("canvas");

Module["setStatus"] = (msg) => msg && console.info(msg);

if (!Module["preRun"]) Module["preRun"] = [];

Module["preRun"].push(() => {
    addRunDependency("initialize-filesystems");
    setStatusText("Initializing filesystem");
    fetch("./index.txt")
        .then((x) => x.text())
        .then((raw) => {
            const filesIndex = raw
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

            ASYNCFETCHFS.pathPrefix = "game/";

            ASYNCFETCHFS.useGzip = true;

            FS.mount(
                ASYNCFETCHFS,
                {
                    files: filesIndex,
                },
                "/app"
            );

            FS.mount(IDBFS, {}, "/app/data/SAVEGAME");

            FS.mount(MEMFS, {}, "/app/data/MAPS");
            FS.mount(MEMFS, {}, "/app/data/proto/items");
            FS.mount(MEMFS, {}, "/app/data/proto/critters");

            FS.chdir("/app");

            return new Promise((resolve) => {
                // The FS.syncfs do not understand nested mounts so we need to find mount node directly
                IDBFS.syncfs(
                    FS.lookupPath("/app/data/SAVEGAME").node.mount,
                    true,
                    () => {
                        setTimeout(() => {
                            setStatusText("Starting");
                            resolve();
                        }, 1);
                    }
                );
            });
        })
        .then(() => {
            removeRunDependency("initialize-filesystems");
        });

    // To save do this:
    // IDBFS.syncfs(FS.lookupPath('/app/data/SAVEGAME').node.mount, false, () => console.info('saved'))
});

Module["onRuntimeInitialized"] = () => {};

function setStatusText(text) {
    const statusTextEl = document.getElementById("status_text");
    statusTextEl.innerHTML = text;
    statusTextEl.style.opacity = text ? 1 : 0;
}

ASYNCFETCHFS.onFetching = (fileName) => {
    setStatusText(fileName);
};

Module["onAbort"] = (what) => {
    console.info("aborted!", what);
};

Module["onExit"] = (code) => {
    console.info(`Exited with code ${code}`);
    document.getElementById(
        "status_text"
    ).innerHTML = `Exited with code ${code}`;
    document.exitPointerLock();
    document.exitFullscreen().catch((e) => {});
};

function resizeCanvas() {
    const canvas = document.getElementById("canvas");
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

Module["instantiateWasm"] = async (info, receiveInstance) => {
    // const arrBuf = await fetch(wasmBinaryFile).then(x => x.arrayBuffer());
    setStatusText("Loading WASM binary");

    const req = new XMLHttpRequest();
    req.open("GET", wasmBinaryFile, true);
    req.responseType = "arraybuffer";

    req.onload = (event) => {
        console.info(`Done`, event);
        const arrayBuffer = req.response; // Note: not req.responseText

        if (!arrayBuffer) {
            setStatusText("WASM binary loading failed");
            return;
        }

        setStatusText("Instantiating WebAssembly");
        WebAssembly.instantiate(arrayBuffer, info).then((inst) => {
            setStatusText("Waiting for dependencies");
            receiveInstance(inst.instance, inst.module);
        });
    };
    req.onprogress = (event) => {
        const progress = event.loaded / event.total;
        setStatusText(`WASM binary loading ${(Math.floor(progress * 100))}%`);        
    };
    req.onerror = event => {
        console.info(`error`, event);
        setStatusText("WASM binary loading failed");
    }

    req.send();
};
