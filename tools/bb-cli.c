/*
 * bb-cli - Building Block CLI Tool
 *
 * Usage:
 *   bb-cli ping                     - Check bus status
 *   bb-cli pub <topic> <payload>    - Publish message
 *   bb-cli sub <topic>              - Subscribe and listen
 *   bb-cli led <cmd> [k v ...]      - Control LED block
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "bb_json.h"
#include "bb_persist.h"

#define BUS_PATH "/run/bb-bus.sock"

static int bus_connect(void) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return -1; }

    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    strncpy(addr.sun_path, BUS_PATH, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect"); close(fd); return -1;
    }
    return fd;
}

static int send_line(int fd, const char *line) {
    int len = strlen(line);
    char buf[8192];
    snprintf(buf, sizeof(buf), "%s\n", line);
    return send(fd, buf, strlen(buf), 0) > 0 ? 0 : -1;
}

static int cmd_ping(void) {
    int fd = bus_connect();
    if (fd < 0) return 1;

    send_line(fd, "PING");
    char resp[64];
    int n = recv(fd, resp, sizeof(resp) - 1, 0);
    close(fd);

    if (n > 0) {
        resp[n] = '\0';
        printf("Bus: %s", resp);
        return 0;
    }
    printf("Bus: no response\n");
    return 1;
}

static int cmd_pub(const char *topic, const char *payload) {
    int fd = bus_connect();
    if (fd < 0) return 1;

    char line[8192];
    snprintf(line, sizeof(line), "PUB %s %s", topic, payload);
    send_line(fd, line);
    close(fd);
    printf("Published to %s\n", topic);
    return 0;
}

static int cmd_sub(const char *topic) {
    int fd = bus_connect();
    if (fd < 0) return 1;

    char line[8192];
    snprintf(line, sizeof(line), "SUB %s", topic);
    send_line(fd, line);

    printf("Subscribed to %s. Listening... (Ctrl+C to stop)\n", topic);

    char buf[8192];
    int buf_len = 0;
    while (1) {
        int n = recv(fd, buf + buf_len, sizeof(buf) - 1 - buf_len, 0);
        if (n <= 0) break;
        buf_len += n;
        buf[buf_len] = '\0';

        char *start = buf;
        char *nl;
        while ((nl = strchr(start, '\n')) != NULL) {
            *nl = '\0';
            if (strncmp(start, "PUB ", 4) == 0) {
                char *t = start + 4;
                char *p = strchr(t, ' ');
                if (p) {
                    *p = '\0';
                    printf("[%s] %s\n", t, p + 1);
                } else {
                    printf("%s\n", start);
                }
            } else {
                printf("%s\n", start);
            }
            start = nl + 1;
        }
        if (start > buf) {
            buf_len = buf + buf_len - start;
            memmove(buf, start, buf_len);
            buf[buf_len] = '\0';
        }
    }
    close(fd);
    return 0;
}

static int cmd_led(int argc, char *argv[]) {
    if (argc < 1) {
        fprintf(stderr, "Usage: bb-cli led <cmd> [key val ...]\n");
        return 1;
    }

    char payload[4096];
    bb_json_writer_t w;
    bb_json_init(&w, payload, sizeof(payload));
    bb_json_start_object(&w);
    bb_json_add_string(&w, "cmd", argv[0]);
    for (int i = 1; i + 1 < argc; i += 2) {
        // Try int first
        char *end;
        long v = strtol(argv[i + 1], &end, 10);
        if (*end == '\0') {
            bb_json_add_int(&w, argv[i], (int)v);
        } else {
            bb_json_add_string(&w, argv[i], argv[i + 1]);
        }
    }
    bb_json_end_object(&w);

    return cmd_pub("/dev/bb-led/cmd", payload);
}

static int cmd_boot_log(int argc, char *argv[]) {
    // Default: show last 10 entries
    int count = 10;
    if (argc >= 1) count = atoi(argv[0]);
    if (count <= 0) count = 10;
    if (count > 256) count = 256;

    bb_persist_init(); // non-fatal if /persist not mounted

    bb_boot_entry_t entries[256];
    int n = bb_persist_boot_log_read(entries, count);

    if (n == 0) {
        printf("No boot records found.\n");
        return 0;
    }

    printf("Boot History (last %d entries)\n", n);
    printf("%-4s %-20s %-4s %-10s %s\n", "#", "Timestamp", "Slot", "Result", "Boot Time");
    printf("---- -------------------- ---- ---------- ----------\n");

    for (int i = 0; i < n; i++) {
        char timebuf[32];
        time_t ts = (time_t)entries[i].timestamp;
        struct tm tm;
        localtime_r(&ts, &tm);
        strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", &tm);

        double boot_s = entries[i].boot_time_ms / 1000.0;
        printf("%-4d %-20s %-4c %-10s %8.3fs\n",
               i + 1, timebuf,
               entries[i].slot,
               entries[i].success ? "OK" : "FAIL",
               boot_s);
    }
    return 0;
}

static void usage(void) {
    printf("bb-cli - Building Block CLI\n\n"
           "Usage:\n"
           "  bb-cli ping                  Check bus status\n"
           "  bb-cli pub <topic> <json>    Publish message\n"
           "  bb-cli sub <topic>           Subscribe to topic\n"
           "  bb-cli led <cmd> [k v ...]   Control LED block\n"
           "  bb-cli boot-log [count]      Show boot time history\n\n"
           "LED commands:\n"
           "  bb-cli led blink on_ms 200 off_ms 200 count 5\n"
           "  bb-cli led solid state on\n"
           "  bb-cli led heartbeat\n"
           "  bb-cli led status\n"
           "  bb-cli led stop\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) { usage(); return 0; }

    if (strcmp(argv[1], "ping") == 0) {
        return cmd_ping();
    }
    else if (strcmp(argv[1], "pub") == 0 && argc >= 4) {
        return cmd_pub(argv[2], argv[3]);
    }
    else if (strcmp(argv[1], "sub") == 0 && argc >= 3) {
        return cmd_sub(argv[2]);
    }
    else if (strcmp(argv[1], "led") == 0 && argc >= 3) {
        return cmd_led(argc - 2, argv + 2);
    }
    else if (strcmp(argv[1], "boot-log") == 0) {
        return cmd_boot_log(argc - 2, argv + 2);
    }
    else {
        usage();
    }
    return 0;
}
