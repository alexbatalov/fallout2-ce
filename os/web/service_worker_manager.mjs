export function registerServiceWorker() {
    if ("serviceWorker" in navigator) {
        const doNotInstallServiceWorker =
            window.location.hostname === "localhost" ||
            window.location.origin.startsWith("https://dev.");

        if (doNotInstallServiceWorker) {
            console.info("Will not install service worker");
            navigator.serviceWorker.getRegistrations().then((registrations) => {
                for (const registration of registrations) {
                    console.info(
                        `Unregistering service worker ${registration.scope}`,
                    );
                    registration.unregister();
                }
            });
        } else {
            console.info("Registering service worker");
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
