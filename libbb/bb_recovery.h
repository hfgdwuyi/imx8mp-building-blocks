#ifndef BB_RECOVERY_H
#define BB_RECOVERY_H

#include <stdint.h>

// Recovery mode states
typedef enum {
    BB_RECOVERY_NONE       = 0,  // Normal operation
    BB_RECOVERY_REQUESTED  = 1,  // User requested recovery via bus command
    BB_RECOVERY_SLOT_FAIL  = 2,  // Both slots exhausted
    BB_RECOVERY_WATCHDOG   = 3,  // Hardware watchdog triggered recovery
    BB_RECOVERY_FACTORY    = 4,  // Factory reset requested
} bb_recovery_reason_t;

// ---------------------------------------------------------------------------
// Recovery trigger (from normal system)
// ---------------------------------------------------------------------------

// Request recovery mode on next boot (sets U-Boot env)
int bb_recovery_request(bb_recovery_reason_t reason);

// Check if we're currently booted in recovery mode
int bb_recovery_is_active(void);

// Get the reason for entering recovery
bb_recovery_reason_t bb_recovery_reason(void);

// ---------------------------------------------------------------------------
// Recovery agent (runs in recovery rootfs)
// ---------------------------------------------------------------------------

// Verify all partitions, return 0 if all healthy
int bb_recovery_verify_partitions(void);

// Re-image a partition from a source image file
//   partition: device path, e.g. "/dev/mmcblk0p6"
//   image_path: path to ext4 image or tar.gz
//   progress_cb: optional callback for progress (0-100)
int bb_recovery_reimage(const char *partition, const char *image_path,
                        void (*progress_cb)(int percent));

// Restore a previously created snapshot
int bb_recovery_restore_snapshot(const char *snapshot_label);

// List available snapshots on the recovery partition
// Returns JSON array string, caller must free
char *bb_recovery_list_snapshots(void);

// Factory reset: restore all partitions to manufacturing state
int bb_recovery_factory_reset(void);

// ---------------------------------------------------------------------------
// Snapshot management (from normal system)
// ---------------------------------------------------------------------------

// Create a snapshot of current state (config + package list + persist keys)
int bb_snapshot_create(const char *label);

// Delete a named snapshot
int bb_snapshot_delete(const char *label);

#endif // BB_RECOVERY_H
