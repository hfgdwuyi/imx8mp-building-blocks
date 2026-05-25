/*
 * bb-update — Building Blocks Update Tool
 *
 * Usage:
 *   bb-update verify <update.bbu>          Verify package signature and checksums
 *   bb-update install <update.bbu>         Install update to inactive slot
 *   bb-update create [options] <output>    Create a new update package
 *   bb-update slot                         Show current boot slot
 *   bb-update version                      Show current version
 */

#include "bb_update.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static void usage(void)
{
    printf("Usage:\n"
           "  bb-update verify   <update.bbu>            Verify package\n"
           "  bb-update install  <update.bbu>            Install to inactive slot\n"
           "  bb-update create   [options] <output.bbu>  Create update package\n"
           "  bb-update slot                             Show current boot slot\n"
           "  bb-update version                          Show current version\n"
           "\n"
           "Create options:\n"
           "  --from <version>     Current version\n"
           "  --to <version>       Target version\n"
           "  --slot <a|b>         Target slot (default: auto)\n"
           "  --product <name>     Product identifier\n"
           "  --boot <boot.tar.gz> Boot partition content\n"
           "  --rootfs <rootfs.tar.gz> Root filesystem content\n"
           "  --post <script.sh>   Post-install script\n"
           "  --sign <key.pem>     Signing private key\n");
}

int main(int argc, char *argv[])
{
    if (argc < 2) { usage(); return 1; }

    const char *cmd = argv[1];

    if (strcmp(cmd, "verify") == 0) {
        if (argc < 3) { usage(); return 1; }
        return bb_update_verify(argv[2], NULL) == 0 ? 0 : 1;
    }

    if (strcmp(cmd, "install") == 0) {
        if (argc < 3) { usage(); return 1; }
        return bb_update_install(argv[2]) == 0 ? 0 : 1;
    }

    if (strcmp(cmd, "slot") == 0) {
        printf("%c\n", bb_update_current_slot());
        return 0;
    }

    if (strcmp(cmd, "version") == 0) {
        char v[64];
        bb_update_current_version(v, sizeof(v));
        printf("%s\n", v);
        return 0;
    }

    if (strcmp(cmd, "create") == 0) {
        // Parse create options
        bb_update_manifest_t m;
        memset(&m, 0, sizeof(m));
        m.target_slot = '=';
        m.timestamp = time(NULL);

        const char *boot_tar = NULL;
        const char *rootfs_tar = NULL;
        const char *postinstall = NULL;
        const char *privkey = NULL;
        const char *output = NULL;

        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--from") == 0 && i + 1 < argc)
                snprintf(m.from_version, sizeof(m.from_version), "%s", argv[++i]);
            else if (strcmp(argv[i], "--to") == 0 && i + 1 < argc)
                snprintf(m.to_version, sizeof(m.to_version), "%s", argv[++i]);
            else if (strcmp(argv[i], "--slot") == 0 && i + 1 < argc)
                m.target_slot = argv[++i][0];
            else if (strcmp(argv[i], "--product") == 0 && i + 1 < argc)
                snprintf(m.product, sizeof(m.product), "%s", argv[++i]);
            else if (strcmp(argv[i], "--boot") == 0 && i + 1 < argc)
                boot_tar = argv[++i];
            else if (strcmp(argv[i], "--rootfs") == 0 && i + 1 < argc)
                rootfs_tar = argv[++i];
            else if (strcmp(argv[i], "--post") == 0 && i + 1 < argc)
                postinstall = argv[++i];
            else if (strcmp(argv[i], "--sign") == 0 && i + 1 < argc)
                privkey = argv[++i];
            else if (argv[i][0] != '-')
                output = argv[i];
        }

        if (!output || !rootfs_tar || !m.from_version[0] || !m.to_version[0]) {
            fprintf(stderr, "Missing required arguments: --from, --to, --rootfs, <output>\n");
            return 1;
        }

        return bb_update_create(output, &m, boot_tar, rootfs_tar, postinstall, privkey) == 0 ? 0 : 1;
    }

    usage();
    return 1;
}
