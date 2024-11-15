import { configuration } from "./config.mjs";
import { removeGameCache } from "./gamecache.mjs";
import { addHotkeysForFullscreen } from "./hotkeys_and_workarounds.mjs";
import { IniParser } from "./iniparser.mjs";
import { downloadAllGameFiles, initFilesystem } from "./initFilesystem.mjs";
import { isTouchDevice } from "./onscreen_keyboard.mjs";
import { inflate } from "./pako.mjs";
import { resizeCanvas } from "./resizeCanvas.mjs";
import { preventAutoreload } from "./service_worker_manager.mjs";
import { setErrorState } from "./setErrorState.mjs";
import { setStatusText } from "./setStatusText.mjs";
import { packTarFile, tarEnding, tarReadFile } from "./tar.mjs";

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
            IDBFS_DB_VERSION,
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

            if (!fileStore.indexNames.contains("timestamp")) {
                fileStore.createIndex("timestamp", "timestamp", {
                    unique: false,
                });
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
        typeof x === "string" ? x.startsWith(prefix) : false,
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
                entry ? entry.contents : null,
            ),
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
        input.accept = ".tar,.tar.gz,.tgz";
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

    const tar =
        file.name.endsWith(".gz") || file.name.endsWith(".tgz")
            ? inflate(new Uint8Array(raw))
            : new Uint8Array(raw);

    setStatusText(`Checking tar file...`);
    let commonPrefix = "";
    {
        /** @type {string[]} */
        const seenFiles = [];
        let buf = tar;
        while (1) {
            const [tarFile, rest] = tarReadFile(buf);
            if (!tarFile) {
                break;
            }
            buf = rest;
            seenFiles.push(tarFile.path);
        }

        const saveDatFile = seenFiles.find(
            (x) => x === "SAVE.DAT" || x.endsWith("/SAVE.DAT"),
        );

        if (!saveDatFile) {
            throw new Error(`There is no SAVE.DAT file!`);
        }

        commonPrefix = saveDatFile.slice(
            0,
            saveDatFile.length - "SAVE.DAT".length,
        );
        if (seenFiles.some((x) => !x.startsWith(commonPrefix))) {
            throw new Error(`Files are not in the same folder!`);
        }

        console.info(
            `Common prefix: ${commonPrefix}, saveDatFile: ${saveDatFile}`,
        );
    }

    const prefix = `/${folderName}/data/SAVEGAME/${slotFolderName}/`;
    {
        setStatusText(`Removing old saving...`);
        const files = await readFilesFromDb(database);
        for (const fileToRemove of [...files.keys()].filter((x) =>
            typeof x === "string" ? x.startsWith(prefix) : false,
        )) {
            console.info(`Removing ${fileToRemove}`);
            await new Promise((resolve, reject) => {
                const transaction = database.transaction(
                    [IDBFS_STORE_NAME],
                    "readwrite",
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

            const path = tarFile.path.slice(commonPrefix.length);

            const fullPath = prefix + path;

            console.info(`Saving into ${fullPath}`);

            {
                const folderPath = fullPath.split("/").slice(0, -1).join("/");
                await new Promise((resolve, reject) => {
                    const transaction = database.transaction(
                        [IDBFS_STORE_NAME],
                        "readwrite",
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
                    "readwrite",
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
        `/${folderName}/data/SAVEGAME/${slotFolderName}/SAVE.DAT`,
    );
    if (!saveDat || !saveDat.contents) {
        return null;
    }

    const expectedHeader = "FALLOUT SAVE FILE";
    const observedHeader = String.fromCharCode(
        ...saveDat.contents.slice(0, expectedHeader.length),
    );
    if (expectedHeader !== observedHeader) {
        return null;
    }

    const saveName = new TextDecoder("windows-1251").decode(
        saveDat.contents.slice(
            0x3d,
            Math.min(0x3d + 0x1e, saveDat.contents.indexOf(0, 0x3d)),
        ),
    );
    return saveName;
}

/**
 *
 * @param {string} gameFolder
 * @param {HTMLElement} slotsDiv
 * @param {LangData} lang
 * @returns {Promise<number>}
 */
async function renderGameSlots(gameFolder, slotsDiv, lang) {
    slotsDiv.innerHTML = "...";

    const database = await initDb(gameFolder);
    const files = await readFilesFromDb(database);

    slotsDiv.innerHTML = "";

    let usedSlots = 0;

    for (let i = 1; i <= 10; i++) {
        const slotDiv = document.createElement("div");
        slotDiv.className = "game_slot";

        const slotFolderName = `SLOT` + i.toString().padStart(2, "0");

        const saveName = getSaveInfo(files, gameFolder, slotFolderName);

        if (saveName !== null) {
            usedSlots++;
        }

        slotDiv.innerHTML = `
            <div class="game_slot_id">${lang.slot} ${i}</div>
            
            
                ${
                    saveName !== null
                        ? `<a class="game_slot_name" href="#" id="download_${gameFolder}_${slotFolderName}">[${
                              saveName || lang.noName
                          }]</a>`
                        : ""
                }

                <a class="game_slot_upload" href="#" id="upload_${gameFolder}_${slotFolderName}">${
                    lang.import
                }</a>
               
            
        `;

        slotsDiv.appendChild(slotDiv);

        const uploadButton = document.getElementById(
            `upload_${gameFolder}_${slotFolderName}`,
        );
        if (!uploadButton) {
            throw new Error(`No upload button!`);
        }
        uploadButton.addEventListener("click", (e) => {
            e.preventDefault();

            preventAutoreload();
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
                `download_${gameFolder}_${slotFolderName}`,
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

    return usedSlots;
}

/**
 *
 * @param {HTMLElement} elem
 */
function goFullscreen(elem) {
    if (elem.requestFullscreen) {
        return elem.requestFullscreen({
            navigationUI: "hide",
        });
        // @ts-ignore
    } else if (elem.mozRequestFullScreen) {
        // @ts-ignore
        return elem.mozRequestFullScreen({
            navigationUI: "hide",
        });
        // @ts-ignore
    } else if (elem.webkitRequestFullScreen) {
        // @ts-ignore
        return elem.webkitRequestFullScreen({
            navigationUI: "hide",
        });
        // @ts-ignore
    } else if (elem.webkitEnterFullscreen) {
        // @ts-ignore
        return elem.webkitEnterFullscreen();
    }
}

/**
 *
 * @param {typeof configuration['games'][number]} game
 * @param {HTMLElement} menuDiv
 * @param {LangData} lang
 * @param {boolean} hideWhenNoSaveGames
 */
function renderGameMenu(game, menuDiv, lang, hideWhenNoSaveGames) {
    const div = document.createElement("div");

    div.className = "game_menu";
    div.innerHTML = `
        <div class="game_header"><a href="#/${game.folder}" id="select_game_${
            game.folder
        }">${game.name}</a></div>
        <button class="game_start" id="start_${game.folder}">${
            lang.startGame
        }</button>
        <div class="game_slots" id="game_slots_${game.folder}">...</div>

        <div class="game_options">
            <input type="checkbox" id="enable_hires_${
                game.folder
            }" name="enable_hires_${game.folder}" />
            <label for="enable_hires_${game.folder}">Enable hi-res mode</label>
        </div>
        <div class="game_bottom_container">
            <div class="game_links">
              ${game.links
                  .map((link) => `<a href="${link}">${link}</a>`)
                  .join("")}
            </div>
            <div class="game_cache_actions">
              <a href="#" id="game_download_${game.folder}"></a>
              <a href="#" id="game_cleanup_${game.folder}"></a>              
            </div>
        </div>
    
    `;

    div.style.display = "none";

    menuDiv.appendChild(div);

    const selectGameLink = document.getElementById(
        `select_game_${game.folder}`,
    );
    if (!selectGameLink) {
        throw new Error(`No link!`);
    }
    selectGameLink.onclick = () => {
        redirectToPath(`/${game.folder}`);
    };

    const enableHiResCheckbox = /** @type {HTMLInputElement | null} */ (
        document.getElementById(`enable_hires_${game.folder}`)
    );
    if (!enableHiResCheckbox) {
        throw new Error(`No checkbox!`);
    }

    {
        const enableHiResCheckboxLocalStorageKey = `enable_hires_${game.folder}`;
        const enableHiResCheckboxLocalStorageValue = localStorage.getItem(
            enableHiResCheckboxLocalStorageKey,
        );
        if (enableHiResCheckboxLocalStorageValue === "no") {
            enableHiResCheckbox.checked = false;
        } else if (enableHiResCheckboxLocalStorageValue === "yes") {
            enableHiResCheckbox.checked = true;
        } else {
            // Keep some default value
        }
        enableHiResCheckbox.addEventListener("change", () => {
            if (enableHiResCheckbox.checked) {
                localStorage.setItem(enableHiResCheckboxLocalStorageKey, "yes");
            } else {
                localStorage.setItem(enableHiResCheckboxLocalStorageKey, "no");
            }
        });
    }

    const startButton = document.getElementById(`start_${game.folder}`);
    if (!startButton) {
        throw new Error(`No button!`);
    }
    startButton.addEventListener("click", () => {
        preventAutoreload();

        // This will not reload page
        window.location.hash = `/${game.folder}`;

        // @ts-ignore
        document.getElementById("menu").style.display = "none";

        const canvas = /** @type {HTMLCanvasElement | null} */ (
            document.getElementById("canvas")
        );
        if (!canvas) {
            throw new Error(`No canvas!`);
        }
        canvas.style.display = "";

        const canvasParent = canvas.parentElement;
        if (!canvasParent) {
            throw new Error(`Canvas have no parent element!`);
        }
        const isUsingHiRes = enableHiResCheckbox.checked;

        (async () => {
            let isGoingFullscreen;
            if (
                window.location.hostname !== "localhost" &&
                window.location.hostname !== "127.0.0.1"
            ) {
                isGoingFullscreen = true;
                try {
                    await goFullscreen(canvasParent);
                } catch (e) {
                    isGoingFullscreen = false;
                }

                function goFullscreenOnClick() {
                    setTimeout(() => {
                        if (!canvasParent) {
                            return;
                        }
                        document.addEventListener("click", () => {
                            goFullscreen(canvasParent);
                        });
                        document.addEventListener("touchend", () => {
                            goFullscreen(canvasParent);
                        });
                    }, 1);
                }

                if (!isUsingHiRes) {
                    // When not using hi-res then make fullscren optional
                    // and enforce only on mobiles
                    addHotkeysForFullscreen(canvasParent);
                    if (isTouchDevice()) {
                        goFullscreenOnClick();
                    }
                } else {
                    // In high-res mode we always enforce fullscreen
                    // because we have fixed-size canvas
                    if (isGoingFullscreen) {
                        addHotkeysForFullscreen(canvasParent);
                        goFullscreenOnClick();
                    }
                }
            } else {
                isGoingFullscreen = false;
            }

            canvasParent.style.display = "flex";
            canvasParent.style.justifyContent = "center";
            canvasParent.style.alignItems = "center";

            if (!isUsingHiRes) {
                canvas.width = 640;
                canvas.height = 480;
                window.addEventListener("resize", resizeCanvas);
                resizeCanvas();
            }

            if (isTouchDevice()) {
                // This is a small workaround for Android Chrome
                // For some reasons even awaited fullscreen makes the element
                // to have sliglthy lower height than it should have
                await new Promise((r) => setTimeout(r, 200));
            }

            const cssPixelWidth = canvasParent.clientWidth;
            const cssPixelHeight = canvasParent.clientHeight;

            const gameScreenWidth = Math.floor(
                cssPixelWidth * devicePixelRatio,
            );
            const gameScreenHeight = Math.floor(
                cssPixelHeight * devicePixelRatio,
            );

            if (isUsingHiRes) {
                canvas.style.width = `${cssPixelWidth}px`;
                canvas.style.height = `${cssPixelHeight}px`;
            }

            /** @type {import("./fetcher.mjs").FileTransformer} */
            const fileTransformer = async (filePath, data) => {
                if (filePath.toLowerCase() === "f2_res.ini") {
                    const iniParser = new IniParser(data);

                    iniParser.setValue("MAIN", "WINDOWED", "1");

                    if (!isUsingHiRes) {
                        iniParser.setValue("MAIN", "SCR_WIDTH", `640`);
                        iniParser.setValue("MAIN", "SCR_HEIGHT", `480`);
                        iniParser.setValue("MAIN", "SCALE_2X", "0");
                    } else {
                        iniParser.setValue(
                            "MAIN",
                            "SCR_WIDTH",
                            `${gameScreenWidth}`,
                        );
                        iniParser.setValue(
                            "MAIN",
                            "SCR_HEIGHT",
                            `${gameScreenHeight}`,
                        );

                        const scaling = Math.min(
                            Math.floor(gameScreenWidth / 640),
                            Math.floor(gameScreenHeight / 480),
                        );

                        iniParser.setValue(
                            "MAIN",
                            "SCALE_2X",
                            (scaling - 1).toString(),
                        );

                        iniParser.setValue("IFACE", "IFACE_BAR_MODE", "0");
                        iniParser.setValue(
                            "IFACE",
                            "IFACE_BAR_WIDTH",
                            `${gameScreenWidth >= 800 ? 800 : 640}`,
                        );
                        iniParser.setValue("IFACE", "IFACE_BAR_SIDE_ART", "2");
                        iniParser.setValue("IFACE", "IFACE_BAR_SIDES_ORI", "0");

                        iniParser.setValue(
                            "STATIC_SCREENS",
                            "SPLASH_SCRN_SIZE",
                            `1`,
                        );
                    }

                    const iniData = iniParser.pack();
                    console.info(
                        "f2_res.ini:\n",
                        String.fromCharCode(...iniData),
                    );
                    return iniData;
                } else if (filePath.toLowerCase() === "ddraw.ini") {
                    const iniParser = new IniParser(data);

                    if (isUsingHiRes) {
                        iniParser.setValue("Main", "HiResMode", "1");
                    }

                    const iniData = iniParser.pack();
                    console.info(
                        "ddraw.ini:\n",
                        String.fromCharCode(...iniData),
                    );
                    return iniData;
                } else if (
                    filePath.toLowerCase() ===
                        "data/art/intrface/hr_ifacelft2.frm" ||
                    filePath.toLowerCase() ===
                        "data/art/intrface/hr_ifacerht2.frm"
                ) {
                    if (data.byteLength !== 0) {
                        return data;
                    }

                    // Workaround to fix missing files without re-calculating index
                    // TODO: Just add those files into the game folder and remove this

                    const fName =
                        "./game/FalloutNevadaEng/Patch001.dat/ART/INTRFACE/" +
                        filePath.toLowerCase().split("/").pop() +
                        ".gz";

                    const newDataPackedBuf = await fetch(fName)
                        .then((res) => res.arrayBuffer())
                        .catch((e) => {});
                    if (!newDataPackedBuf) {
                        // If failed to download then do not stop
                        return new Uint8Array(0);
                    }
                    const newData = inflate(new Uint8Array(newDataPackedBuf));
                    return new Uint8Array(newData);
                }
                return data;
            };
            await initFilesystem(
                game.folder,
                game.filesVersion,
                async (indexOriginal) => {
                    const index = [...indexOriginal];

                    for (const filePath of [
                        "f2_res.ini",
                        "ddraw.ini",

                        // Those files are missing in the index
                        // This is workaround to skip re-calculation of index
                        // TODO: Remove me
                        "data/art/intrface/hr_ifacelft2.frm",
                        "data/art/intrface/hr_ifacerht2.frm",
                    ]) {
                        if (
                            !index.find(
                                (f) => f.name.toLowerCase() === filePath,
                            )
                        ) {
                            // If file is not present then we have to add it here with content
                            const fileContent = await fileTransformer(
                                filePath,
                                new Uint8Array(0),
                            );
                            const calculatedHex = [
                                ...new Uint8Array(fileContent),
                            ]
                                .map((digit) =>
                                    digit.toString(16).padStart(2, "0"),
                                )
                                .join("")
                                .toLowerCase();

                            index.push({
                                name: filePath,
                                size: 0,
                                contents: fileContent,
                                sha256hash: calculatedHex,
                            });
                        }
                    }
                    return index;
                },
                fileTransformer,
            );
            setStatusText("Starting");
            removeRunDependency("initialize-filesystems");
            {
                // EMSCRIPTEN uses this function to set title
                // We override it no it will be no-op
                setWindowTitle = () => {};
                document.title = game.name;
            }
        })().catch((e) => {
            setErrorState(e);
        });
    });

    const slotsDiv = document.getElementById(`game_slots_${game.folder}`);
    if (!slotsDiv) {
        throw new Error(`No button!`);
    }
    renderGameSlots(game.folder, slotsDiv, lang).then((usedSlots) => {
        if (hideWhenNoSaveGames && usedSlots === 0) {
            div.style.display = "none";
        } else {
            div.style.display = "";
        }
    });

    const cleanup_link = document.getElementById(`game_cleanup_${game.folder}`);
    if (!cleanup_link) {
        throw new Error(`No button!`);
    }

    let isBusyWithDownloading = false;

    const cleanup_link_text = lang.clearCache;
    cleanup_link.innerHTML = cleanup_link_text;
    cleanup_link.addEventListener("click", (e) => {
        e.preventDefault();
        if (isBusyWithDownloading) {
            return;
        }
        if (
            getGameCacheDownloadedStatus(game.folder) &&
            !confirm(lang.clearCacheConfirm)
        ) {
            return;
        }
        setGameCacheDownloadedStatus(game.folder, false);
        removeGameCache(game.folder, null)
            .then(() => {
                cleanup_link.innerHTML = lang.ready;
            })
            .catch((e) => {
                cleanup_link.innerHTML = lang.error;
            })
            .then(() => {
                setTimeout(() => {
                    cleanup_link.innerHTML = cleanup_link_text;
                }, 1000);
            });
    });

    const download_link = document.getElementById(
        `game_download_${game.folder}`,
    );
    if (!download_link) {
        throw new Error(`No button!`);
    }
    const download_link_text = lang.downloadGame;
    download_link.innerHTML = download_link_text;

    download_link.addEventListener("click", async (e) => {
        e.preventDefault();
        if (isBusyWithDownloading) {
            return;
        }
        if (getGameCacheDownloadedStatus(game.folder)) {
            if (!confirm(lang.alreadyDownloadedConfirm)) {
                return;
            }
        } else {
            if (!confirm(lang.downloadConfirm)) {
                return;
            }
        }
        const reloadPreventStop = preventAutoreload();
        download_link.innerHTML = lang.downloading;
        isBusyWithDownloading = true;

        const wakeLockSentinel =
            "wakeLock" in navigator
                ? await navigator.wakeLock.request().catch((e) => null)
                : null;

        try {
            await downloadAllGameFiles(game.folder, game.filesVersion);
            setGameCacheDownloadedStatus(game.folder, true);
            alert(lang.ready);
        } catch (e) {
            alert(
                `${lang.error}: ${
                    e instanceof Error ? e.name + " " + e.message : e
                }`,
            );
        }

        download_link.innerHTML = download_link_text;
        isBusyWithDownloading = false;

        reloadPreventStop();
        if (wakeLockSentinel) {
            await wakeLockSentinel.release();
        }
    });
}

/**
 * @param {string} url
 */
function redirectToPath(url) {
    window.location.hash = url;
    window.location.reload();
}

const langData = /** @type {const} */ ({
    ru: {
        header: "Фаллаут Невада и Сонора в браузере",
        help:
            "~ = Esc\n" +
            "Тап двумя пальцами = клик правой кнопкой\n" +
            "Скролл двумя пальцами = двигать окно\n" +
            "F11 или ctrl+f = на весь экран",
        startGame: "Запустить игру",
        slot: "Слот",
        noName: "Нет имени",
        import: "Импорт",
        downloadGame: "Загрузить в оффлайн",
        clearCache: "Очистить кэш",
        clearCacheConfirm:
            "Дейсвительно очистить кэш игры?\n" +
            "После этого игра будет опять требовать интернета",
        ready: "Готово",
        error: "Ошибка",
        alreadyDownloadedConfirm:
            "Игра уже загружена но можно перепроверить файлы\n" + "Продолжить?",
        downloadConfirm:
            "Загрузка может занять какое-то время и место на диске\n" +
            "Но после этого игра будет запускаться без интернета\n" +
            "Продолжить?",
        downloading: "Загружаю...",
        showAllVersions: "Показать все версии",
        showAllGames: "Показать все игры",
        langName: "Русский",
    },
    en: {
        header: "Fallout Nevada and Sonora in the browser",
        help:
            "~ = Esc\n" +
            "Tap two fingers = right mouse click\n" +
            "Scroll two fingers = move window\n" +
            "F11 or ctrl+f = fullscreen",
        startGame: "Start game",
        slot: "Slot",
        noName: "No name",
        import: "Import",
        downloadGame: "Download to offline",
        clearCache: "Clear cache",
        clearCacheConfirm:
            "Really to remove game cache?\n" +
            "After this the game will require internet again",
        ready: "Ready",
        error: "Error",
        alreadyDownloadedConfirm:
            "The game is already downloaded but we can re-check files\n" +
            "Proceed?",
        downloadConfirm:
            "Downloading can take some time and disk space\n" +
            "But after this the game will work without internet connection\n" +
            "Proceed?",
        downloading: "Downloading...",
        showAllVersions: "Show all versions",
        showAllGames: "Show all games",
        langName: "English",
    },
});

/**
 * @typedef {typeof langData} LangDataObj
 * @typedef {LangDataObj[keyof LangDataObj]} LangData
 */

export function renderMenu() {
    const menuDiv = document.getElementById("menu");
    if (!menuDiv) {
        throw new Error(`No menu div!`);
    }

    // <no hash> - redirect to automatic language
    // #/ru - use russial language
    // #/ru/all - use russian language and show all games, not only first of each type
    // #/Fallout_Sonora - show only the game, use language from the game

    const [langOrGameStr, showAllStr] = window.location.hash
        .slice(1)
        .split("/")
        .slice(1);
    const isOneGameSelected = configuration.games.find(
        (x) => x.folder === langOrGameStr,
    );
    const langKey = /** @type {keyof typeof langData} */ (
        !isOneGameSelected ? langOrGameStr : isOneGameSelected.lang
    );
    const lang = langData[langKey];

    if (!langOrGameStr || !lang) {
        const isRusLang = (
            navigator.languages || [navigator.language || "ru"]
        ).some((lang) => lang.startsWith("ru"));

        redirectToPath(isRusLang ? "/ru" : "/en");
        return;
    }

    const appendDiv = (/** @type {string} */ html) => {
        const infoDiv = document.createElement("div");
        menuDiv.appendChild(infoDiv);
        infoDiv.innerHTML = html;
    };

    appendDiv(`<div class="mainmenu_header">
${lang.header}
    `);

    appendDiv(`<div class="info_help">
        ${lang.help}
</div>`);

    const renderingGames = isOneGameSelected
        ? [
              {
                  gameInfo: isOneGameSelected,
                  hideWhenNoSaveGames: false,
              },
          ]
        : configuration.games
              .filter((game) => game.lang === langKey)
              .map((gameInfo, index, arr) => {
                  let hideWhenNoSaveGames;
                  if (showAllStr === "all") {
                      hideWhenNoSaveGames = false;
                  } else {
                      // Keep only first of the game type
                      hideWhenNoSaveGames =
                          arr.findIndex(
                              (x) => x.gameType === gameInfo.gameType,
                          ) !== index;
                  }

                  return {
                      gameInfo,
                      hideWhenNoSaveGames,
                  };
              });

    for (const game of renderingGames) {
        renderGameMenu(game.gameInfo, menuDiv, lang, game.hideWhenNoSaveGames);
    }

    const links = [
        "https://github.com/roginvs/fallout2-ce",
        "https://github.com/alexbatalov/fallout2-ce",
    ];

    if (
        renderingGames.some((x) => x.hideWhenNoSaveGames) ||
        isOneGameSelected
    ) {
        appendDiv(`<div class="show_all_games">
        <button id="show_all_games">${
            isOneGameSelected ? lang.showAllGames : lang.showAllVersions
        }</button>
    </div>`);
        const showAllVersionsOrGamesButton =
            document.getElementById("show_all_games");
        if (!showAllVersionsOrGamesButton) {
            throw new Error("No button element");
        }
        showAllVersionsOrGamesButton.onclick = () => {
            redirectToPath(
                isOneGameSelected ? `/${langKey}` : `/${langKey}/all`,
            );
        };
    }

    appendDiv(`<div class="info_links">
       ${links.map((link) => `<a href="${link}">${link}</a>`).join("")}
    </div>`);

    {
        appendDiv(`<div class="languages_links">
            ${Object.keys(langData)
                .map(
                    (langKey) =>
                        `<a href="#/${langKey}" id="language_link_${langKey}">${
                            langData[/** @type {keyof LangDataObj} */ (langKey)]
                                .langName
                        }</a>`,
                )
                .join("")}
        </div>`);
        for (const k of Object.keys(langData)) {
            const link = document.getElementById(`language_link_${k}`);
            if (!link) {
                throw new Error("No link element");
            }
            link.onclick = () => {
                redirectToPath(`/${k}`);
            };
        }
    }
}

/**
 * @param {string} gameName
 */
function getLocalStorageKeyForDownloadedGameFlag(gameName) {
    return gameName + "___downloaded";
}

/**
 * @param {string} gameName
 */
function getGameCacheDownloadedStatus(gameName) {
    return !!localStorage.getItem(
        getLocalStorageKeyForDownloadedGameFlag(gameName),
    );
}

/**
 * @param {string} gameName
 * @param {boolean} isDownloaded
 */
function setGameCacheDownloadedStatus(gameName, isDownloaded) {
    const key = getLocalStorageKeyForDownloadedGameFlag(gameName);
    if (isDownloaded) {
        localStorage.setItem(key, "yes");
    } else {
        localStorage.removeItem(key);
    }
}
