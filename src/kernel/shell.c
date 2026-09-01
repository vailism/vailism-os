#include "../include/shell.h"
#include "../include/framebuffer.h"
#include "../include/serial.h"
#include "../include/string.h"
#include "../include/vfs.h"
#include "../include/pmm.h"
#include "../include/heap.h"
#include "../include/scheduler.h"
#include "../include/io.h"

#define MAX_LINE_LEN 256
#define MAX_ARGS     16

static char g_line_buffer[MAX_LINE_LEN];
static size_t g_line_len = 0;

static void shell_prompt(void) {
    fb_set_color(FB_COLOR_CYAN, FB_COLOR_BG);
    fb_puts("vailism-os> ");
    fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);

    serial_puts("\nvailism-os> ");
}

static void cmd_help(int argc, char **argv) {
    (void)argc;
    (void)argv;

    fb_set_color(FB_COLOR_YELLOW, FB_COLOR_BG);
    fb_puts("Vailism OS Built-in Commands:\n");
    fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);

    fb_puts("  help                - Display this command manual\n");
    fb_puts("  clear               - Clear the terminal screen\n");
    fb_puts("  echo <text>         - Print text to screen\n");
    fb_puts("  ls [path]           - List files and directories\n");
    fb_puts("  cat <path>          - Display contents of a file\n");
    fb_puts("  mkdir <path>        - Create a new directory\n");
    fb_puts("  touch <path>        - Create an empty file\n");
    fb_puts("  write <path> <text> - Write text into a file\n");
    fb_puts("  mem                 - Show memory allocation telemetry\n");
    fb_puts("  uname               - Display operating system release info\n");
    fb_puts("  reboot              - Restart the computer\n");
}

static void cmd_clear(int argc, char **argv) {
    (void)argc;
    (void)argv;
    fb_clear(FB_COLOR_BG);
}

static void cmd_echo(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        fb_puts(argv[i]);
        serial_puts(argv[i]);
        if (i < argc - 1) {
            fb_putchar(' ');
            serial_putchar(' ');
        }
    }
    fb_putchar('\n');
    serial_putchar('\n');
}

static void cmd_ls(int argc, char **argv) {
    const char *path = (argc > 1) ? argv[1] : "/";
    vfs_node_t *dir = vfs_lookup(path);
    if (!dir || dir->type != VFS_DIRECTORY) {
        fb_set_color(FB_COLOR_RED, FB_COLOR_BG);
        fb_puts("ls: cannot access '");
        fb_puts(path);
        fb_puts("': No such directory\n");
        fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
        return;
    }

    vfs_node_t *curr = dir->children;
    if (!curr) {
        fb_puts("(empty directory)\n");
        return;
    }

    while (curr) {
        if (curr->type == VFS_DIRECTORY) {
            fb_set_color(FB_COLOR_CYAN, FB_COLOR_BG);
            fb_puts("[DIR]  ");
        } else {
            fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
            fb_puts("[FILE] ");
        }
        fb_puts(curr->name);
        fb_puts("\n");
        curr = curr->next;
    }
    fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
}

static void cmd_cat(int argc, char **argv) {
    if (argc < 2) {
        fb_set_color(FB_COLOR_RED, FB_COLOR_BG);
        fb_puts("Usage: cat <path>\n");
        fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
        return;
    }

    int fd = vfs_open(argv[1], O_RDONLY);
    if (fd < 0) {
        fb_set_color(FB_COLOR_RED, FB_COLOR_BG);
        fb_puts("cat: ");
        fb_puts(argv[1]);
        fb_puts(": No such file\n");
        fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
        return;
    }

    char buffer[256];
    int64_t bytes;
    while ((bytes = vfs_read(fd, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes] = '\0';
        fb_puts(buffer);
        serial_puts(buffer);
    }
    vfs_close(fd);
    fb_putchar('\n');
}

static void cmd_mkdir(int argc, char **argv) {
    if (argc < 2) {
        fb_set_color(FB_COLOR_RED, FB_COLOR_BG);
        fb_puts("Usage: mkdir <path>\n");
        fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
        return;
    }

    if (!vfs_mkdir(argv[1])) {
        fb_set_color(FB_COLOR_RED, FB_COLOR_BG);
        fb_puts("mkdir: failed to create directory '");
        fb_puts(argv[1]);
        fb_puts("'\n");
        fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
    } else {
        fb_set_color(FB_COLOR_GREEN, FB_COLOR_BG);
        fb_puts("Directory created: ");
        fb_puts(argv[1]);
        fb_puts("\n");
        fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
    }
}

static void cmd_touch(int argc, char **argv) {
    if (argc < 2) {
        fb_set_color(FB_COLOR_RED, FB_COLOR_BG);
        fb_puts("Usage: touch <path>\n");
        fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
        return;
    }

    int fd = vfs_open(argv[1], O_CREAT | O_WRONLY);
    if (fd >= 0) {
        vfs_close(fd);
        fb_set_color(FB_COLOR_GREEN, FB_COLOR_BG);
        fb_puts("Created file: ");
        fb_puts(argv[1]);
        fb_puts("\n");
        fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
    } else {
        fb_set_color(FB_COLOR_RED, FB_COLOR_BG);
        fb_puts("touch: cannot create '");
        fb_puts(argv[1]);
        fb_puts("'\n");
        fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
    }
}

static void cmd_write(int argc, char **argv) {
    if (argc < 3) {
        fb_set_color(FB_COLOR_RED, FB_COLOR_BG);
        fb_puts("Usage: write <path> <text...>\n");
        fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
        return;
    }

    int fd = vfs_open(argv[1], O_CREAT | O_WRONLY | O_TRUNC);
    if (fd < 0) {
        fb_set_color(FB_COLOR_RED, FB_COLOR_BG);
        fb_puts("write: failed to open '");
        fb_puts(argv[1]);
        fb_puts("'\n");
        fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
        return;
    }

    for (int i = 2; i < argc; i++) {
        vfs_write(fd, argv[i], strlen(argv[i]));
        if (i < argc - 1) {
            vfs_write(fd, " ", 1);
        }
    }
    vfs_write(fd, "\n", 1);
    vfs_close(fd);

    fb_set_color(FB_COLOR_GREEN, FB_COLOR_BG);
    fb_puts("Wrote text into: ");
    fb_puts(argv[1]);
    fb_puts("\n");
    fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
}

static void print_number(uint64_t val) {
    char buf[32];
    int i = 0;
    if (val == 0) {
        fb_putchar('0');
        return;
    }
    while (val > 0) {
        buf[i++] = (char)('0' + (val % 10));
        val /= 10;
    }
    for (int j = i - 1; j >= 0; j--) {
        fb_putchar(buf[j]);
    }
}

static void cmd_mem(int argc, char **argv) {
    (void)argc;
    (void)argv;

    uint64_t total_mb = pmm_get_total_memory() / (1024 * 1024);
    uint64_t free_mb  = pmm_get_free_memory() / (1024 * 1024);
    uint64_t used_mb  = pmm_get_used_memory() / (1024 * 1024);

    fb_set_color(FB_COLOR_PURPLE, FB_COLOR_BG);
    fb_puts("System Memory Telemetry:\n");
    fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
    fb_puts("  Total Physical Memory: "); print_number(total_mb); fb_puts(" MB\n");
    fb_puts("  Used Physical Memory:  "); print_number(used_mb);  fb_puts(" MB\n");
    fb_puts("  Free Physical Memory:  "); print_number(free_mb);  fb_puts(" MB\n");
}

static void cmd_uname(int argc, char **argv) {
    (void)argc;
    (void)argv;
    fb_set_color(FB_COLOR_GREEN, FB_COLOR_BG);
    fb_puts("Vailism OS 0.6.0-alpha (x86_64 Long Mode Bare-Metal Kernel)\n");
    fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
}

static void cmd_reboot(int argc, char **argv) {
    (void)argc;
    (void)argv;
    fb_set_color(FB_COLOR_YELLOW, FB_COLOR_BG);
    fb_puts("Rebooting Vailism OS...\n");
    serial_puts("[REBOOT] Restarting machine...\n");
    outb(0x64, 0xFE);
    for (;;) { __asm__ volatile ("hlt"); }
}

void shell_execute_command(const char *cmdline) {
    if (!cmdline || strlen(cmdline) == 0) return;

    char buf[MAX_LINE_LEN];
    strncpy(buf, cmdline, MAX_LINE_LEN - 1);
    buf[MAX_LINE_LEN - 1] = '\0';

    char *argv[MAX_ARGS];
    int argc = 0;

    char *p = buf;
    while (*p && argc < MAX_ARGS) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;

        argv[argc++] = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        if (*p) {
            *p = '\0';
            p++;
        }
    }

    if (argc == 0) return;

    if (strcmp(argv[0], "help") == 0) {
        cmd_help(argc, argv);
    } else if (strcmp(argv[0], "clear") == 0) {
        cmd_clear(argc, argv);
    } else if (strcmp(argv[0], "echo") == 0) {
        cmd_echo(argc, argv);
    } else if (strcmp(argv[0], "ls") == 0) {
        cmd_ls(argc, argv);
    } else if (strcmp(argv[0], "cat") == 0) {
        cmd_cat(argc, argv);
    } else if (strcmp(argv[0], "mkdir") == 0) {
        cmd_mkdir(argc, argv);
    } else if (strcmp(argv[0], "touch") == 0) {
        cmd_touch(argc, argv);
    } else if (strcmp(argv[0], "write") == 0) {
        cmd_write(argc, argv);
    } else if (strcmp(argv[0], "mem") == 0) {
        cmd_mem(argc, argv);
    } else if (strcmp(argv[0], "uname") == 0) {
        cmd_uname(argc, argv);
    } else if (strcmp(argv[0], "reboot") == 0) {
        cmd_reboot(argc, argv);
    } else {
        fb_set_color(FB_COLOR_RED, FB_COLOR_BG);
        fb_puts("Unknown command: '");
        fb_puts(argv[0]);
        fb_puts("'. Type 'help' for available commands.\n");
        fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
    }
}

void shell_handle_char(char c) {
    if (c == '\r' || c == '\n') {
        fb_putchar('\n');
        serial_putchar('\n');

        g_line_buffer[g_line_len] = '\0';
        if (g_line_len > 0) {
            shell_execute_command(g_line_buffer);
            g_line_len = 0;
        }
        shell_prompt();
        return;
    }

    if (c == '\b') {
        if (g_line_len > 0) {
            g_line_len--;
            fb_putchar('\b');
            serial_puts("\b \b");
        }
        return;
    }

    if (c >= 32 && c <= 126) {
        if (g_line_len < MAX_LINE_LEN - 1) {
            g_line_buffer[g_line_len++] = c;
            fb_putchar(c);
            serial_putchar(c);
        }
    }
}

void shell_init(void) {
    g_line_len = 0;
    memset(g_line_buffer, 0, sizeof(g_line_buffer));
    shell_prompt();
    serial_puts("[SHELL] Interactive Shell initialized.\n");
}
