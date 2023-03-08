declare var pako: {
    Inflate: new () => {
        push(chunk: Uint8Array): void;
        readonly result: ArrayBuffer;
    },
    inflate: (data: Uint8Array) => Uint8Array,
};

declare var assert;

declare var FS;

declare var Asyncify;
