declare const FS;
declare const IDBFS;
declare const MEMFS;

declare const Asyncify;

declare function addRunDependency(depName: string);
declare function removeRunDependency(depName: string);

declare const wasmBinaryFile: string;

declare var setWindowTitle: (title: string) => void;
