const CACHE_FILES = [
    "menu.html",
    "menu.js",
    "menu.css",
    "tar.js",
    "pako_inflate.min.js",
];

const VERSION = 1;

const CACHE_NAME = "menu";

self.addEventListener("install", (event) => {
    event.waitUntil(
        caches
            .open(CACHE_NAME)
            .then((cache) => cache.addAll(CACHE_FILES))
            .then(self.skipWaiting())
    );
});

self.addEventListener("fetch", (event) => {
    // Skip cross-origin requests, like those for Google Analytics.
    if (!event.request.url.startsWith(self.location.origin)) {
        return;
    }

    if (CACHE_FILES.every((f) => !event.request.url.endsWith(f))) {
        return;
    }

    event.respondWith(
        caches.match(event.request).then((cachedResponse) => {
            if (cachedResponse) {
                return cachedResponse;
            }
            return fetch(event.request);
        })
    );
});
