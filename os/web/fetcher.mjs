/**
 * @typedef { (filePath: string, fileData: Uint8Array) => Uint8Array } FileTransformer
 */

import { fetchArrayBufProgress } from "./fetchArrayBufProgress.mjs";

/**
 *
 * @param {string | null} cacheName
 */
function createCachingFetch(cacheName) {
    /** @type {Cache | undefined} */
    let openedCache = undefined;

    return async (/** @type {string} */ url) => {
        if (cacheName) {
            if (!openedCache) {
                openedCache = await caches.open(cacheName);
            }

            const cachedMatch = await openedCache.match(url);
            if (cachedMatch) {
                return cachedMatch;
            }
        }

        const response = await fetch(url);

        if (openedCache) {
            const cloned = response.clone();
            openedCache.put(url, cloned);
        }

        return response;
    };
}

/**
 *
 * @param {string} urlPrefix
 * @param {string | null} cacheName
 * @param {boolean} useGzip
 * @param {(msg: string|null) => void} onFetching
 * @param {FileTransformer?} fileTransformer
 */
export function createFetcher(
    urlPrefix = "",
    cacheName,
    useGzip,
    onFetching,
    fileTransformer
) {
    if (!onFetching) {
        onFetching = () => {};
    }

    const cachingFetch = createCachingFetch(cacheName);
    return async (
        /** @type {string} */
        filePath,
        /** @type {number|undefined} */ expectedSize,
        /** @type {string|undefined} */ expectedSha256hash
    ) => {
        onFetching(filePath);

        const fullUrl = urlPrefix + filePath + (useGzip ? ".gz" : "");

        /** @type {Uint8Array | null} */
        let data = null;
        while (1) {
            onFetching(filePath);
            data = await fetchArrayBufProgress(
                fullUrl,
                useGzip,
                (downloadedBytes, contentLength) => {
                    const progress = downloadedBytes / contentLength;
                    onFetching(`${filePath} ${Math.floor(progress * 100)}%`);
                },
                cachingFetch
            ).catch((e) => {
                console.info(e);
                return null;
            });
            if (data) {
                break;
            }
            onFetching(`Network error, retrying...`);
            await new Promise((resolve) => setTimeout(resolve, 3000));
        }
        if (!data) {
            throw new Error(`Internal error`);
        }
        onFetching(null);
        if (expectedSize !== undefined && expectedSize !== data.byteLength) {
            onFetching(
                `Error with size of ${filePath}, expected=${expectedSize} received=${data.byteLength}`
            );

            throw new Error("Data file size mismatch");
        }

        // @TODO: Check hash if provided

        if (fileTransformer) {
            data = fileTransformer(filePath, data);
        }

        return data;
    };
}
