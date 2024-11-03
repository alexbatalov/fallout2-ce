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
        ? inflate(new Uint8Array(raw))
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
            `upload_${gameFolder}_${slotFolderName}`
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
        <div class="game_header">${game.name}</div>
        <button class="game_start" id="start_${game.folder}">${
        lang.startGame
    }</button>
        <div class="game_slots" id="game_slots_${game.folder}">...</div>

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

    const button = document.getElementById(`start_${game.folder}`);
    if (!button) {
        throw new Error(`No button!`);
    }
    button.addEventListener("click", () => {
        preventAutoreload();

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

        //const beforeFullscreen = `beforeFullscreen=${canvasParent.clientWidth}x${canvasParent.clientHeight} `+`${window.screen.availHeight} ${window.screen.height}`;;

        let screenWidth = 0;
        let screenHeight = 0;

        if (
            window.location.hostname !== "localhost" &&
            window.location.hostname !== "127.0.0.1"
        ) {
            // Workaround for Android
            screenWidth = window.screen ? window.screen.availWidth : 0;
            screenHeight = window.screen ? window.screen.availHeight : 0;

            goFullscreen(canvasParent);
            if (isTouchDevice()) {
                setTimeout(() => {
                    document.addEventListener("click", () => {
                        goFullscreen(canvasParent);
                    });
                    document.addEventListener("touchend", () => {
                        goFullscreen(canvasParent);
                    });
                }, 1);
            }

            addHotkeysForFullscreen(canvasParent);
        }

        // const beforeAsync = `beforeAsync=${canvasParent.clientWidth}x${canvasParent.clientHeight}`;

        (async () => {
            /** @type {import("./fetcher.mjs").FileTransformer} */
            const fileTransformer = (filePath, data) => {
                if (filePath.toLowerCase() === "f2_res.ini") {
                    const IS_RESIZING_DISABLED = true;
                    if (IS_RESIZING_DISABLED) {
                        // Disabled because of missing HDR patches
                        return data;
                    }

                    const iniParser = new IniParser(data);

                    // const onLoading = `onLoading=${canvasParent.clientWidth}x${canvasParent.clientHeight}`;

                    screenWidth = screenWidth || canvasParent.clientWidth;
                    screenHeight = screenHeight || canvasParent.clientHeight;

                    const screenRatio = screenWidth / screenHeight;

                    const growingWidth = screenRatio >= 4 / 3;

                    const MAX_RATIO_HORIZONTAL = 16 / 9;
                    const MIN_RATIO_VERTICAL = 1 / MAX_RATIO_HORIZONTAL;

                    const canvasPixelWidth = growingWidth
                        ? Math.floor(
                              480 * Math.min(MAX_RATIO_HORIZONTAL, screenRatio)
                          )
                        : 640;
                    const canvasPixelHeight = growingWidth
                        ? 480
                        : Math.floor(
                              640 / Math.max(MIN_RATIO_VERTICAL, screenRatio)
                          );

                    iniParser.setValue(
                        "MAIN",
                        "SCR_HEIGHT",
                        `${canvasPixelHeight}`
                    );
                    iniParser.setValue(
                        "MAIN",
                        "SCR_WIDTH",
                        `${canvasPixelWidth}`
                    );

                    iniParser.setValue("IFACE", "IFACE_BAR_MODE", "0");
                    iniParser.setValue(
                        "IFACE",
                        "IFACE_BAR_WIDTH",
                        `${canvasPixelWidth >= 800 ? 800 : 640}`
                    );
                    iniParser.setValue("IFACE", "IFACE_BAR_SIDE_ART", "2");
                    iniParser.setValue("IFACE", "IFACE_BAR_SIDES_ORI", "0");

                    // At this point we assume that graphics is not initialized so it is safe to resize canvas
                    canvas.width = canvasPixelWidth;
                    canvas.height = canvasPixelHeight;
                    resizeCanvas();

                    return iniParser.pack();
                }
                return data;
            };
            await initFilesystem(
                game.folder,
                game.filesVersion,
                fileTransformer
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
        `game_download_${game.folder}`
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
                }`
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
        downloadGame: "Download to offile",
        clearCache: "Clear cache",
        clearCacheConfirm:
            "Really to remove game cache?\n" +
            "After this the game will require internet again",
        ready: "Ready",
        error: "Error",
        alreadyDownloadedConfirm:
            "The game is already downloaded but we can check files\n" +
            "Proceed?",
        downloadConfirm:
            "Downloading can take some time and disk space\n" +
            "But after this the game will work without internet connection\n" +
            "Proceed?",
        downloading: "Downloading...",
        showAllVersions: "Show all versions",
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

    const [langStr, filter] = window.location.hash.slice(1).split("/").slice(1);
    if (!langStr || !(langStr in langData)) {
        const isRusLang = (
            navigator.languages || [navigator.language || "ru"]
        ).filter((lang) => lang.startsWith("ru"));

        redirectToPath(isRusLang ? "/ru" : "/en");
        return;
    }

    const langKey = /** @type {keyof typeof langData} */ (langStr);
    const lang = langData[langKey];

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

    const renderingGames = configuration.games
        .filter((game) => game.lang === langKey)
        .map((gameInfo, index, arr) => {
            let hideWhenNoSaveGames;
            if (filter === "all") {
                hideWhenNoSaveGames = false;
            } else {
                // Keep only first of the game type
                hideWhenNoSaveGames =
                    arr.findIndex((x) => x.gameType === gameInfo.gameType) !==
                    index;
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

    if (filter !== "all") {
        appendDiv(`<div class="show_all_games">
        <button id="show_all_games">${lang.showAllVersions}</button>
    </div>`);
        const showAllVersionsButton = document.getElementById("show_all_games");
        if (!showAllVersionsButton) {
            throw new Error("No button element");
        }
        showAllVersionsButton.onclick = () => {
            redirectToPath(`/${langKey}/all`);
        };
    }

    appendDiv(`<div class="info_links">
       ${links.map((link) => `<a href="${link}">${link}</a>`).join("")}
    </div>`);
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
        getLocalStorageKeyForDownloadedGameFlag(gameName)
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
