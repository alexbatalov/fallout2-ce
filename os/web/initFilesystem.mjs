import { ASYNCFETCHFS } from "./asyncfetchfs.mjs";
import { configuration } from "./config.mjs";
import { createFetcher } from "./fetcher.mjs";
import { getCacheName } from "./gamecache.mjs";
import { setStatusText } from "./setStatusText.mjs";

const GAME_PATH = "./game/";

/**
 *
 * @param {Uint8Array} indexUnpackedRaw
 */
export function parseIndex(indexUnpackedRaw) {
    const indexRaw = new TextDecoder("windows-1251").decode(indexUnpackedRaw);

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
    return filesIndex;
}

/**
 * @param {string} folderName
 * @param {string} filesVersion
 */
export async function downloadAllGameFiles(folderName, filesVersion) {
    const fetcher = createFetcher(
        GAME_PATH + folderName + "/",
        getCacheName(folderName, filesVersion),
        configuration.useGzip,
        (text) => {
            if (text) {
                setStatusText(text);
            } else {
                // Do not update status, just a visual fix
                // Do not forget to clear status when this function returns
            }
        },
        null,
        filesVersion
    );

    const indexUnpackedRaw = await fetcher("index.txt");

    const filesIndex = parseIndex(indexUnpackedRaw)
        .filter((file) => file.name !== "index.txt")
        .filter((file) => {
            // This is workaround for Nevada to skip files in cp1251 encoding
            if (file.name.startsWith("!docs/")) {
                return false;
            }

            return true;
        });

    let availableTaskIndex = 0;
    async function worker() {
        while (true) {
            const task = filesIndex[availableTaskIndex];
            if (!task) {
                setStatusText(null);
                return;
            }
            availableTaskIndex++;
            await fetcher(task.name, task.size, task.sha256hash);
        }
    }

    await Promise.all(new Array(5).fill(0).map(() => worker()));

    setStatusText(null);
}

/**
 *
 * @param {string} folderName
 * @param {string} filesVersion
 * @param {import("./fetcher.mjs").FileTransformer} fileTransformer
 */
export async function initFilesystem(
    folderName,
    filesVersion,
    fileTransformer
) {
    setStatusText("Fetching files index");

    const fetcher = createFetcher(
        GAME_PATH + folderName + "/",
        getCacheName(folderName, filesVersion),
        configuration.useGzip,
        setStatusText,
        fileTransformer,
        filesVersion
    );

    const indexUnpackedRaw = await fetcher("index.txt");

    const filesIndex = parseIndex(indexUnpackedRaw);

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
