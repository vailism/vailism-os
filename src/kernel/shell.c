#include "../include/shell.h"
#include "../include/framebuffer.h"
#include "../include/serial.h"
#include "../include/string.h"
#include "../include/vfs.h"
#include "../include/pmm.h"
#include "../include/heap.h"
#include "../include/scheduler.h"
#include "../include/timer.h"
#include "../include/gui.h"
#include "../include/io.h"

#define MAX_LINE_LEN 256
#define MAX_ARGS     16
#define MAX_HISTORY  8

static char g_line_buffer[MAX_LINE_LEN];
static size_t g_line_len = 0;

// Command history
static char g_history[MAX_HISTORY][MAX_LINE_LEN];
static int  g_history_count = 0;
static int  g_history_index __attribute__((unused)) = -1;

static void history_push(const char *cmd) {
    if (g_history_count < MAX_HISTORY) {
        strncpy(g_history[g_history_count], cmd, MAX_LINE_LEN - 1);
        g_history[g_history_count][MAX_LINE_LEN - 1] = '\0';
        g_history_count++;
    } else {
        // Shift up
        for (int i = 0; i < MAX_HISTORY - 1; i++) {
            strcpy(g_history[i], g_history[i + 1]);
        }
        strncpy(g_history[MAX_HISTORY - 1], cmd, MAX_LINE_LEN - 1);
    }
    g_history_index = -1;
}

static void shell_prompt(void) {
    fb_set_color(FB_COLOR_CYAN, FB_COLOR_BG);
    fb_puts("vailism-os");
    fb_set_color(FB_COLOR_GREEN, FB_COLOR_BG);
    fb_puts(":/ ");
    fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);

    serial_puts("\nvailism-os:/ ");
}

// ─── Built-in Commands ───────────────────────────────────────────────────────

static void cmd_help(int argc, char **argv) {
    (void)argc;
    (void)argv;

    fb_set_color(FB_COLOR_YELLOW, FB_COLOR_BG);
    fb_puts("Vailism OS Command Reference:\n");
    fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);

    fb_set_color(FB_COLOR_CYAN, FB_COLOR_BG);
    fb_puts("\n  Navigation & Files:\n");
    fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
    fb_puts("    ls [path]           - List directory contents\n");
    fb_puts("    cat <path>          - Display file contents\n");
    fb_puts("    mkdir <path>        - Create a new directory\n");
    fb_puts("    touch <path>        - Create an empty file\n");
    fb_puts("    write <path> <text> - Write text into a file\n");

    fb_set_color(FB_COLOR_CYAN, FB_COLOR_BG);
    fb_puts("\n  System Information:\n");
    fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
    fb_puts("    uname               - Display OS release info\n");
    fb_puts("    mem                 - Physical memory telemetry\n");
    fb_puts("    ps                  - List active kernel threads\n");
    fb_puts("    uptime              - System uptime since boot\n");

    fb_set_color(FB_COLOR_CYAN, FB_COLOR_BG);
    fb_puts("\n  Desktop & System:\n");
    fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
    fb_puts("    gui                 - Launch Desktop GUI & Window Manager\n");
    fb_puts("    clear               - Clear the terminal screen\n");
    fb_puts("    echo <text>         - Print text to screen\n");
    fb_puts("    help                - Display this command reference\n");
    fb_puts("    reboot              - Restart the computer\n");
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
        fb_set_color(FB_COLOR_MUTED, FB_COLOR_BG);
        fb_puts("(empty directory)\n");
        fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
        return;
    }

    int count = 0;
    while (curr) {
        if (curr->type == VFS_DIRECTORY) {
            fb_set_color(FB_COLOR_CYAN, FB_COLOR_BG);
            fb_puts("[DIR]  ");
        } else {
            fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
            fb_puts("[FILE] ");
        }
        fb_puts(curr->name);

        // Show file size
        if (curr->type == VFS_FILE) {
            char size_buf[32];
            fb_set_color(FB_COLOR_MUTED, FB_COLOR_BG);
            ksnprintf(size_buf, sizeof(size_buf), "  (%u bytes)", (uint64_t)curr->size);
            fb_puts(size_buf);
        }

        fb_puts("\n");
        curr = curr->next;
        count++;
    }
    fb_set_color(FB_COLOR_MUTED, FB_COLOR_BG);
    char count_buf[32];
    ksnprintf(count_buf, sizeof(count_buf), "  %d entries total\n", (int64_t)count);
    fb_puts(count_buf);
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
        fb_puts("mkdir: failed to create '");
        fb_puts(argv[1]);
        fb_puts("'\n");
        fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
    } else {
        fb_set_color(FB_COLOR_GREEN, FB_COLOR_BG);
        fb_puts("Created: ");
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
        fb_puts("Created: ");
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
    fb_puts("Wrote to: ");
    fb_puts(argv[1]);
    fb_puts("\n");
    fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
}

static void cmd_mem(int argc, char **argv) {
    (void)argc;
    (void)argv;

    uint64_t total = pmm_get_total_memory();
    uint64_t free  = pmm_get_free_memory();
    uint64_t used  = pmm_get_used_memory();
    char buf[128];

    fb_set_color(FB_COLOR_PURPLE, FB_COLOR_BG);
    fb_puts("Physical Memory:\n");
    fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);

    ksnprintf(buf, sizeof(buf), "  Total: %u MB (%u bytes)\n", (uint64_t)(total / (1024 * 1024)), total);
    fb_puts(buf);
    ksnprintf(buf, sizeof(buf), "  Used:  %u MB (%u bytes)\n", (uint64_t)(used / (1024 * 1024)), used);
    fb_puts(buf);
    ksnprintf(buf, sizeof(buf), "  Free:  %u MB (%u bytes)\n", (uint64_t)(free / (1024 * 1024)), free);
    fb_puts(buf);

    // Visual bar
    uint32_t bar_len = 40;
    uint32_t used_len = (total > 0) ? (uint32_t)((used * bar_len) / total) : 0;
    if (used_len < 1) used_len = 1;

    fb_set_color(FB_COLOR_MUTED, FB_COLOR_BG);
    fb_puts("  [");
    for (uint32_t i = 0; i < bar_len; i++) {
        if (i < used_len) {
            fb_set_color(FB_COLOR_CYAN, FB_COLOR_BG);
            fb_putchar('#');
        } else {
            fb_set_color(FB_COLOR_MUTED, FB_COLOR_BG);
            fb_putchar('-');
        }
    }
    fb_set_color(FB_COLOR_MUTED, FB_COLOR_BG);
    fb_puts("]\n");
    fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
}

static void cmd_ps(int argc, char **argv) {
    (void)argc;
    (void)argv;

    fb_set_color(FB_COLOR_PURPLE, FB_COLOR_BG);
    fb_puts("Active Kernel Threads:\n");
    fb_set_color(FB_COLOR_MUTED, FB_COLOR_BG);
    fb_puts("  TID  STATE       NAME\n");
    fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);

    // Access thread table via scheduler (we expose get_current_thread)
    // For now display what we know about the main and worker threads
    thread_t *current = get_current_thread();
    if (current) {
        char buf[128];
        ksnprintf(buf, sizeof(buf), "  %u    RUNNING     %s (current)\n",
                  (uint64_t)current->tid, current->name);
        fb_set_color(FB_COLOR_GREEN, FB_COLOR_BG);
        fb_puts(buf);
        fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
    }

    fb_set_color(FB_COLOR_MUTED, FB_COLOR_BG);
    fb_puts("  (Only current thread visible in this view)\n");
    fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
}

static void cmd_uptime(int argc, char **argv) {
    (void)argc;
    (void)argv;

    uint64_t ticks = timer_get_ticks();
    uint64_t total_secs = ticks / 100;
    uint64_t hours = total_secs / 3600;
    uint64_t mins  = (total_secs % 3600) / 60;
    uint64_t secs  = total_secs % 60;
    char buf[64];

    fb_set_color(FB_COLOR_GREEN, FB_COLOR_BG);
    ksnprintf(buf, sizeof(buf), "Uptime: %uh %um %us (%u ticks @ 100 Hz)\n",
              (uint64_t)hours, (uint64_t)mins, (uint64_t)secs, (uint64_t)ticks);
    fb_puts(buf);
    fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
}

static void cmd_uname(int argc, char **argv) {
    (void)argc;
    (void)argv;
    fb_set_color(FB_COLOR_GREEN, FB_COLOR_BG);
    fb_puts("Vailism OS 0.7.0-desktop (x86_64 Long Mode Bare-Metal Kernel)\n");
    fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
}

static void cmd_gui(int argc, char **argv) {
    (void)argc;
    (void)argv;
    fb_set_color(FB_COLOR_CYAN, FB_COLOR_BG);
    fb_puts("Launching Desktop GUI Compositor...\n");
    gui_run_desktop();
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

// ─── Command Dispatcher ─────────────────────────────────────────────────────

void shell_execute_command(const char *cmdline) {
    if (!cmdline || strlen(cmdline) == 0) return;

    history_push(cmdline);

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
    } else if (strcmp(argv[0], "gui") == 0) {
        cmd_gui(argc, argv);
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
    } else if (strcmp(argv[0], "ps") == 0) {
        cmd_ps(argc, argv);
    } else if (strcmp(argv[0], "uptime") == 0) {
        cmd_uptime(argc, argv);
    } else if (strcmp(argv[0], "uname") == 0) {
        cmd_uname(argc, argv);
    } else if (strcmp(argv[0], "reboot") == 0) {
        cmd_reboot(argc, argv);
    } else {
        fb_set_color(FB_COLOR_RED, FB_COLOR_BG);
        fb_puts("Command not found: '");
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
    g_history_count = 0;
    g_history_index = -1;
    memset(g_line_buffer, 0, sizeof(g_line_buffer));

    fb_set_color(FB_COLOR_YELLOW, FB_COLOR_BG);
    fb_puts("Welcome to Vailism OS Interactive Shell\n");
    fb_set_color(FB_COLOR_MUTED, FB_COLOR_BG);
    fb_puts("Type 'help' for available commands, 'gui' for Desktop GUI\n\n");
    fb_set_color(FB_COLOR_WHITE, FB_COLOR_BG);

    shell_prompt();
    serial_puts("[SHELL] Interactive Shell initialized.\n");
}
