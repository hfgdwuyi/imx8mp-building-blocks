#include "bb_persist.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static char get_boot_slot(void)
{
    FILE *f = fopen("/proc/cmdline", "r");
    if (!f) return 'a';

    char cmdline[512], slot = 'a';
    if (fgets(cmdline, sizeof(cmdline), f)) {
        char *p = strstr(cmdline, "boot_slot=");
        if (p) slot = p[10];
    }
    fclose(f);
    return slot;
}

int main(void)
{
    // 1. Mount /persist partition
    if (bb_persist_init() != 0) {
        fprintf(stderr, "bb-boot-ok: cannot init /persist\n");
        // Non-fatal on stock images without persist partition
    }

    // 2. Read kernel uptime — this IS the startup time since kernel boots at t=0
    FILE *f = fopen("/proc/uptime", "r");
    if (!f) {
        fprintf(stderr, "bb-boot-ok: cannot open /proc/uptime\n");
        return 1;
    }
    double uptime_sec;
    if (fscanf(f, "%lf", &uptime_sec) != 1) {
        fclose(f);
        return 1;
    }
    fclose(f);

    // 3. Determine current slot
    char slot = get_boot_slot();

    // 4. Record boot entry to persist partition
    bb_boot_entry_t entry = {
        .timestamp   = (uint64_t)time(NULL),
        .slot        = slot,
        .success     = 1,
        .boot_time_ms = (uint32_t)(uptime_sec * 1000.0),
    };
    bb_persist_boot_log_add(&entry);

    // 5. Tell U-Boot this was a good boot (A/B fallback reset)
    system("fw_setenv boot_ok 1 2>/dev/null");

    printf("Boot OK: slot=%c time=%.3fs\n", slot, uptime_sec);
    return 0;
}
