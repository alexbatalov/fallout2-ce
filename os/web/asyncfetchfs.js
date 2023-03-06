const S_IFDIR = 0040000;
const S_IFREG = 0100000;
const ENOENT = 2;
const EPERM = 1;
const EIO = 5;
const SEEK_SET = 0;
const SEEK_CUR = 1;
const SEEK_END = 2;
const EINVAL = 22;

function fetchWithRetry(fullUrl, inGamePath, onFetching, onDone) {
    onFetching(inGamePath);

    const req = new XMLHttpRequest();
    req.open("GET", fullUrl, true);
    req.responseType = "arraybuffer";

    req.onload = (event) => {
        const arrayBuffer = req.response; // Note: not req.responseText

        if (!arrayBuffer || req.status >= 300) {
            onFetching(`${inGamePath} retrying...`);
            setTimeout(() => {
                fetchWithRetry(fullUrl, inGamePath, onFetching, onDone);
            }, 10000);
            return;
        }
        onFetching("");
        onDone([arrayBuffer, req]);
    };
    req.onprogress = (event) => {
        const progress = event.loaded / event.total;
        onFetching(`${inGamePath} ${Math.floor(progress * 100)}%`);
    };
    req.onerror = (event) => {
        console.info(`error`, event);
        onFetching(`${inGamePath} retrying...`);
        setTimeout(() => {
            fetchWithRetry(fullUrl, inGamePath, onFetching, onDone);
        }, 10000);
    };

    req.send();
}

const ASYNCFETCHFS = {
    DIR_MODE: S_IFDIR | 511 /* 0777 */,
    FILE_MODE: S_IFREG | 511 /* 0777 */,
    reader: null,

    // TODO: Options below should be part of `mount` options

    /** Replace with with your function to be notified about progress */
    onFetching: () => {},

    pathPrefix: "",

    useGzip: false,

    mount: function (mount) {
        if (ASYNCFETCHFS.useGzip && typeof pako === "undefined") {
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
        // We also accept FileList here, by using Array.prototype
        Array.prototype.forEach.call(
            mount.opts["files"] || [],
            function (file) {
                ASYNCFETCHFS.createNode(
                    ensureParent(file.name),
                    base(file.name),
                    ASYNCFETCHFS.FILE_MODE,
                    0,
                    file.size
                    // file.lastModifiedDate
                );
            }
        );
        return root;
    },
    createNode: function (parent, name, mode, dev, fileSize, mtime) {
        var node = FS.createNode(parent, name, mode);
        node.mode = mode;
        node.node_ops = ASYNCFETCHFS.node_ops;
        node.stream_ops = ASYNCFETCHFS.stream_ops;
        node.timestamp = (mtime || new Date()).getTime();
        assert(ASYNCFETCHFS.FILE_MODE !== ASYNCFETCHFS.DIR_MODE);
        if (mode === ASYNCFETCHFS.FILE_MODE) {
            node.size = fileSize;
            node.contents = null;
        } else {
            node.size = 4096;
            node.contents = {};
        }
        if (parent) {
            parent.contents[name] = node;
        }
        return node;
    },
    node_ops: {
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
            console.info(`ASYNCFETCHFS node_ops.mknod`, name, parent);
            throw new FS.ErrnoError(EPERM);
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
        readdir: function (node) {
            var entries = [".", ".."];
            for (var key in node.contents) {
                if (!node.contents.hasOwnProperty(key)) {
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
        read: function (stream, buffer, offset, length, position) {
            if (position >= stream.node.size) return 0;

            const chunk = stream.node.contents.slice(
                position,
                position + length
            );
            buffer.set(new Uint8Array(chunk), offset);
            return chunk.byteLength;
        },
        write: function (stream, buffer, offset, length, position) {
            console.info(`ASYNCFETCHFS write`, stream);
            return length;
            // throw new FS.ErrnoError(EIO);
        },
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
        open: function (stream) {
            if (Asyncify.state == Asyncify.State.Normal) {
                if (stream.node.contents) {
                    return;
                }
            }

            const node = stream.node;

            // ___syscall_openat creates new stream everytime it runs
            // We need to close this stream when we run for the first time in async mode
            if (Asyncify.state == Asyncify.State.Normal) {
                FS.closeStream(stream.fd);
            }

            return Asyncify.handleSleep(function (wakeUp) {
                // TODO: Maybe we can release data from some files to save some memory

                let inGamePath = "";
                let searchNode = node;
                while (searchNode.name !== "/") {
                    inGamePath = "/" + searchNode.name + inGamePath;
                    searchNode = searchNode.parent;
                }
                inGamePath = inGamePath.slice(1);

                ASYNCFETCHFS.onFetching(inGamePath);

                const fullUrl =
                    ASYNCFETCHFS.pathPrefix +
                    inGamePath +
                    (ASYNCFETCHFS.useGzip ? ".gz" : "");

                fetchWithRetry(
                    fullUrl,
                    inGamePath,
                    ASYNCFETCHFS.onFetching,
                    ([data, req]) => {
                        // TODO: In some cases data is automatically unpacked by hosting
                        // Maybe change .gz suffix into something else, for example .gzzzz?

                        let unpackedData;
                        if (ASYNCFETCHFS.useGzip) {
                            try {
                                unpackedData = pako.inflate(data);
                            } catch {
                                ASYNCFETCHFS.onFetching(
                                    `Error unpacking ${inGamePath}`
                                );
                                return;
                            }
                        } else {
                            unpackedData = data;
                        }

                        ASYNCFETCHFS.onFetching(null);

                        node.contents = unpackedData;

                        wakeUp();
                    }
                );
            });
        },
    },
};
