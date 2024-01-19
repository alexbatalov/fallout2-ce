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

        if (response.status >= 300) {
            throw new Error(`Status is >=300`);
        }

        if (openedCache) {
            const cloned = response.clone();
            openedCache.put(url, cloned);
        }

        return response;
    };
}

/**
 * Some game files contains special symbols which do not work as url
 * @param {string} path
 */
function encodeFalloutPathToUrl(path) {
    return path.split("#").join("%23");
}

/**
 *
 * @param {string} urlPrefix
 * @param {string | null} cacheName
 * @param {boolean} useGzip
 * @param {(msg: string|null) => void} onFetching
 * @param {FileTransformer?} fileTransformer
 * @param {string} [filesVersion]
 */
export function createFetcher(
    urlPrefix = "",
    cacheName,
    useGzip,
    onFetching,
    fileTransformer,
    filesVersion
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

        const fullUrl =
            urlPrefix +
            encodeFalloutPathToUrl(filePath) +
            (useGzip ? ".gz" : "") +
            // This suffix is to force browser to download new files
            // Previous deployment did not have "Expires" header so browser cached files
            (filesVersion ? `?v=${filesVersion}` : "");

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

            throw new Error(`Data file size mismatch on file '${filePath}'`);
        }

        if (expectedSha256hash !== undefined) {
            const calculatedBuf = await crypto.subtle.digest("SHA-256", data);
            const calculatedHex = [...new Uint8Array(calculatedBuf)]
                .map((digit) => digit.toString(16).padStart(2, "0"))
                .join("")
                .toLowerCase();
            if (expectedSha256hash.toLowerCase() !== calculatedHex) {
                throw new Error(`Data hash mismatch on file '${filePath}'`);
            }
        }

        if (fileTransformer) {
            data = fileTransformer(filePath, data);
        }

        return data;
    };
}
