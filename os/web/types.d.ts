declare var pako: {
    Inflate: new () => {
        push(chunk: Uint8Array): void;
        readonly result: ArrayBuffer;
    }
};

declare var assert;

declare var FS;

declare var Asyncify;
