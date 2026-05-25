#ifndef BB_LOG_H
#define BB_LOG_H

#include <stdarg.h>
#include <stddef.h>

// Log levels
typedef enum {
    BB_LOG_FATAL = 0,   // System is unusable
    BB_LOG_ERROR = 1,   // Action must be taken immediately
    BB_LOG_WARN  = 2,   // Warning conditions
    BB_LOG_INFO  = 3,   // Normal but significant condition
    BB_LOG_DEBUG = 4,   // Debug-level messages
    BB_LOG_TRACE = 5,   // Very verbose tracing
} bb_log_level_t;

// Log destination flags (bitmask)
#define BB_LOG_TO_JOURNAL   (1 << 0)  // systemd journal (sd_journal_send)
#define BB_LOG_TO_FILE      (1 << 1)  // Persistent log file
#define BB_LOG_TO_STDERR    (1 << 2)  // stderr (for foreground/debug)
#define BB_LOG_TO_CRASH     (1 << 3)  // Fatal crash dump area

// Initialize logging for a block
//   ident: block identifier (e.g., "bb-avgate")
//   dest_mask: bitmask of BB_LOG_TO_* destinations
//   file_path: required if BB_LOG_TO_FILE set (e.g., "/var/log/persist/blocks/bb-avgate.log")
//   max_size: max log file size before rotation (0 = default 1MB)
int  bb_log_init(const char *ident, int dest_mask, const char *file_path, size_t max_size);

// Set runtime log level
void bb_log_set_level(bb_log_level_t level);
bb_log_level_t bb_log_get_level(void);

// Core logging function
void bb_log_write(bb_log_level_t level, const char *file, int line,
                  const char *fmt, ...) __attribute__((format(printf, 4, 5)));

// Convenience macros — these are the primary API
#define bb_log_fatal(fmt, ...)  bb_log_write(BB_LOG_FATAL, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define bb_log_error(fmt, ...)  bb_log_write(BB_LOG_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define bb_log_warn(fmt, ...)   bb_log_write(BB_LOG_WARN,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define bb_log_info(fmt, ...)   bb_log_write(BB_LOG_INFO,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define bb_log_debug(fmt, ...)  bb_log_write(BB_LOG_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define bb_log_trace(fmt, ...)  bb_log_write(BB_LOG_TRACE, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

// Crash dump (called from signal handler, writes to /persist/crash/)
void bb_log_crash(const char *ident, const char *reason, void *stack_ptr);

// Self-disclosure: returns JSON string with logging diagnostics
// Caller must free. Format:
//   {"log_level":"info","log_size":1234,"crash_count":0}
char *bb_log_diagnostics(const char *ident);

// Cleanup
void bb_log_close(void);

#endif // BB_LOG_H
