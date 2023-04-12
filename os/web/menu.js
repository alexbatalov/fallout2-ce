// @ts-check
// @filename: types.d.ts
// @filename: tar.js
// @filename: games.js

const IDBFS_STORE_NAME = "FILE_DATA";

/**
 * @param {string} folderName
 * @returns {Promise<IDBDatabase>}
 * */
async function initDb(folderName) {
    const IDBFS_DB_VERSION = 21;

    return new Promise((resolve, reject) => {
        const request = window.indexedDB.open(
            `/${folderName}/data/SAVEGAME`,
            IDBFS_DB_VERSION
        );
        request.onerror = (event) => {
            reject(request.error);
        };
        request.onsuccess = (event) => {
            const database = request.result;
            resolve(database);
        };
        request.onupgradeneeded = (e) => {         
            // Copy-paste from IDBFS code   

            // @ts-ignore
            var db = e.target.result;
            // @ts-ignore
            var transaction = e.target.transaction;
    
            var fileStore;
    
            if (db.objectStoreNames.contains(IDBFS_STORE_NAME)) {
              fileStore = transaction.objectStore(IDBFS_STORE_NAME);
            } else {
              fileStore = db.createObjectStore(IDBFS_STORE_NAME);
            }
    
            if (!fileStore.indexNames.contains('timestamp')) {
              fileStore.createIndex('timestamp', 'timestamp', { unique: false });
            }
        };
    });
}



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
 * @param {Map<IDBValidKey, IdbFileData>} files
 * @param {string} folderName
 * @param {string} slotFolderName
 * @param {string} saveName
 */
function downloadSlot(files, folderName, slotFolderName, saveName) {
    const prefix = `/${folderName}/data/SAVEGAME/${slotFolderName}/`;
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

    downloadBuf(tarBuf, `${slotFolderName} ${saveName}.tar`);
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
   @param {string} folderName
 * @param {string} slotFolderName
 */
async function uploadSavegame(database, folderName, slotFolderName) {
    setStatusText(`Pick a file...`);
    const file = await pickFile();

    setStatusText(`Loading file...`);
    const url = URL.createObjectURL(file);
    const raw = await fetch(url).then((x) => x.arrayBuffer());
    URL.revokeObjectURL(url);

    const tar = file.name.endsWith(".gz")
        ? pako.inflate(new Uint8Array(raw))
        : new Uint8Array(raw);

    setStatusText(`Checking tar file...`);
    {
        let saveDatFound = false;

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

            if (path === "SAVE.DAT") {
                saveDatFound = true;
                break;
            }
        }

        if (!saveDatFound) {
            throw new Error(`There is no SAVE.DAT file!`);
        }
    }

    const prefix = `/${folderName}/data/SAVEGAME/${slotFolderName}/`;
    {
        setStatusText(`Removing old saving...`);
        const files = await readFilesFromDb(database);
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
    }

    {
        setStatusText(`Pushing file into database...`);
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
    }

    setStatusText(`Done`);
}


/**
 * @param {Map<IDBValidKey, IdbFileData>} files
 * @param {string} folderName
 * @param {string} slotFolderName
 */
function getSaveInfo(files, folderName, slotFolderName) {
    const saveDat = files.get(
        `/${folderName}/data/SAVEGAME/${slotFolderName}/SAVE.DAT`
    );
    if (!saveDat || !saveDat.contents) {
        return null;
    }

    const expectedHeader = "FALLOUT SAVE FILE";
    const observedHeader = String.fromCharCode(
        ...saveDat.contents.slice(0, expectedHeader.length)
    );
    if (expectedHeader !== observedHeader) {
        return null;
    }

    const saveName = new TextDecoder("windows-1251").decode(
        saveDat.contents.slice(
            0x3d,
            Math.min(0x3d + 0x1e, saveDat.contents.indexOf(0, 0x3d))
        )
    );
    return saveName;
}


/**
 *
 * @param {string} gameFolder
 * @param {HTMLElement} slotsDiv
 */
async function renderGameSlots(gameFolder, slotsDiv) {
    slotsDiv.innerHTML = "...";

    const database = await initDb(gameFolder);
    const files = await readFilesFromDb(database);

    slotsDiv.innerHTML = "";

    for (let i = 1; i <= 10; i++) {
        const slotDiv = document.createElement("div");
        slotDiv.className = "game_slot";

        const slotFolderName = `SLOT` + i.toString().padStart(2, "0");

        const saveName = getSaveInfo(files, gameFolder, slotFolderName);

        slotDiv.innerHTML = `
            <div class="game_slot_id">Slot ${i}</div>
            
            
                ${
                    saveName !== null
                        ? `<a class="game_slot_name" href="#" id="download_${gameFolder}_${slotFolderName}">${saveName}</a>`
                        : ""
                }

                <a class="game_slot_upload" href="#" id="upload_${gameFolder}_${slotFolderName}">upload</a>
               
            
        `;

        slotsDiv.appendChild(slotDiv);

        const uploadButton = document.getElementById(
            `upload_${gameFolder}_${slotFolderName}`
        );
        if (!uploadButton) {
            throw new Error(`No upload button!`);
        }
        uploadButton.addEventListener("click", (e) => {
            e.preventDefault();

            (async () => {
                const reopenedDb = await initDb(gameFolder);
                await uploadSavegame(reopenedDb, gameFolder, slotFolderName);
                setStatusText(`Done, refreshing page...`);
                window.location.reload();
            })().catch((e) => {
                console.warn(e);
                setErrorState(e);
            });
        });

        if (saveName !== null) {
            const downloadButton = document.getElementById(
                `download_${gameFolder}_${slotFolderName}`
            );
            if (!downloadButton) {
                throw new Error(`No download button`);
            }

            downloadButton.addEventListener("click", (e) => {
                e.preventDefault();
                downloadSlot(files, gameFolder, slotFolderName, saveName);
            });
        }
    }

    database.close();
}


/**
 *
 * @param {typeof gamesConfig[number]} game
 * @param {HTMLElement} menuDiv
 */
function renderGameMenu(game, menuDiv) {
    const div = document.createElement("div");

    div.className = "game_menu";
    div.innerHTML = `
        <div class="game_header">${game.name}</div>
        <button class="game_start" id="start_${game.folder}">Start game</button>
        <div class="game_slots" id="game_slots_${game.folder}">...</div>

        <div class="game_bottom_container">
            <div class="game_links">
              ${game.links
                  .map((link) => `<a href="${link}">${link}</a>`)
                  .join("")}
            </div>
            <div class="game_cleanup">
              <a href="#" id="game_cleanup_${game.folder}"></a>
            </div>
        </div>
    
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

            doBackgroundFilesPreload(
                game.folder,
                (fileInfo) => savePreloadedFileToFs(game.folder, fileInfo)
                // savePreloadedFileToServiceWorkerCache(game.folder, fileInfo)
            ).catch((e) => {
                console.warn(e);
                setStatusText(`Preloading files error: ${e.name} ${e.message}`);
            });
        })().catch((e) => {
            setErrorState(e);
        });
    });

    const slotsDiv = document.getElementById(`game_slots_${game.folder}`);
    if (!slotsDiv) {
        throw new Error(`No button!`);
    }
    renderGameSlots(game.folder, slotsDiv);

    const cleanup_link = document.getElementById(`game_cleanup_${game.folder}`);
    if (!cleanup_link) {
        throw new Error(`No button!`);
    }

    const cleanup_link_text = "Clean cache";
    cleanup_link.innerHTML = cleanup_link_text;
    cleanup_link.addEventListener("click", (e) => {
        e.preventDefault();
        caches
            .delete(GAMES_CACHE_PREFIX + game.folder)
            .then(() => {
                cleanup_link.innerHTML = "Done!";
            })
            .then(() => new Promise((r) => setTimeout(r, 2000)))
            .then(() => (cleanup_link.innerHTML = cleanup_link_text));
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
