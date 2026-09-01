#ifndef VFS_H
#define VFS_H

#include "types.h"

#define VFS_FILE        0x01
#define VFS_DIRECTORY   0x02
#define VFS_CHARDEVICE  0x03
#define VFS_BLOCKDEVICE 0x04

#define O_RDONLY        0x00
#define O_WRONLY        0x01
#define O_RDWR          0x02
#define O_CREAT         0x04
#define O_TRUNC         0x08
#define O_APPEND        0x10

#define MAX_PATH_LENGTH 256
#define MAX_FDS         64

struct vfs_node;

typedef int64_t (*vfs_read_t)(struct vfs_node *node, uint64_t offset, size_t size, uint8_t *buffer);
typedef int64_t (*vfs_write_t)(struct vfs_node *node, uint64_t offset, size_t size, const uint8_t *buffer);
typedef struct vfs_node *(*vfs_finddir_t)(struct vfs_node *node, const char *name);

/**
 * Virtual Filesystem Inode Node
 */
typedef struct vfs_node {
    char                name[128];
    uint32_t            type;           // VFS_FILE, VFS_DIRECTORY, etc.
    uint64_t            size;           // File size in bytes
    uint64_t            inode;
    uint8_t            *content;        // Dynamic data buffer for in-memory files
    size_t              capacity;       // Buffer capacity

    vfs_read_t          read;
    vfs_write_t         write;
    vfs_finddir_t       finddir;

    struct vfs_node    *parent;
    struct vfs_node    *children;       // Linked list of sub-nodes if directory
    struct vfs_node    *next;           // Sibling pointer in directory list
} vfs_node_t;

/**
 * File Descriptor (open file tracking)
 */
typedef struct file_descriptor {
    vfs_node_t *node;
    uint64_t    offset;
    int         flags;
    bool        is_used;
} file_descriptor_t;

/**
 * Initialize Virtual Filesystem and mount root filesystem (/).
 */
void vfs_init(void);

/**
 * Open a file by path. Returns a non-negative file descriptor on success, or -1 on failure.
 */
int vfs_open(const char *path, int flags);

/**
 * Read up to 'count' bytes from open file descriptor into buffer.
 */
int64_t vfs_read(int fd, void *buffer, size_t count);

/**
 * Write up to 'count' bytes from buffer into open file descriptor.
 */
int64_t vfs_write(int fd, const void *buffer, size_t count);

/**
 * Close open file descriptor.
 */
int vfs_close(int fd);

/**
 * Create a new directory at the specified absolute path.
 */
vfs_node_t *vfs_mkdir(const char *path);

/**
 * Create a new regular file at the specified absolute path with optional initial content.
 */
vfs_node_t *vfs_create_file(const char *path, const void *initial_data, size_t size);

/**
 * Lookup a VFS node by its absolute path.
 */
vfs_node_t *vfs_lookup(const char *path);

/**
 * Print directory contents to serial console and screen.
 */
void vfs_list_dir(const char *path);

#endif // VFS_H
