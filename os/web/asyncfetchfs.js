const S_IFDIR = 0040000;
const S_IFREG = 0100000;
const ENOENT = 2;
const EPERM = 1;
const EIO = 5;
const SEEK_SET = 0;
const SEEK_CUR = 1;
const SEEK_END = 2;
const EINVAL = 22;

async function fetchWithRetry(url) {
    while (1) {
        const data = await fetch(url)
            .then((res) => res.arrayBuffer())
            .catch(() => null);
        if (data) {
            return data;
        }
        console.info(`Network retry ${url}`);
        await new Promise((r) => setTimeout(r, 1000));
    }
}

const ASYNCFETCHFS = {
    DIR_MODE: S_IFDIR | 511 /* 0777 */,
    FILE_MODE: S_IFREG | 511 /* 0777 */,
    reader: null,

    /** Replace with with your function to be notified about progress */
    onFetching: null,

    mount: function (mount) {
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

            return Asyncify.handleSleep(function (wakeUp) {
                // TODO: Maybe we can release data from some files

                let fullPath = "";
                let node = stream.node;
                while (node.name !== "/") {
                    fullPath = "/" + node.name + fullPath;
                    node = node.parent;
                }

                if (ASYNCFETCHFS.onFetching) {
                    ASYNCFETCHFS.onFetching(fullPath);
                }

                fetchWithRetry(fullPath).then((data) => {
                    if (ASYNCFETCHFS.onFetching) {
                        ASYNCFETCHFS.onFetching(null);
                    }

                    stream.node.contents = data;

                    wakeUp();
                });

                //stream.node.contents = new ArrayBuffer(stream.node.size);
                //const s = "lol=kek\n";
                //const view = new Uint8Array(stream.node.contents);
                //for (let i = 0; i < stream.node.size; i++) {
                //    view[i] = s.charCodeAt(i % s.length);
                //}
            });
        },
    },
};
