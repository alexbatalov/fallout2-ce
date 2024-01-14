import { Inflate } from "./pako.mjs";

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
 * @param {(url: string) => Promise<Response>} [doFetch]
 */
export async function fetchArrayBufProgress(url, usePako, onProgress, doFetch) {
    const response = doFetch ? await doFetch(url) : await fetch(url);

    if (!response.body) {
        throw new Error(`No response body`);
    }
    if (response.status >= 300) {
        throw new Error(`Status is >=300`);
    }
    const contentLength = parseInt(
        response.headers.get("Content-Length") || "",
    );

    const inflator = usePako
        ? new Inflate()
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
