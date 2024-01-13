import "./pako_inflate.min.js";

export const Inflate =
    /** @type {new () => { push(chunk: Uint8Array): void; readonly result: Uint8Array; }} */
    (/** @type {any} */ (window).pako.Inflate);

export const inflate = /** @type {(data: Uint8Array) => Uint8Array} */ (
    /** @type {any} */ (window).pako.inflate
);
