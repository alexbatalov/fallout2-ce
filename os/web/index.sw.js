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
    "iniparser.js",
    "consts.js",
    "fetcher.js",
    "asyncfetchfs.js",
    "onscreen_keyboard.js",
    "tar.js",
    "pako_inflate.min.js",
    "fallout2-ce.wasm",
    "fallout2-ce.js",
    "fallout2-ce.ico",
    ".",
];

const VERSION = 40;

const ENGINE_CACHE_NAME = "engine";

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
    event.waitUntil(
        (async () => {
            for (const cacheKey of await caches.keys()) {
                // Drop old cache schema because newer asyncfetchfs saved files into cache on its own
                const GAMES_OLD_CACHE_PREFIX = "gamedata_";
                const GAMES_OLD_CACHE_PREFIX_2 = "gamescache";

                if (
                    cacheKey.startsWith(GAMES_OLD_CACHE_PREFIX) ||
                    cacheKey.startsWith(GAMES_OLD_CACHE_PREFIX_2)
                ) {
                    await caches.delete(cacheKey);
                }
            }

            await clients.claim();
        })()
    );
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

            if (CACHE_FILES.some((f) => url.endsWith(f))) {
                const cloned = responseFromNetwork.clone();
                console.warn(
                    "Service worker saved engine to cache during fetch. This should never happen because all engine files should be saved during install phase"
                );
                const cache = await caches.open(ENGINE_CACHE_NAME);
                cache.put(request, cloned);
            }

            return responseFromNetwork;
        })(event.request)
    );
});
