// @ts-check
/// <reference lib="webworker" />

/** @type ServiceWorkerGlobalScope */
// @ts-ignore
const me = self;

/** @type Clients */
var clients;

const CACHE_FILES = [
    "index.html",
    "index.js",
    "index.css",
    "menu.js",
    "menu.css",
    "config.js",
    "asyncfetchfs.js",
    "tar.js",
    "pako_inflate.min.js",
    "fallout2-ce.wasm",
    "fallout2-ce.js",
    "fallout2-ce.ico",
];

const VERSION = 2;

const ENGINE_CACHE_NAME = "engine";

const GAMES_CACHE_PREFIX = "gamedata_";

me.addEventListener("install", (event) => {
    event.waitUntil(
        caches
            .open(ENGINE_CACHE_NAME)
            .then((cache) => cache.addAll(CACHE_FILES))
            .then(() => me.skipWaiting())
    );
});

me.addEventListener("activate", (event) => {
    event.waitUntil(clients.claim());
});

me.addEventListener("fetch", (event) => {
    console.info("requsted", event.request.url);

    // Skip cross-origin requests, like those for Google Analytics.
    if (!event.request.url.startsWith(me.location.origin)) {
        console.info("KEK skipped", event.request.url);
        return;
    }

    // Engine fetching
    event.respondWith(
        (async (request) => {
            let url = event.request.url;
            if (request.url === me.registration.scope) {
                url = url + "index.html";
            }

            const cachedResponse = await caches.match(url);
            if (cachedResponse) {
                return cachedResponse;
            }

            const responseFromNetwork = await fetch(url);

            const cloned = responseFromNetwork.clone();
            if (CACHE_FILES.some((f) => url.endsWith(f))) {
                const cache = await caches.open(ENGINE_CACHE_NAME);
                await cache.put(request, cloned);
            } else {
                const scopePath = new URL(me.registration.scope).pathname;
                let urlPath = new URL(url).pathname;
                const urlNoScope = urlPath.startsWith(scopePath)
                    ? urlPath.slice(scopePath.length)
                    : null;
                if (urlNoScope !== null) {
                    const [game, gameName] = urlNoScope.split("/");
                    if (game === "game") {
                        const cacheName = GAMES_CACHE_PREFIX + gameName;
                        const cache = await caches.open(cacheName);
                        await cache.put(request, cloned);
                    } else {
                        console.warn(`What is this request for? ${url}`);
                    }
                } else {
                    console.warn(
                        `LOL unable to detect path`,
                        me.registration.scope,
                        url
                    );
                }
            }

            // In theory this should never happen
            return responseFromNetwork;
        })(event.request)
    );
    // }
});
