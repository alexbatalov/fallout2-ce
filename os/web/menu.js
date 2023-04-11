// @ts-check
// @filename: types.d.ts
// @filename: tar.js
// @filename: games.js

/** @returns {Promise<IDBDatabase>} */
async function initDb() {
    const IDBFS_DB_VERSION = 21;

    return new Promise((resolve, reject) => {
        const request = window.indexedDB.open(
            "/app/data/SAVEGAME",
            IDBFS_DB_VERSION
        );
        request.onerror = (event) => {
            reject(request.error);
        };
        request.onsuccess = (event) => {
            const database = request.result;
            resolve(database);
        };
        request.onupgradeneeded = (event) => {
            reject(new Error(`LOL IDBFS version bumped!`));
            throw new Error(`Not implemented`);
        };
    });
}

const IDBFS_STORE_NAME = "FILE_DATA";

/**
 * @typedef { {
 *     mode: number,
 *     timestamp:Date
 *     contents?: Int8Array
 *   } } IdbFileData
 */

/**
 * @param {IDBDatabase} database
 * @returns {Promise<Map<IDBValidKey, IdbFileData>>}
 */
async function readFilesFromDb(database) {
    return await new Promise((resolve, reject) => {
        const objectStore = database
            .transaction(IDBFS_STORE_NAME, "readonly")
            .objectStore(IDBFS_STORE_NAME);
        const cursorReq = objectStore.openCursor();

        const newMap = new Map();
        cursorReq.onsuccess = (event) => {
            const cur = cursorReq.result;
            if (!cur) {
                resolve(newMap);
                return;
            }
            const key = cur.key;
            const value = cur.value;
            newMap.set(key, value);
            cur.continue();
        };
        cursorReq.onerror = (event) => {
            reject(cursorReq.error);
        };
    });
}

/**
 *
 * @param {Uint8Array} buf
 * @param {string} fname
 */
function downloadBuf(buf, fname) {
    const blob = new Blob([buf]);
    const url = URL.createObjectURL(blob);

    const link = document.createElement("a");
    link.style.display = "none";
    document.body.appendChild(link);
    link.href = URL.createObjectURL(blob);
    link.download = fname;
    link.click();
    setTimeout(() => {
        URL.revokeObjectURL(url);
    }, 10);
}

/**
 *
 * @param {Map<IDBValidKey, IdbFileData>} files
 * @param {string} slotId
 */
function downloadSlot(files, slotId) {
    const prefix = `/app/data/SAVEGAME/SLOT${slotId}/`;
    const filesList = [...files.keys()].filter((x) =>
        typeof x === "string" ? x.startsWith(prefix) : false
    );

    /** @type {Uint8Array[]} */
    const tarBlocks = [];
    for (const fName of filesList) {
        if (typeof fName !== "string") {
            continue;
        }
        const entry = files.get(fName);

        tarBlocks.push(
            packTarFile(
                fName.slice(prefix.length),
                entry ? entry.contents : null
            )
        );
    }
    tarBlocks.push(tarEnding);

    const totalTarSize = tarBlocks.reduce((acc, cur) => acc + cur.length, 0);
    const tarBuf = new Uint8Array(new ArrayBuffer(totalTarSize));
    let offset = 0;
    for (const block of tarBlocks) {
        tarBuf.set(block, offset);
        offset += block.length;
    }

    downloadBuf(tarBuf, `slot${slotId}.tar`);
}

/**
 *
 * @param {Map<IDBValidKey, IdbFileData>} files
 */
function renderSlots(files) {
    const container = document.getElementById("slots_container");
    if (!container) {
        throw new Error(`No container`);
    }
    container.innerHTML = "";
    for (let i = 1; i <= 10; i++) {
        const slotId = ("0" + i.toString()).slice(-2);
        const slotDiv = document.createElement("div");
        slotDiv.className = "slot_div";
        container.appendChild(slotDiv);

        const uploadButtonHtml = `<button id="slot_${slotId}_upload">Upload</button>`;

        const saveDat = files.get(`/app/data/SAVEGAME/SLOT${slotId}/SAVE.DAT`);
        if (!saveDat || !saveDat.contents) {
            slotDiv.innerHTML =
                `<div class="slot_status">Slot ${slotId}: [empty]</div>` +
                `<div class="slot_buttons">
                        ${uploadButtonHtml}
                </div>`;
            continue;
        }

        const expectedHeader = "FALLOUT SAVE FILE";
        const observedHeader = String.fromCharCode(
            ...saveDat.contents.slice(0, expectedHeader.length)
        );
        if (expectedHeader !== observedHeader) {
            slotDiv.innerHTML =
                `<div class="slot_status">Slot ${slotId}: Header error</div>` +
                `<div class="slot_buttons">
                    ${uploadButtonHtml}
                </div>`;
            continue;
        }

        const saveName = String.fromCharCode(
            ...saveDat.contents.slice(
                0x3d,
                Math.min(0x3d + 0x1e, saveDat.contents.indexOf(0, 0x3d))
            )
        );

        slotDiv.innerHTML =
            `<div class="slot_status">Slot ${slotId}: '${saveName}'</div>` +
            `<div class="slot_buttons">
                        <button id="slot_${slotId}_download">Download</button>
                        ${uploadButtonHtml}
            </div>`;

        const downloadButton = document.getElementById(
            `slot_${slotId}_download`
        );
        if (!downloadButton) {
            throw new Error(`Internal error`);
        }
        downloadButton.onclick = () => downloadSlot(files, slotId);
    }
}

function setStatus(msg) {
    const el = document.getElementById("status_text");
    if (!el) {
        throw new Error(`No status text div`);
    }
    el.innerHTML = msg;
}

/**
 *
 * @returns { Promise<File> }
 */
async function pickFile() {
    // showOpenFilePicker
    return new Promise((resolve, reject) => {
        const input = document.createElement("input");
        input.type = "file";
        input.accept = ".tar,.tar.gz";
        input.onchange = (e) => {
            const file = input.files ? input.files[0] : null;
            if (!file) {
                reject("No file selected");
                return;
            }
            resolve(file);
        };
        input.click();
    });
}
/**
 *
 * @param {IDBDatabase} database
 * @param {string} slotId
 */
async function uploadSavegame(database, slotId) {
    setStatus(`Pick a file...`);
    const file = await pickFile();

    setStatus(`Loading file...`);
    const url = URL.createObjectURL(file);
    const raw = await fetch(url).then((x) => x.arrayBuffer());
    URL.revokeObjectURL(url);

    const tar = file.name.endsWith(".gz")
        ? pako.inflate(new Uint8Array(raw))
        : new Uint8Array(raw);

    setStatus(`Removing old saving...`);

    const files = await readFilesFromDb(database);
    const prefix = `/app/data/SAVEGAME/SLOT${slotId}/`;
    for (const fileToRemove of [...files.keys()].filter((x) =>
        typeof x === "string" ? x.startsWith(prefix) : false
    )) {
        await new Promise((resolve, reject) => {
            const transaction = database.transaction(
                [IDBFS_STORE_NAME],
                "readwrite"
            );
            const request = transaction
                .objectStore(IDBFS_STORE_NAME)
                .delete(fileToRemove);
            request.onsuccess = () => resolve(null);
            request.onerror = () => reject();
        });
    }

    setStatus(`Pushing file into database...`);

    let buf = tar;
    while (1) {
        const [tarFile, rest] = tarReadFile(buf);
        if (!tarFile) {
            break;
        }
        buf = rest;

        const path = tarFile.path.startsWith("./")
            ? tarFile.path.slice(2)
            : tarFile.path;

        const fullPath = prefix + path;

        {
            const folderPath = fullPath.split("/").slice(0, -1).join("/");
            await new Promise((resolve, reject) => {
                const transaction = database.transaction(
                    [IDBFS_STORE_NAME],
                    "readwrite"
                );

                /** @type {IdbFileData} */
                const value = {
                    mode: 16877,
                    timestamp: new Date(),
                    contents: undefined,
                };
                const request = transaction
                    .objectStore(IDBFS_STORE_NAME)
                    .put(value, folderPath);
                request.onsuccess = () => resolve(null);
                request.onerror = () => reject();
            });
        }

        await new Promise((resolve, reject) => {
            const transaction = database.transaction(
                [IDBFS_STORE_NAME],
                "readwrite"
            );

            /** @type {IdbFileData} */
            const value = {
                mode: tarFile.data ? 33206 : 16877,
                timestamp: new Date(),
                contents: tarFile.data
                    ? new Int8Array(tarFile.data)
                    : undefined,
            };
            const request = transaction
                .objectStore(IDBFS_STORE_NAME)
                .put(value, fullPath);
            request.onsuccess = () => resolve(null);
            request.onerror = () => reject();
        });
    }

    setStatus(`Done`);
}

/*
(async () => {
    setStatus(`Loading database`);
    const database = await initDb();
    const files = await readFilesFromDb(database);
    renderSlots(files);

    for (let i = 1; i <= 10; i++) {
        const slotId = ("0" + i.toString()).slice(-2);
        const uploadButton = document.getElementById(`slot_${slotId}_upload`);
        if (!uploadButton) {
            throw new Error(`Internal error ${i}`);
        }
        uploadButton.onclick = () => {
            uploadSavegame(database, slotId)
                .then(() => {
                    setStatus(`Done, refreshing page...`);
                    setTimeout(() => {
                        window.location.reload();
                    }, 1500);
                })
                .catch((e) => {
                    console.warn(e);
                    setStatus(`${e.name} ${e.message}`);
                });
        };
    }

    setStatus("Ready");
})().catch((e) => {
    console.warn(e);
    setStatus(`${e.name} ${e.message}`);
});

*/

/**
 *
 * @param {typeof gamesConfig[number]} game
 * @param {HTMLElement} menuDiv
 */
function renderGameMenu(game, menuDiv) {
    const div = document.createElement("div");

    div.innerHTML = `
        <div class="game_header">${game.name}</div>
        <button id="start_${game.folder}">Start game</button>
        
    
    
    `;

    menuDiv.appendChild(div);

    const button = document.getElementById(`start_${game.folder}`);
    if (!button) {
        throw new Error(`No button!`);
    }
    button.addEventListener("click", () => {
        // @ts-ignore
        document.getElementById("menu").style.display = "none";
        // @ts-ignore
        document.getElementById("canvas").style.display = "";

        if (
            window.location.hostname !== "localhost" &&
            window.location.hostname !== "127.0.0.1"
        ) {
            document.body.requestFullscreen({
                navigationUI: "hide",
            });
        }

        (async () => {
            await initFilesystem(game.folder);
            setStatusText("Starting");
            removeRunDependency("initialize-filesystems");

            doBackgroundFilesPreload(game.folder, (fileInfo) =>
                savePreloadedFileToFs(game.folder, fileInfo)
            ).catch((e) => {
                console.warn(e);
                setStatusText(`Preloading files error: ${e.name} ${e.message}`);
            });
        })().catch((e) => {
            setErrorState(e);
        });
    });
}
function renderMenu() {
    const menuDiv = document.getElementById("menu");
    if (!menuDiv) {
        throw new Error(`No menu div!`);
    }

    for (const game of gamesConfig) {
        renderGameMenu(game, menuDiv);
    }
}

renderMenu();
