/**
 * @param {Uint8Array} tar
 * @returns {[{path: string, data: Uint8Array | null} | null, Uint8Array]} This is the result
 */
export function tarReadFile(tar) {
    if (tar.length === 0) {
        return [null, tar];
    }
    const header = tar.subarray(0, 512);
    if (header.length !== 512) {
        throw new Error(`No header!`);
    }

    if (header.every((x) => x === 0)) {
        return [null, tar];
    }

    // Assuming only latin1 characters
    const nameSuffix = String.fromCharCode(
        ...header.subarray(0, Math.min(100, header.indexOf(0)))
    );

    const sizeBuf = header.subarray(
        124,
        Math.min(124 + 12, header.indexOf(0, 124))
    );
    let fileSize = 0;
    for (let i = 0; i < sizeBuf.length; i++) {
        fileSize = fileSize + (sizeBuf[i] - 48) * 8 ** (sizeBuf.length - i - 1);
    }

    const isFile = header[156] === 0 || header[156] === 48;
    const isDirectory = header[156] === 53;
    if (!isFile && !isDirectory) {
        throw new Error(`Unsupported type=${header[156]}`);
    }
    if (String.fromCharCode(...header.subarray(257, 257 + 5)) !== "ustar") {
        throw new Error(`Only ustar is supported`);
    }

    const namePrefix = String.fromCharCode(
        ...header.subarray(345, Math.min(345 + 155, header.indexOf(0, 345)))
    );

    const filename = namePrefix + nameSuffix;

    tar = tar.subarray(512);
    const fileData = tar.subarray(0, fileSize);

    const lastBlockBytes = fileSize % 512;
    const paddingBytes = lastBlockBytes === 0 ? 0 : 512 - lastBlockBytes;
    tar = tar.subarray(fileSize + paddingBytes);

    return [
        {
            path: filename,
            data: isFile ? fileData : null,
        },
        tar,
    ];
}

export const tarEnding = new Uint8Array(new ArrayBuffer(512 * 2)).fill(0);
/**
 *
 * @param {string} path
 * @param {Uint8Array | Int8Array | null | undefined} data
 */
export function packTarFile(path, data) {
    const out = new Uint8Array(
        new ArrayBuffer(
            512 + (data ? data.length + ((512 - (data.length % 512)) % 512) : 0)
        )
    ).fill(0);

    /**
     *
     * @param {number} offset
     * @param {string} str
     */
    function writeStr(offset, str) {
        for (let i = 0; i < str.length; i++) {
            out[i + offset] = str.charCodeAt(i);
        }
    }

    writeStr(0, path);

    const modeStr = data ? "0000644" : "0000755";
    writeStr(100, modeStr);

    writeStr(108, "0000001");
    writeStr(116, "0000001");
    writeStr(136, "0000001");

    const sizeStr = (
        "000000000000" + (data ? data.length : 0).toString(8)
    ).slice(-11);
    writeStr(124, sizeStr);

    const typeFlag = data ? 48 : 48 + 5;
    out[156] = typeFlag;

    writeStr(257, "ustar ");

    let checksum = 0;
    for (let i = 0; i < 512; i++) {
        if (i < 148 || i >= 148 + 8) {
            checksum += out[i];
        } else {
            checksum += 32;
        }
    }
    const checksumStr = ("000000" + checksum.toString(8)).slice(-6);
    writeStr(148, checksumStr);
    out[148 + 7] = 32;

    if (data) {
        out.set(data, 512);
    }
    return out;
}
