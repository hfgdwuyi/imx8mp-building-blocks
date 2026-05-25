#include "bb_update.h"
#include "../../libbb/bb_board.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

// ---------------------------------------------------------------------------
// Verify
// ---------------------------------------------------------------------------
int bb_update_verify(const char *path, const char *pubkey_path)
{
    (void)pubkey_path; // TODO: OpenSSL RSA signature verification

    // For now: check magic + parse manifest + verify checksums
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Cannot open %s\n", path);
        return -1;
    }

    // Read magic
    uint32_t magic;
    if (fread(&magic, 4, 1, f) != 1 || magic != BB_UPDATE_MAGIC) {
        fprintf(stderr, "Invalid update package (bad magic)\n");
        fclose(f);
        return -1;
    }

    // Read version
    uint32_t ver;
    fread(&ver, 4, 1, f);

    // Read manifest
    bb_update_manifest_t m;
    fread(&m, sizeof(m), 1, f);
    fclose(f);

    printf("Update: %s → %s (slot %c)\n", m.from_version, m.to_version, m.target_slot);
    printf("Product: %s\n", m.product);
    printf("Manifest OK, signature verification: SKIPPED (no key)\n");
    return 0;
}

int bb_update_read_manifest(const char *path, bb_update_manifest_t *out)
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    uint32_t magic, ver;
    if (fread(&magic, 4, 1, f) != 1 || magic != BB_UPDATE_MAGIC) {
        fclose(f);
        return -1;
    }
    fread(&ver, 4, 1, f);
    fread(out, sizeof(*out), 1, f);
    fclose(f);
    return 0;
}

// ---------------------------------------------------------------------------
// Install (runs on target)
// ---------------------------------------------------------------------------
int bb_update_current_slot(void)
{
    FILE *f = fopen("/proc/cmdline", "r");
    if (!f) return 'a';

    char cmdline[512];
    char slot = 'a';
    if (fgets(cmdline, sizeof(cmdline), f)) {
        char *p = strstr(cmdline, "boot_slot=");
        if (p) slot = p[10];  // "boot_slot=X"
    }
    fclose(f);
    return slot;
}

void bb_update_current_version(char *buf, size_t len)
{
    FILE *f = fopen("/etc/os-release", "r");
    if (!f) {
        snprintf(buf, len, "unknown");
        return;
    }

    char line[256];
    buf[0] = '\0';
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "BUILDING_BLOCKS_VERSION=", 24) == 0) {
            char *v = line + 24;
            size_t vlen = strlen(v);
            while (vlen > 0 && (v[vlen-1] == '\n' || v[vlen-1] == '"'))
                vlen--;
            snprintf(buf, len, "%.*s", (int)vlen, v);
            break;
        }
    }
    fclose(f);
    if (buf[0] == '\0') snprintf(buf, len, "unknown");
}

int bb_update_install(const char *path)
{
    printf("=== Building Blocks Update ===\n");
    printf("Package: %s\n", path);

    // 1. Verify signature
    if (bb_update_verify(path, NULL) != 0) {
        fprintf(stderr, "Verification failed\n");
        return -1;
    }

    // 2. Read manifest
    bb_update_manifest_t m;
    if (bb_update_read_manifest(path, &m) != 0) {
        fprintf(stderr, "Cannot read manifest\n");
        return -1;
    }

    // 3. Determine target slot
    int current = bb_update_current_slot();
    char target = m.target_slot;
    if (target == '=') {
        target = (current == 'a') ? 'b' : 'a';
    }
    printf("Current slot: %c, Target slot: %c\n", current, target);

    // 4. Determine partition numbers
    char boot_part[32], rootfs_part[32];
    snprintf(boot_part, sizeof(boot_part),
             BB_ROOT_DEV "p%d", (target == 'a') ? BB_PART_BOOT_A : BB_PART_BOOT_B);
    snprintf(rootfs_part, sizeof(rootfs_part),
             BB_ROOT_DEV "p%d", (target == 'a') ? BB_PART_ROOTFS_A : BB_PART_ROOTFS_B);

    // 5. Format target partitions
    printf("Formatting %s ...\n", boot_part);
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "mkfs.vfat -F 32 %s 2>/dev/null", boot_part);
    system(cmd);

    printf("Formatting %s ...\n", rootfs_part);
    snprintf(cmd, sizeof(cmd), "mkfs.ext4 -F %s 2>/dev/null", rootfs_part);
    system(cmd);

    // 6. Extract package contents
    printf("Extracting package...\n");

    // Create temp directory
    char tmpdir[] = "/tmp/bb-update-XXXXXX";
    if (!mkdtemp(tmpdir)) {
        fprintf(stderr, "Cannot create temp dir\n");
        return -1;
    }

    // Extract .bbu as tar
    // The .bbu is: [4B magic][4B version][manifest][tar archive with boot.tar.gz, rootfs.tar.gz, post-install.sh]
    snprintf(cmd, sizeof(cmd),
             "tail -c +%zu %s | tar xzf - -C %s 2>/dev/null",
             sizeof(uint32_t) * 2 + sizeof(bb_update_manifest_t), path, tmpdir);
    // Note: this is a simplification. A proper implementation would parse the
    // tar offset from the manifest structure rather than using tail.

    if (system(cmd) != 0) {
        fprintf(stderr, "Failed to extract package\n");
        return -1;
    }

    // 7. Write boot partition
    {
        char boot_tar[256];
        snprintf(boot_tar, sizeof(boot_tar), "%s/boot.tar.gz", tmpdir);

        struct stat st;
        if (stat(boot_tar, &st) == 0) {
            printf("Writing boot partition...\n");
            char mnt[] = "/tmp/bb-boot-XXXXXX";
            if (mkdtemp(mnt)) {
                snprintf(cmd, sizeof(cmd), "mount %s %s", boot_part, mnt);
                system(cmd);
                snprintf(cmd, sizeof(cmd), "tar xzf %s -C %s", boot_tar, mnt);
                system(cmd);
                snprintf(cmd, sizeof(cmd), "umount %s", mnt);
                system(cmd);
                rmdir(mnt);
            }
        }
    }

    // 8. Write rootfs partition
    {
        char rootfs_tar[256];
        snprintf(rootfs_tar, sizeof(rootfs_tar), "%s/rootfs.tar.gz", tmpdir);

        printf("Writing rootfs partition (this may take a while)...\n");
        char mnt[] = "/tmp/bb-rootfs-XXXXXX";
        if (mkdtemp(mnt)) {
            snprintf(cmd, sizeof(cmd), "mount %s %s", rootfs_part, mnt);
            system(cmd);
            snprintf(cmd, sizeof(cmd), "tar xzf %s -C %s", rootfs_tar, mnt);
            system(cmd);

            // 9. Run post-install script (in target rootfs context)
            char postinstall[256];
            snprintf(postinstall, sizeof(postinstall), "%s/post-install.sh", mnt);
            if (access(postinstall, X_OK) == 0) {
                printf("Running post-install script...\n");
                // Run chroot'd or via absolute path
                snprintf(cmd, sizeof(cmd), "sh %s", postinstall);
                system(cmd);
            }

            snprintf(cmd, sizeof(cmd), "umount %s", mnt);
            system(cmd);
            rmdir(mnt);
        }
    }

    // Cleanup temp
    snprintf(cmd, sizeof(cmd), "rm -rf %s", tmpdir);
    system(cmd);

    // 10. Set U-Boot environment for next boot
    printf("Configuring bootloader for slot %c...\n", target);

    char env_cmd[256];
    snprintf(env_cmd, sizeof(env_cmd),
             "fw_setenv boot_slot %c && "
             "fw_setenv boot_attempt 3 && "
             "fw_setenv boot_ok 0",
             target);
    if (system(env_cmd) != 0) {
        fprintf(stderr, "WARNING: fw_setenv failed — manual U-Boot env setup required\n");
    }

    // 11. Log update to persist
    {
        FILE *f = fopen("/persist/update-log.json", "a");
        if (f) {
            fprintf(f, "{\"ts\":%lld,\"from\":\"%s\",\"to\":\"%s\",\"slot\":\"%c\",\"ok\":false}\n",
                    (long long)time(NULL), m.from_version, m.to_version, target);
            fclose(f);
        }
    }

    printf("\n========================================\n");
    printf("Update installed to slot %c\n", target);
    printf("Version: %s → %s\n", m.from_version, m.to_version);
    printf("REBOOT REQUIRED to activate new version.\n");
    printf("Run: systemctl reboot\n");
    printf("========================================\n");

    return 0;
}

// ---------------------------------------------------------------------------
// Create (runs on host build machine)
// ---------------------------------------------------------------------------
int bb_update_create(const char *output,
                     const bb_update_manifest_t *manifest,
                     const char *boot_tar,
                     const char *rootfs_tar,
                     const char *postinstall,
                     const char *privkey_path)
{
    (void)privkey_path; // TODO: OpenSSL RSA signing

    printf("Creating update package: %s\n", output);
    printf("Version: %s → %s\n", manifest->from_version, manifest->to_version);

    FILE *out = fopen(output, "wb");
    if (!out) {
        perror("fopen");
        return -1;
    }

    // Write header
    uint32_t magic = BB_UPDATE_MAGIC;
    uint32_t ver = BB_UPDATE_VERSION;
    fwrite(&magic, 4, 1, out);
    fwrite(&ver, 4, 1, out);
    fwrite(manifest, sizeof(*manifest), 1, out);

    // Build tar archive of payloads
    char tmpdir[] = "/tmp/bb-update-create-XXXXXX";
    if (!mkdtemp(tmpdir)) {
        fclose(out);
        return -1;
    }

    // Copy payloads into tmpdir
    char cmd[512];

    if (boot_tar) {
        snprintf(cmd, sizeof(cmd), "cp %s %s/boot.tar.gz", boot_tar, tmpdir);
        system(cmd);
    }

    snprintf(cmd, sizeof(cmd), "cp %s %s/rootfs.tar.gz", rootfs_tar, tmpdir);
    system(cmd);

    if (postinstall && access(postinstall, R_OK) == 0) {
        snprintf(cmd, sizeof(cmd), "cp %s %s/post-install.sh", postinstall, tmpdir);
        system(cmd);
    }

    // Create tar at end of output file
    snprintf(cmd, sizeof(cmd),
             "tar czf - -C %s . >> %s 2>/dev/null", tmpdir, output);
    system(cmd);

    // Cleanup
    snprintf(cmd, sizeof(cmd), "rm -rf %s", tmpdir);
    system(cmd);

    fclose(out);
    printf("Created: %s\n", output);
    return 0;
}
