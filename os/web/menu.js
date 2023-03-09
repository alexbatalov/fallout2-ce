// @ts-check
// @filename: types.d.ts

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
 * @param {Map<IDBValidKey, IdbFileData>} files 
 * @param {string} slotId 
 */
function downloadSlot(files, slotId){
    const prefix = `/app/data/SAVEGAME/SLOT${slotId}/`;
    const filesList = [...files.keys()].filter(x => typeof x === 'string' ? x.startsWith(prefix):false)
    console.info(filesList)
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
        } else {
            const expectedHeader = "FALLOUT SAVE FILE";
            const observedHeader = String.fromCharCode(
                ...saveDat.contents.slice(0, expectedHeader.length)
            );
            if (expectedHeader !== observedHeader) {
                slotDiv.innerHTML = `<div class="slot_status">Slot ${slotId}: Header error</div>`;
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
            downloadButton.onclick = () => downloadSlot(files, slotId)
        }
        const uploadButton = document.getElementById(`slot_${slotId}_upload`);
        if (!uploadButton) {
            throw new Error(`Internal error`);
        }
    }
}

function setStatus(msg) {
    const el = document.getElementById("status_text");
    if (!el) {
        throw new Error(`No status text div`);
    }
    el.innerHTML = msg;
}
(async () => {
    setStatus(`Loading database`);
    const database = await initDb();
    const files = await readFilesFromDb(database);

    renderSlots(files);

    setStatus("Ready");
})().catch((e) => {
    console.warn(e);
    setStatus(`${e.name} ${e.message}`);
});
