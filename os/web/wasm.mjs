import { fetchArrayBufProgress } from "./fetchArrayBufProgress.mjs";
import { loadJs } from "./loadJs.mjs";
import { setErrorState } from "./setErrorState.mjs";
import { setStatusText } from "./setStatusText.mjs";

export function initializeGlobalModuleObject() {
    if (/** @type {any} */ (window).Module) {
        throw new Error(`This file must be the first to load`);
    }

    /** @type {any} */ (window).Module = {
        canvas: document.getElementById("canvas"),
        setStatus: (/** @type {string} */ msg) =>
            msg && console.info("setStatus", msg),
        preRun: [],
        preInit: [() => addRunDependency("initialize-filesystems")],
        onRuntimeInitialized: () => {},
        onAbort: (/** @type {string} */ what) => {
            console.warn("aborted!", what);
        },
        onExit: (/** @type {number} */ code) => {
            console.info(`Exited with code ${code}`);
            document.exitPointerLock();
            document.exitFullscreen().catch((e) => {});
            if (code === 0) {
                setStatusText(`Exited with code ${code}`);
                window.location.reload();
            } else {
                setErrorState(new Error(`Exited with code ${code}`));
            }
        },

        instantiateWasm: (
            /** @type {WebAssembly.Imports} */ info,
            /** @type {(instance: WebAssembly.Instance, module: WebAssembly.Module) => void} */ receiveInstance
        ) => {
            (async () => {
                setStatusText("Loading WASM binary");

                const arrayBuffer = await fetchArrayBufProgress(
                    wasmBinaryFile,
                    false,
                    (loaded, total) =>
                        setStatusText(
                            `WASM binary loading ${Math.floor(
                                (loaded / total) * 100
                            )}%`
                        )
                );

                // await new Promise(r => setTimeout(r, 10000));

                setStatusText("Instantiating WebAssembly");
                const inst = await WebAssembly.instantiate(arrayBuffer, info);
                setStatusText("");
                receiveInstance(inst.instance, inst.module);
            })().catch((e) => {
                console.warn(e);
                setErrorState(e);
            });
        },
    };
}

export async function loadEmscriptenJs() {
    setStatusText("Loading emscripten");
    await loadJs("./fallout2-ce.js");
}
