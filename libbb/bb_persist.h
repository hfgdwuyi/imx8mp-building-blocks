#ifndef BB_PERSIST_H
#define BB_PERSIST_H

#include <stddef.h>
#include <stdint.h>

// ---------------------------------------------------------------------------
// Persist partition (/persist) — survives A/B updates
// ---------------------------------------------------------------------------

// Initialize persist access (mounts /persist if needed, verifies integrity)
int bb_persist_init(void);

// Read/write configuration files on persist partition
//   namespace: subdirectory under /persist/config/ (e.g., "network", "blocks")
//   key: configuration key
char *bb_persist_config_get(const char *ns, const char *key);
int   bb_persist_config_set(const char *ns, const char *key, const char *value);

// Machine identity
const char *bb_persist_machine_id(void);   // /persist/machine-id

// Boot count log (circular buffer, max 256 entries)
typedef struct {
    uint64_t    timestamp;
    char        slot;               // 'a' or 'b'
    int         success;            // 1 = boot_ok, 0 = failed
    uint32_t    boot_time_ms;       // time from kernel to boot_ok
} bb_boot_entry_t;

int  bb_persist_boot_log_add(const bb_boot_entry_t *entry);
int  bb_persist_boot_log_read(bb_boot_entry_t *entries, int max_entries);

// Update history (last 32 updates)
typedef struct {
    uint64_t    timestamp;
    char        from_version[32];
    char        to_version[32];
    char        target_slot;
    int         success;
} bb_update_entry_t;

int  bb_persist_update_log_add(const bb_update_entry_t *entry);
int  bb_persist_update_log_read(bb_update_entry_t *entries, int max_entries);

// ---------------------------------------------------------------------------
// Manufacturing partition (/mfg) — read-only machine identity
// ---------------------------------------------------------------------------

#define BB_MFG_SERIAL_LEN     32
#define BB_MFG_MAC_LEN         6
#define BB_MFG_HW_REV_LEN     32
#define BB_MFG_DATE_LEN       16
#define BB_MFG_PARTNO_LEN     32
#define BB_MFG_CERT_FP_LEN    64

typedef struct __attribute__((packed)) {
    char     serial[BB_MFG_SERIAL_LEN];
    uint8_t  mac[BB_MFG_MAC_LEN];
    uint8_t  mac_wifi[BB_MFG_MAC_LEN];
    char     hw_revision[BB_MFG_HW_REV_LEN];
    char     mfg_date[BB_MFG_DATE_LEN];       // ISO 8601
    char     part_number[BB_MFG_PARTNO_LEN];
    char     cert_fingerprint[BB_MFG_CERT_FP_LEN];  // SHA-256 hex
    uint32_t crc32;
} bb_mfg_data_t;

// Read manufacturing data from the raw partition
// Returns 0 on success, -1 if partition not found or CRC mismatch
int bb_mfg_read(bb_mfg_data_t *out);

// Get individual fields (safe accessors)
const char *bb_mfg_serial(void);
const char *bb_mfg_hw_revision(void);
const char *bb_mfg_part_number(void);

#endif // BB_PERSIST_H
