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
    document.getElementById("status_text").innerHTML = fileName;
};

Module["onAbort"] = (what) => {
    console.info("aborted!", what);
};
