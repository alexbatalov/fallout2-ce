/**
 *
 * @param {number | null} size
 */
function createDummyInflator(size) {
    if (size !== null && !isNaN(size)) {
        const buffer = new Uint8Array(size);
        let pos = 0;
        return {
            /**
             * @param {Uint8Array} chunk
             */
            push(chunk) {
                buffer.set(chunk, pos);
                pos += chunk.length;
            },
            get result() {
                return buffer;
            },
        };
    } else {
        /** @type {Uint8Array[]} */
        const chunks = [];
        let receivedLength = 0;
        return {
            /**
             * @param {Uint8Array} chunk
             */
            push(chunk) {
                chunks.push(chunk);
                receivedLength += chunk.length;
            },
            get result() {
                let chunksAll = new Uint8Array(receivedLength);
                let position = 0;
                for (let chunk of chunks) {
                    chunksAll.set(chunk, position);
                    position += chunk.length;
                }
                return chunksAll;
            },
        };
    }
}

/**
 *
 * @param {string} url
 * @param {boolean} usePako
 * @param {(downloaded: number, total: number) => void} onProgress,
 * @param {(url: string) => Response} doFetch
 */
async function fetchArrayBufProgress(
    url,
    usePako,
    onProgress,
    doFetch = fetch
) {
    const response = await doFetch(url);
    if (!response.body) {
        throw new Error(`No response body`);
    }
    if (response.status >= 300) {
        throw new Error(`Status is >=300`);
    }
    const contentLength = parseInt(
        response.headers.get("Content-Length") || ""
    );

    const inflator = usePako
        ? new pako.Inflate()
        : createDummyInflator(contentLength);

    const reader = response.body.getReader();
    let downloadedBytes = 0;
    while (1) {
        const { done, value } = await reader.read();

        if (done) {
            break;
        }

        inflator.push(value);

        downloadedBytes += value.length;
        if (!isNaN(contentLength)) {
            onProgress(downloadedBytes, contentLength);
        }
    }

    const data = inflator.result;
    return data;
}

/**
 * @typedef { (filePath: string, fileData: Uint8Array) => Uint8Array } FileTransformer
 */

/**
 *
 * @param {string} cacheName
 */
function createCachingFetch(cacheName) {
    /** @type {Cache | undefined} */
    let openedCache = undefined;

    return async (/** @type {string} */ url) => {
        if (cacheName && !openedCache) {
            openedCache = await caches.open(cacheName);
        }

        const cachedMatch = await openedCache.match(url);
        if (cachedMatch) {
            return cachedMatch;
        }

        const response = await fetch(url);

        {
            const cloned = response.clone();
            openedCache.put(url, cloned);
        }

        return response;
    };
}

/**
 *
 * @param {string} urlPrefix
 * @param {string?} cacheName
 * @param {boolean} useGzip
 * @param {(msg: string|null) => void} onFetching
 * @param {FileTransformer?} fileTransformer
 */
function createFetcher(
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
                `Error with size of ${inGamePath}, expected=${node.size} received=${data.byteLength}`
            );

            throw new Error("Data file size mismatch");
        }

        // @TODO: Save into cache

        if (fileTransformer) {
            data = fileTransformer(filePath, data);
        }

        return data;
    };
}
