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
    "mainmenu.js",
    "mainmenu.css",
    "config.js",
    "consts.js",
    "asyncfetchfs.js",
    "tar.js",
    "pako_inflate.min.js",
    "fallout2-ce.wasm",
    "fallout2-ce.js",
    "fallout2-ce.ico",
    ".",
];

const VERSION = 17;

const ENGINE_CACHE_NAME = "engine";

importScripts("./consts.js");

me.addEventListener("install", (event) => {
    event.waitUntil(
        (async () => {
            const cache = await caches.open(ENGINE_CACHE_NAME);

            for (const fpath of CACHE_FILES) {
                await cache.add(
                    new Request(fpath, {
                        cache: "no-cache",
                        headers: {
                            "Cache-Control": "no-cache",
                        },
                    })
                );
            }

            await me.skipWaiting();
        })()
    );
});

me.addEventListener("activate", (event) => {
    event.waitUntil(clients.claim());
});

me.addEventListener("fetch", (event) => {
    // console.info("service worker request", event.request.url);

    // Skip cross-origin requests, like those for Google Analytics.
    if (!event.request.url.startsWith(me.location.origin)) {
        return;
    }

    event.respondWith(
        (async (request) => {
            const url = event.request.url;

            const cachedResponse = await caches.match(url);
            if (cachedResponse) {
                return cachedResponse;
            }

            const responseFromNetwork = await fetch(url);

            const cloned = responseFromNetwork.clone();
            if (CACHE_FILES.some((f) => url.endsWith(f))) {
                const cache = await caches.open(ENGINE_CACHE_NAME);
                cache.put(request, cloned);
            } else {
                const scopePath = new URL(me.registration.scope).pathname;
                let urlPath = new URL(url).pathname;
                const urlNoScope = urlPath.startsWith(scopePath)
                    ? urlPath.slice(scopePath.length)
                    : null;
                if (urlNoScope !== null) {
                    const [game, gameName] = urlNoScope.split("/");
                    if ("./" + game + "/" === GAME_PATH) {
                        const cacheName = GAMES_CACHE_PREFIX + gameName;
                        const cache = await caches.open(cacheName);
                        cache.put(request, cloned);
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

            return responseFromNetwork;
        })(event.request)
    );
});
