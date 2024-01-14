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
 * @param {AsyncFetchFsNode} node
 * @returns
 */
function getNodePath(node) {
    let inGamePath = "";
    let searchNode = node;
    while (searchNode.name !== "/") {
        inGamePath = "/" + searchNode.name + inGamePath;
        searchNode = searchNode.parent;
    }
    inGamePath = inGamePath.slice(1);
    return inGamePath;
}

/**
 *
 * @typedef { {
 *       fetcher: (filePath: string, expectedSize?: number, expectedSha256hash?: string) => Promise<Uint8Array>
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
 *   unloadTimerId?: number
 * } & FsNode} AsyncFetchFsNode
 *
 * @typedef {{ node: AsyncFetchFsNode, fd: number, position: number}} AsyncFetchFsStream
 */

const MEGABYTE = 1024 * 1024;

export const ASYNCFETCHFS = {
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
        var root = ASYNCFETCHFS.createNode(null, "/", ASYNCFETCHFS.DIR_MODE, 0);
        /** @type {Record<string, AsyncFetchFsNode>} */
        var createdParents = {};
        function ensureParent(/** @type {string} */ path) {
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
        function base(/** @type {string} */ path) {
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
        /** @type {AsyncFetchFsNode} */
        parent,
        /** @type {string} */
        name,
        /** @type {typeof ASYNCFETCHFS.FILE_MODE | typeof ASYNCFETCHFS.DIR_MODE} */
        mode,
        /** @type {unknown} */
        dev,
        /** @type {number} */
        fileSize,
        /** @type {Date} */
        mtime,
        /** @type {Uint8Array} */
        contents,
        /** @type {string} */
        sha256hash,
        /** @type {AsyncFetchFsOptions} */
        opts
    ) {
        /** @type {AsyncFetchFsNode} */
        const node = FS.createNode(parent, name, mode);
        node.mode = mode;
        node.node_ops = ASYNCFETCHFS.node_ops;
        node.stream_ops = ASYNCFETCHFS.stream_ops;
        node.timestamp = (mtime || new Date()).getTime();
        if (ASYNCFETCHFS.FILE_MODE === ASYNCFETCHFS.DIR_MODE) {
            throw new Error(`Internal error`);
        }
        if (mode === ASYNCFETCHFS.FILE_MODE) {
            node.size = fileSize;
            node.contents = contents;
        } else {
            node.size = 4096;
            node.childNodes = {};
        }
        node.openedCount = 0;
        if (parent && parent.childNodes) {
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
                size: node.contents ? node.contents.byteLength : node.size,
                atime: new Date(node.timestamp),
                mtime: new Date(node.timestamp),
                ctime: new Date(node.timestamp),
                blksize: 4096,
                blocks: Math.ceil(
                    (node.contents ? node.contents.byteLength : node.size) /
                        4096
                ),
            };
        },
        setattr: function (
            /** @type {AsyncFetchFsNode} */ node,
            /** @type {{mode?: number, timestamp?: number}} */ attr
        ) {
            if (attr.mode !== undefined) {
                node.mode = attr.mode;
            }
            if (attr.timestamp !== undefined) {
                node.timestamp = attr.timestamp;
            }
        },
        lookup: function (
            /** @type {unknown} */ parent,
            /** @type {unknown} */ name
        ) {
            // console.info(`ASYNCFETCHFS node_ops.lookup: `,name);
            throw new FS.ErrnoError(ENOENT);
        },
        mknod: function (
            /** @type {AsyncFetchFsNode} */ parent,
            /** @type {string} */ name,
            /** @type {number} */ mode,
            /** @type {unknown} */ dev
        ) {
            console.info(
                `ASYNCFETCHFS mknod ` +
                    `parent=${getNodePath(parent)} name=${name} ` +
                    `mode=${mode} dev=${dev}`
            );
            // console.info(
            //     `ASYNCFETCHFS node_ops.mknod: `,
            //     parent,
            //     name,
            //     mode,
            //     dev
            // );
            if (FS.isBlkdev(mode) || FS.isFIFO(mode)) {
                // no supported
                console.info(
                    `ASYNCFETCHFS node_ops.mknod NOT SUPPORTED: `,
                    name
                );
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
            if (parent && parent.childNodes) {
                parent.childNodes[name] = node;
                parent.timestamp = node.timestamp;
            }

            return node;
        },
        rename: function (
            /** @type {unknown} */ oldNode,
            /** @type {unknown} */ newDir,
            /** @type {unknown} */ newName
        ) {
            console.info(`ASYNCFETCHFS node_ops.rename: `, newName);
            throw new FS.ErrnoError(EPERM);
        },
        unlink: function (
            /** @type {unknown} */ parent,
            /** @type {unknown} */ name
        ) {
            console.info(`ASYNCFETCHFS node_ops.unlink: `, name);
            throw new FS.ErrnoError(EPERM);
        },
        rmdir: function (
            /** @type {unknown} */ parent,
            /** @type {unknown} */ name
        ) {
            console.info(`ASYNCFETCHFS node_ops.rmdir: `, name);
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
        symlink: function (
            /** @type {unknown} */ parent,
            /** @type {unknown} */ newName,
            /** @type {unknown} */ oldPath
        ) {
            console.info(`ASYNCFETCHFS node_ops.rmdir: `, newName);
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
            if (!stream.node.contents) {
                throw new Error(
                    `Node ${stream.node.name} have no content during read`
                );
            }

            if (position >= stream.node.contents.byteLength) {
                return 0;
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
                `ASYNCFETCHFS write ` +
                    `${getNodePath(stream.node)} offset=${offset} ` +
                    `len=${length} pos=${position} ` +
                    `curSize=${stream.node.contents?.byteLength}`
            );
            // console.info(
            //     `ASYNCFETCHFS write`,
            //     stream,
            //     buffer,
            //     offset,
            //     length,
            //     position
            // );
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
                    position += stream.node.contents
                        ? stream.node.contents.byteLength
                        : stream.node.size;
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

            if (node.unloadTimerId) {
                clearTimeout(node.unloadTimerId);
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
                const inGamePath = getNodePath(node);

                const opts = node.opts;

                const data = await opts.fetcher(
                    inGamePath,
                    node.size,
                    node.sha256hash
                );

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
                const unloadTimeoutMs = 1000;
                node.unloadTimerId = setTimeout(() => {
                    // console.info(`Unloaded node`, node.name)
                    if (node.openedCount === 0) {
                        node.contents = undefined;
                    } else {
                        console.warn(`Got unload even but node is opened`);
                    }
                }, unloadTimeoutMs);
            }
        },
    },
};
