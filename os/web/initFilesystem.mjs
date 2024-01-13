import { ASYNCFETCHFS } from "./asyncfetchfs.mjs";
import { configuration } from "./config.mjs";
import { GAMES_CACHE_PREFIX, GAME_PATH } from "./consts.mjs";
import { createFetcher } from "./fetcher.mjs";
import { setStatusText } from "./setStatusText.mjs";

/**
 *
 * @param {string} folderName
 * @param {import("./fetcher.mjs").FileTransformer} fileTransformer
 */
export async function initFilesystem(folderName, fileTransformer) {
    setStatusText("Fetching files index");

    const fetcher = createFetcher(
        GAME_PATH + folderName + "/",
        // @TODO: Use some function which will build this
        // @TODO: Add version and remove caches with other versions
        GAMES_CACHE_PREFIX + "____" + folderName,
        configuration.useGzip,
        setStatusText,
        fileTransformer
    );

    const indexUnpacked = await fetcher("index.txt");

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

    FS.mount(
        ASYNCFETCHFS,
        {
            files: filesIndex,
            options: {
                fetcher,
            },
        },
        "/" + folderName
    );

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

    {
        const originalSyncfs = IDBFS.syncfs;
        IDBFS.syncfs = (
            /** @type {any} */ mount,
            /** @type {any} */ populate,
            /** @type {any} */ callback
        ) => {
            originalSyncfs(mount, populate, () => {
                if (!navigator.storage || !navigator.storage.persist) {
                    callback();
                    return;
                }
                navigator.storage
                    .persist()
                    .catch((e) => console.warn(e))
                    .then(() => callback());
            });
        };
    }

    // To save UDBFS into indexeddb do this:
    // IDBFS.syncfs(FS.lookupPath('/app/data/SAVEGAME').node.mount, false, () => console.info('saved'))
}
