export function registerServiceWorker() {
    if ("serviceWorker" in navigator) {
        if (window.location.hostname !== "localhost") {
            const originalServiceWorker = navigator.serviceWorker.controller;

            navigator.serviceWorker.register("index.sw.js");

            navigator.serviceWorker.addEventListener("controllerchange", () => {
                if (preventReloadCounter > 0) {
                    isReloadPending = true;
                } else {
                    if (originalServiceWorker) {
                        window.location.reload();
                    }
                }
            });
        }
    }
}

let preventReloadCounter = 0;

let isReloadPending = false;

export function preventAutoreload() {
    preventReloadCounter++;
    return () => {
        preventReloadCounter--;
        if (isReloadPending) {
            window.location.reload();
        }
    };
}
