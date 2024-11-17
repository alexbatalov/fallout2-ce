import { fetchArrayBufProgress } from "./fetchArrayBufProgress.mjs";
import {
    addBackquoteAsEscape,
    addRightMouseButtonWorkaround,
} from "./hotkeys_and_workarounds.mjs";
import "./pako.mjs";
import { setStatusText } from "./setStatusText.mjs";
import "./onscreen_keyboard.mjs";
import { setErrorState } from "./setErrorState.mjs";
import { renderMenu } from "./mainmenu.mjs";
import { initializeGlobalModuleObject, loadEmscriptenJs } from "./wasm.mjs";
import { removeOldCache } from "./gamecache.mjs";
import { registerServiceWorker } from "./service_worker_manager.mjs";

window.addEventListener("error", (err) => {
    console.info("error", err);
    console.info(
        `Error is: ${err.error?.name}: ${err.error?.message} ${err.error?.stack}`
    );
    setErrorState(err.error);
});
window.addEventListener("unhandledrejection", (err) => {
    console.info("unhandledrejection", err);
    console.info(
        `Reason is: ${err.reason?.name}: ${err.reason?.message} ${err.reason?.stack}`
    );
});

initializeGlobalModuleObject();
await loadEmscriptenJs();

addRightMouseButtonWorkaround();
addBackquoteAsEscape();

renderMenu();

removeOldCache();

registerServiceWorker();
