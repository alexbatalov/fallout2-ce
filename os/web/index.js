var Module = typeof Module !== "undefined" ? Module : {};
Module["canvas"] = document.getElementById("canvas");

Module["setStatus"] = (msg) => msg && console.info(msg);

if (!Module["preRun"]) Module["preRun"] = [];

Module["preRun"].push(() => {
    addRunDependency("initialize-filesystems");

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

ASYNCFETCHFS.onFetching = (fileName) => {
    const statusTextEl = document.getElementById("status_text");    
    statusTextEl.innerHTML = fileName;
    statusTextEl.style.opacity = fileName ? 1 : 0;
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
    document.exitFullscreen();
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
        document.body.requestFullscreen({
            navigationUI: "hide",
        });
    },
    { once: true }
);
