export class IniParser {
    /** @type { Record<string, Record<string,string>>}} */
    #data = {};

    /**
     *
     * @param {Uint8Array} data
     */
    constructor(data) {
        const originalString = String.fromCharCode(...data);
        let currentHeader = "";
        for (const line of originalString.split("\n").map((x) => x.trim())) {
            if (!line || line.startsWith(";")) {
                continue;
            }
            if (line.startsWith("[") && line.endsWith("]")) {
                currentHeader = line.slice(1, -1).trim();
                continue;
            }
            const idx = line.indexOf("=");
            const key = line.slice(0, idx).trim();
            const value = line.slice(idx + 1).trim();

            const currentHeaderVal = this.#data[currentHeader] || {};
            currentHeaderVal[key] = value;
            this.#data[currentHeader] = currentHeaderVal;
        }
    }

    /**
     *
     * @param {string} header
     * @param {string} key
     */
    #getCaseInsensitivePath(header, key) {
        for (const origHeader of Object.keys(this.#data)) {
            if (origHeader.toLowerCase() === header.toLowerCase()) {
                for (const origKey of Object.keys(this.#data[origHeader])) {
                    if (origKey.toLowerCase() === key.toLowerCase()) {
                        return [origHeader, origKey];
                    }
                }
                return [origHeader, null];
            }
        }
        return [null, null];
    }

    /**
     *
     * @param {string} header
     * @param {string} key
     */
    getValue(header, key) {
        const [origHeader, origKey] = this.#getCaseInsensitivePath(header, key);
        if (origHeader === null || origKey === null) {
            return null;
        }
        return this.#data[origHeader][origKey];
    }

    /**
     *
     * @param {string} header
     * @param {string} key
     * @param {string} value
     */
    setValue(header, key, value) {
        let [origHeader, origKey] = this.#getCaseInsensitivePath(header, key);
        if (origHeader === null) {
            origHeader = header;
            this.#data[origHeader] = {};
        }
        if (origKey === null) {
            origKey = key;
        }
        this.#data[origHeader][origKey] = value;
    }

    pack() {
        /** @type {string[]} */
        const outLines = [];

        for (const origHeader of Object.keys(this.#data)) {
            outLines.push(`[${origHeader}]`);
            for (const origKey of Object.keys(this.#data[origHeader])) {
                outLines.push(`${origKey}=${this.#data[origHeader][origKey]}`);
            }
        }
        const out = outLines.join("\n") + "\n";

        return new Uint8Array(out.split("").map((x) => x.charCodeAt(0)));
    }
}
