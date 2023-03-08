// @ts-check

/**
 * @param {Uint8Array} tar
 */
function readTar(tar) {
    /** @type { {name: string, data: Uint8Array}[] } */
    const result = [];
    while (true) {
        if (tar.length === 0) {
            break;
        }
        const header = tar.slice(0, 512);
        if (header.length !== 512) {
            throw new Error(`No header!`);
        }

        if (header.every((x) => x === 0)) {
            break;
        }

        // Assuming only latin1 characters
        const nameSuffix = String.fromCharCode(
            ...header.slice(0, Math.min(100, header.indexOf(0)))
        );

        const sizeBuf = header.slice(
            124,
            Math.min(124 + 12, header.indexOf(0, 124))
        );
        let fileSize = 0;
        for (let i = 0; i < sizeBuf.length; i++) {
            fileSize =
                fileSize + (sizeBuf[i] - 48) * 8 ** (sizeBuf.length - i - 1);
        }

        if (header[156] !== 0 && header[156] !== 48) {
            throw new Error(`Links are not supported`);
        }
        if (String.fromCharCode(...header.slice(257, 257 + 5)) !== "ustar") {
            throw new Error(`Only ustar is supported`);
        }

        const namePrefix = String.fromCharCode(
            ...header.slice(345, Math.min(345 + 155, header.indexOf(0, 345)))
        );

        const filename = namePrefix + nameSuffix;

        tar = tar.slice(512);
        const fileData = tar.slice(0, fileSize);

        const lastBlockBytes = fileSize % 512;
        const paddingBytes = lastBlockBytes === 0 ? 0 : 512 - lastBlockBytes;
        tar = tar.slice(fileSize + paddingBytes);

        result.push({
            name: filename,
            data: fileData,
        });
    }

    return result;
}
