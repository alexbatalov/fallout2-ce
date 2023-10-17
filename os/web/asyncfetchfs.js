// @ts-check

/// <reference path="types.d.ts" />

const S_IFDIR = 0o0040000;
const S_IFREG = 0o0100000;
const ENOENT = 2;
const EPERM = 1;
const EIO = 5;
const SEEK_SET = 0;
const SEEK_CUR = 1;
const SEEK_END = 2;
const EINVAL = 22;

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
 * @param {(downloaded: number, total: number) => void} onProgress
 */
async function fetchArrayBufProgress(url, usePako, onProgress) {
    const response = await fetch(url);
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
 *
 * @typedef { (filePath: string, fileData: Uint8Array) => Uint8Array } FileTransformer
 * @typedef { {
 *       fileTransformer?: FileTransformer
 *       onFetching: (msg: string|null) => void;
 *       pathPrefix: string;
 *       useGzip: boolean;
 * } } AsyncFetchFsOptions
 * @typedef { {
 *     files: {
 *       name: string,
 *       size: number,
 *       contents: Uint8Array | null,
 *       sha256hash?: string,
 *     }[];
 *     options: AsyncFetchFsOptions
 * } } AsyncFetchFsConfig
 */

/**
 * @typedef {unknown} FsNodeOps
 * @typedef {unknown} FsStreamOps
 *
 * @typedef {{
 *      id: number,
 *      mode: number,
 *      node_ops: FsNodeOps,
 *      stream_ops: FsStreamOps,
 *      timestamp: number,
 *      name: string
 * }} FsNode
 *
 * @typedef {{
 *   size: number,
 *   contents?: Uint8Array,
 *   childNodes?: Record<string,AsyncFetchFsNode>,
 *   openedCount: number,
 *   opts: AsyncFetchFsOptions,
 *   sha256hash: string,
 *   parent: AsyncFetchFsNode,
 *   is_memfs: boolean,
 * } & FsNode} AsyncFetchFsNode
 *
 * @typedef {{ node: AsyncFetchFsNode, fd: number, position: number}} AsyncFetchFsStream
 */

const MEGABYTE = 1024 * 1024;
/** If file is bigger then it will never appear in in-memory cache */
const FILE_SIZE_CACHE_THRESHOLD = MEGABYTE * 10;

const ASYNCFETCHFS = {
    DIR_MODE: S_IFDIR | 511 /* 0777 */,
    FILE_MODE: S_IFREG | 511 /* 0777 */,

    /**
     *
     * @param {{
     *   opts: AsyncFetchFsConfig
     * }} mount
     * @returns
     */
    mount: function (mount) {
        if (mount.opts.options.useGzip && typeof pako === "undefined") {
            throw new Error(`useGzip is enabled but no pako in global scope`);
        }

        var root = ASYNCFETCHFS.createNode(null, "/", ASYNCFETCHFS.DIR_MODE, 0);
        var createdParents = {};
        function ensureParent(path) {
            // return the parent node, creating subdirs as necessary
            var parts = path.split("/");
            var parent = root;
            for (var i = 0; i < parts.length - 1; i++) {
                var curr = parts.slice(0, i + 1).join("/");
                // Issue 4254: Using curr as a node name will prevent the node
                // from being found in FS.nameTable when FS.open is called on
                // a path which holds a child of this node,
                // given that all FS functions assume node names
                // are just their corresponding parts within their given path,
                // rather than incremental aggregates which include their parent's
                // directories.
                if (!createdParents[curr]) {
                    createdParents[curr] = ASYNCFETCHFS.createNode(
                        parent,
                        parts[i],
                        ASYNCFETCHFS.DIR_MODE,
                        0
                    );
                }
                parent = createdParents[curr];
            }
            return parent;
        }
        function base(path) {
            var parts = path.split("/");
            return parts[parts.length - 1];
        }

        mount.opts.files.forEach(function (file) {
            ASYNCFETCHFS.createNode(
                ensureParent(file.name),
                base(file.name),
                ASYNCFETCHFS.FILE_MODE,
                0,
                file.size,
                undefined,
                file.contents,
                file.sha256hash,
                mount.opts.options
            );
        });
        return root;
    },
    createNode: function (
        parent,
        name,
        mode,
        dev,
        fileSize,
        mtime,
        contents,
        sha256hash,
        opts
    ) {
        /** @type {AsyncFetchFsNode} */
        const node = FS.createNode(parent, name, mode);
        node.mode = mode;
        node.node_ops = ASYNCFETCHFS.node_ops;
        node.stream_ops = ASYNCFETCHFS.stream_ops;
        node.timestamp = (mtime || new Date()).getTime();
        assert(ASYNCFETCHFS.FILE_MODE !== ASYNCFETCHFS.DIR_MODE);
        if (mode === ASYNCFETCHFS.FILE_MODE) {
            node.size = fileSize;
            node.contents = contents;
        } else {
            node.size = 4096;
            node.childNodes = {};
        }
        node.openedCount = 0;
        if (parent) {
            parent.childNodes[name] = node;
        }
        node.opts = opts;
        node.sha256hash = sha256hash;
        node.is_memfs = false;
        return node;
    },
    node_ops: {
        /**
         *
         * @param {AsyncFetchFsNode} node
         * @returns
         */
        getattr: function (node) {
            return {
                dev: 1,
                ino: node.id,
                mode: node.mode,
                nlink: 1,
                uid: 0,
                gid: 0,
                rdev: undefined,
                size: node.size,
                atime: new Date(node.timestamp),
                mtime: new Date(node.timestamp),
                ctime: new Date(node.timestamp),
                blksize: 4096,
                blocks: Math.ceil(node.size / 4096),
            };
        },
        setattr: function (node, attr) {
            if (attr.mode !== undefined) {
                node.mode = attr.mode;
            }
            if (attr.timestamp !== undefined) {
                node.timestamp = attr.timestamp;
            }
        },
        lookup: function (parent, name) {
            throw new FS.ErrnoError(ENOENT);
        },
        mknod: function (parent, name, mode, dev) {
            console.info(
                `ASYNCFETCHFS node_ops.mknod: `,
                parent,
                name,
                mode,
                dev
            );
            if (FS.isBlkdev(mode) || FS.isFIFO(mode)) {
                // no supported
                throw new FS.ErrnoError(63);
            }

            /** @type {AsyncFetchFsNode} */
            const node = FS.createNode(parent, name, mode, dev);
            node.is_memfs = true;
            node.node_ops = ASYNCFETCHFS.node_ops;
            node.stream_ops = ASYNCFETCHFS.stream_ops;

            if (FS.isDir(node.mode)) {
                node.childNodes = {};
                node.size = 4096;
            } else if (FS.isFile(node.mode)) {
                node.contents = new Uint8Array(0);
            }

            node.timestamp = new Date().getTime();

            node.openedCount = 0;
            if (parent) {
                parent.childNodes[name] = node;
                parent.timestamp = node.timestamp;
            }
            

            return node;
        },
        rename: function (oldNode, newDir, newName) {
            throw new FS.ErrnoError(EPERM);
        },
        unlink: function (parent, name) {
            throw new FS.ErrnoError(EPERM);
        },
        rmdir: function (parent, name) {
            throw new FS.ErrnoError(EPERM);
        },
        /**
         *
         * @param {AsyncFetchFsNode} node
         * @returns
         */
        readdir: function (node) {
            var entries = [".", ".."];
            if (!node.childNodes) {
                throw new Error(`readdir called on node ${node.name}`);
            }
            for (var key in node.childNodes) {
                if (!node.childNodes.hasOwnProperty(key)) {
                    continue;
                }
                entries.push(key);
            }
            return entries;
        },
        symlink: function (parent, newName, oldPath) {
            throw new FS.ErrnoError(EPERM);
        },
    },
    stream_ops: {
        /**
         *
         * @param {AsyncFetchFsStream} stream
         * @param {Uint8Array} buffer
         * @param {number} offset
         * @param {number} length
         * @param {number} position
         * @returns {number}
         */
        read: function (stream, buffer, offset, length, position) {
            if (position >= stream.node.size) return 0;
            if (!stream.node.contents) {
                throw new Error(
                    `Node ${stream.node.name} have no content during read`
                );
            }
            const chunk = stream.node.contents.subarray(
                position,
                position + length
            );
            buffer.set(new Uint8Array(chunk), offset);
            return chunk.byteLength;
        },
        /**
         *
         * @param {AsyncFetchFsStream} stream
         * @param {Uint8Array} buffer
         * @param {number} offset
         * @param {number} length
         * @param {number} position
         * @returns {number}
         */
        write: function (stream, buffer, offset, length, position) {
            console.info(
                `ASYNCFETCHFS write`,
                stream,
                buffer,
                offset,
                length,
                position
            );
            if (!length) {
                return 0;
            }

            const node = stream.node;
            if (!node.is_memfs) {
                node.is_memfs = true;
            }
            node.timestamp = Date.now();

            if (!node.contents) {
                throw new Error(`Unexpected: no content for writing`);
            }

            if (position + length > node.contents.byteLength) {
                const newBuf = new Uint8Array(position + length);
                newBuf.set(node.contents);
                node.contents = newBuf;
            }
            node.contents.set(
                buffer.subarray(offset, offset + length),
                position
            );
            
            return length;            
        },
        /**
         *
         * @param {AsyncFetchFsStream} stream
         * @param {number} offset
         * @param {number} whence
         * @returns {number}
         */
        llseek: function (stream, offset, whence) {
            var position = offset;
            if (whence === SEEK_CUR) {
                position += stream.position;
            } else if (whence === SEEK_END) {
                if (FS.isFile(stream.node.mode)) {
                    position += stream.node.size;
                }
            }
            if (position < 0) {
                throw new FS.ErrnoError(EINVAL);
            }
            return position;
        },
        /**
         *
         * @param {AsyncFetchFsStream} stream
         * @returns {void}
         */
        open: function (stream) {
            const node = stream.node;

            if (!FS.isFile(node.mode)) {
                return;
            }

            if (Asyncify.state == Asyncify.State.Normal) {
                if (node.contents) {
                    node.openedCount++;
                    return;
                }
            }

            if (node.is_memfs) {
                if (Asyncify.state !== Asyncify.State.Normal) {
                    throw new Error(
                        `Unexpected Asyncify state=${Asyncify.state}, memfs nodes are not async`
                    );
                }

                node.openedCount++;
                return;
            }

            // ___syscall_openat creates new stream everytime it runs
            // We need to close this stream when we run for the first time in async mode
            if (Asyncify.state == Asyncify.State.Normal) {
                FS.closeStream(stream.fd);
            }

            return Asyncify.handleAsync(async () => {
                let inGamePath = "";
                let searchNode = node;
                while (searchNode.name !== "/") {
                    inGamePath = "/" + searchNode.name + inGamePath;
                    searchNode = searchNode.parent;
                }
                inGamePath = inGamePath.slice(1);

                const opts = node.opts;
                opts.onFetching(inGamePath);

                const fullUrl =
                    opts.pathPrefix + inGamePath + (opts.useGzip ? ".gz" : "");
                // + (node.sha256hash ? `?${node.sha256hash}` : "");

                /** @type {Uint8Array | null} */
                let data = null;

                while (1) {
                    opts.onFetching(inGamePath);

                    data = await fetchArrayBufProgress(
                        fullUrl,
                        opts.useGzip,
                        (downloadedBytes, contentLength) => {
                            const progress = downloadedBytes / contentLength;
                            opts.onFetching(
                                `${inGamePath} ${Math.floor(progress * 100)}%`
                            );
                        }
                    ).catch((e) => {
                        console.info(e);
                        return null;
                    });

                    if (data) {
                        break;
                    }

                    opts.onFetching(`Network error, retrying...`);
                    await new Promise((resolve) => setTimeout(resolve, 3000));
                }

                if (!data) {
                    throw new Error(`Internal error`);
                }

                opts.onFetching(null);

                if (node.size !== data.byteLength) {
                    opts.onFetching(
                        `Error with size of ${inGamePath}, expected=${node.size} received=${data.byteLength}`
                    );
                    // This will cause Asyncify in suspended state but it is ok
                    throw new Error("Data file size mismatch");
                }

                if (opts.fileTransformer) {
                    data = opts.fileTransformer(inGamePath, data);
                }

                node.contents = data;
                node.openedCount++;
            });
        },
        /**
         *
         * @param {AsyncFetchFsStream} stream
         * @returns {void}
         */
        close: function (stream) {
            const node = stream.node;

            node.openedCount--;

            if (node.is_memfs) {
                return;
            }

            if (node.name === "fallout2.cfg") {
                // This is a huge workaround.
                // If we unload fallout2.cfg then game is not able to gracefully exit.
                // Probably somehow related to async open in write mode but i am not sure.
                // TODO: Find out what the hell is happening here
                return;
            }

            if (
                node.mode === ASYNCFETCHFS.FILE_MODE &&
                node.openedCount === 0
            ) {
                if (node.size > FILE_SIZE_CACHE_THRESHOLD) {
                    // Sometimes game can open the same file multiple times
                    // We rely on service worker caching so it will not be
                    // downloaded via network next time
                    node.contents = undefined;
                }
            }
        },
    },
};
