declare var pako: {
    Inflate: new () => {
        push(chunk: Uint8Array): void;
        readonly result: Uint8Array;
    },
    inflate: (data: Uint8Array) => Uint8Array,
};

declare function assert(value: any): asserts value;

declare const FS;
declare const IDBFS;
declare const MEMFS;

declare const Asyncify;

declare function addRunDependency(depName: string);
declare function removeRunDependency(depName: string);

declare const wasmBinaryFile: string;