#include "../include/vfs.h"
#include "../include/heap.h"
#include "../include/string.h"
#include "../include/serial.h"
#include "../include/framebuffer.h"

static vfs_node_t *g_vfs_root = NULL;
static file_descriptor_t g_fd_table[MAX_FDS];
static uint64_t g_next_inode = 1;

static int64_t ramfs_read(vfs_node_t *node, uint64_t offset, size_t size, uint8_t *buffer) {
    if (!node || !buffer) return -1;
    if (offset >= node->size) return 0; // EOF

    size_t to_read = size;
    if (offset + to_read > node->size) {
        to_read = node->size - offset;
    }

    if (node->content) {
        memcpy(buffer, node->content + offset, to_read);
    }
    return (int64_t)to_read;
}

static int64_t ramfs_write(vfs_node_t *node, uint64_t offset, size_t size, const uint8_t *buffer) {
    if (!node || !buffer) return -1;

    size_t required_capacity = offset + size;
    if (required_capacity > node->capacity) {
        size_t new_cap = required_capacity < 64 ? 64 : required_capacity * 2;
        uint8_t *new_content = (uint8_t *)krealloc(node->content, new_cap);
        if (!new_content) return -1;

        node->content = new_content;
        node->capacity = new_cap;
    }

    memcpy(node->content + offset, buffer, size);
    if (offset + size > node->size) {
        node->size = offset + size;
    }
    return (int64_t)size;
}

static vfs_node_t *ramfs_finddir(vfs_node_t *node, const char *name) {
    if (!node || node->type != VFS_DIRECTORY) return NULL;

    vfs_node_t *curr = node->children;
    while (curr) {
        if (strcmp(curr->name, name) == 0) {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

static vfs_node_t *create_vfs_node(const char *name, uint32_t type) {
    vfs_node_t *node = (vfs_node_t *)kmalloc(sizeof(vfs_node_t));
    if (!node) return NULL;

    memset(node, 0, sizeof(vfs_node_t));
    strncpy(node->name, name, 127);
    node->type = type;
    node->inode = g_next_inode++;
    node->read = ramfs_read;
    node->write = ramfs_write;
    node->finddir = ramfs_finddir;

    return node;
}

vfs_node_t *vfs_lookup(const char *path) {
    if (!path || path[0] != '/') return NULL;
    if (strcmp(path, "/") == 0) return g_vfs_root;

    vfs_node_t *curr = g_vfs_root;
    char temp[MAX_PATH_LENGTH];
    strncpy(temp, path, MAX_PATH_LENGTH - 1);
    temp[MAX_PATH_LENGTH - 1] = '\0';

    char *token = temp + 1; // Skip initial '/'
    char *next_slash;

    while (*token) {
        next_slash = token;
        while (*next_slash && *next_slash != '/') {
            next_slash++;
        }

        bool is_end = (*next_slash == '\0');
        *next_slash = '\0';

        if (strlen(token) > 0) {
            vfs_node_t *child = curr->finddir(curr, token);
            if (!child) return NULL;
            curr = child;
        }

        if (is_end) break;
        token = next_slash + 1;
    }

    return curr;
}

static void add_child_node(vfs_node_t *parent, vfs_node_t *child) {
    child->parent = parent;
    if (!parent->children) {
        parent->children = child;
    } else {
        vfs_node_t *curr = parent->children;
        while (curr->next) {
            curr = curr->next;
        }
        curr->next = child;
    }
}

vfs_node_t *vfs_mkdir(const char *path) {
    if (!path || path[0] != '/') return NULL;

    // Separate parent path and dir name
    char temp[MAX_PATH_LENGTH];
    strncpy(temp, path, MAX_PATH_LENGTH - 1);
    temp[MAX_PATH_LENGTH - 1] = '\0';

    // Remove trailing slash if present
    size_t len = strlen(temp);
    if (len > 1 && temp[len - 1] == '/') {
        temp[len - 1] = '\0';
    }

    char *last_slash = NULL;
    for (int i = (int)strlen(temp) - 1; i >= 0; i--) {
        if (temp[i] == '/') {
            last_slash = &temp[i];
            break;
        }
    }

    if (!last_slash) return NULL;

    const char *dir_name = last_slash + 1;
    vfs_node_t *parent = NULL;

    if (last_slash == temp) {
        parent = g_vfs_root;
    } else {
        *last_slash = '\0';
        parent = vfs_lookup(temp);
    }

    if (!parent || parent->type != VFS_DIRECTORY) return NULL;

    // Check if already exists
    vfs_node_t *existing = parent->finddir(parent, dir_name);
    if (existing) return existing;

    vfs_node_t *new_dir = create_vfs_node(dir_name, VFS_DIRECTORY);
    add_child_node(parent, new_dir);

    return new_dir;
}

vfs_node_t *vfs_create_file(const char *path, const void *initial_data, size_t size) {
    if (!path || path[0] != '/') return NULL;

    char temp[MAX_PATH_LENGTH];
    strncpy(temp, path, MAX_PATH_LENGTH - 1);
    temp[MAX_PATH_LENGTH - 1] = '\0';

    char *last_slash = NULL;
    for (int i = (int)strlen(temp) - 1; i >= 0; i--) {
        if (temp[i] == '/') {
            last_slash = &temp[i];
            break;
        }
    }

    if (!last_slash) return NULL;

    const char *file_name = last_slash + 1;
    vfs_node_t *parent = NULL;

    if (last_slash == temp) {
        parent = g_vfs_root;
    } else {
        *last_slash = '\0';
        parent = vfs_lookup(temp);
    }

    if (!parent || parent->type != VFS_DIRECTORY) return NULL;

    vfs_node_t *new_file = create_vfs_node(file_name, VFS_FILE);
    if (initial_data && size > 0) {
        new_file->write(new_file, 0, size, (const uint8_t *)initial_data);
    }

    add_child_node(parent, new_file);
    return new_file;
}

int vfs_open(const char *path, int flags) {
    vfs_node_t *node = vfs_lookup(path);

    if (!node) {
        if (flags & O_CREAT) {
            node = vfs_create_file(path, NULL, 0);
            if (!node) return -1;
        } else {
            return -1;
        }
    }

    if (flags & O_TRUNC) {
        node->size = 0;
    }

    // Find free FD
    for (int i = 0; i < MAX_FDS; i++) {
        if (!g_fd_table[i].is_used) {
            g_fd_table[i].node = node;
            g_fd_table[i].offset = (flags & O_APPEND) ? node->size : 0;
            g_fd_table[i].flags = flags;
            g_fd_table[i].is_used = true;
            return i;
        }
    }

    return -1;
}

int64_t vfs_read(int fd, void *buffer, size_t count) {
    if (fd < 0 || fd >= MAX_FDS || !g_fd_table[fd].is_used) return -1;

    file_descriptor_t *f = &g_fd_table[fd];
    int64_t bytes = f->node->read(f->node, f->offset, count, (uint8_t *)buffer);
    if (bytes > 0) {
        f->offset += bytes;
    }
    return bytes;
}

int64_t vfs_write(int fd, const void *buffer, size_t count) {
    if (fd < 0 || fd >= MAX_FDS || !g_fd_table[fd].is_used) return -1;

    file_descriptor_t *f = &g_fd_table[fd];
    int64_t bytes = f->node->write(f->node, f->offset, count, (const uint8_t *)buffer);
    if (bytes > 0) {
        f->offset += bytes;
    }
    return bytes;
}

int vfs_close(int fd) {
    if (fd < 0 || fd >= MAX_FDS || !g_fd_table[fd].is_used) return -1;
    g_fd_table[fd].is_used = false;
    g_fd_table[fd].node = NULL;
    return 0;
}

void vfs_list_dir(const char *path) {
    vfs_node_t *dir = vfs_lookup(path);
    if (!dir || dir->type != VFS_DIRECTORY) {
        serial_puts("[VFS] Directory not found: ");
        serial_puts(path);
        serial_puts("\n");
        return;
    }

    serial_puts("\n--- Directory contents of ");
    serial_puts(path);
    serial_puts(" ---\n");

    vfs_node_t *curr = dir->children;
    while (curr) {
        if (curr->type == VFS_DIRECTORY) {
            serial_puts("  [DIR]  ");
        } else {
            serial_puts("  [FILE] ");
        }
        serial_puts(curr->name);
        serial_puts("\n");
        curr = curr->next;
    }
}

void vfs_init(void) {
    memset(g_fd_table, 0, sizeof(g_fd_table));

    // 1. Create root node '/'
    g_vfs_root = create_vfs_node("/", VFS_DIRECTORY);

    // 2. Create core root directories
    vfs_mkdir("/dev");
    vfs_mkdir("/etc");
    vfs_mkdir("/home");
    vfs_mkdir("/boot");

    // 3. Populate default system configuration files
    const char *os_release =
        "NAME=\"Vailism OS\"\n"
        "VERSION=\"0.5.0-alpha\"\n"
        "ID=vailism\n"
        "PRETTY_NAME=\"Vailism OS (x86_64)\"\n"
        "ARCH=\"x86_64\"\n"
        "KERNEL=\"Vailism Monolithic Core\"\n";
    vfs_create_file("/etc/os-release", os_release, strlen(os_release));

    const char *hostname = "vailism-box\n";
    vfs_create_file("/etc/hostname", hostname, strlen(hostname));

    const char *welcome = "Welcome to Vailism OS!\nStorage and Virtual Filesystem (VFS) are fully active.\n";
    vfs_create_file("/home/welcome.txt", welcome, strlen(welcome));

    serial_puts("[VFS] Virtual Filesystem initialized with in-memory root tree mounted.\n");
}
