#ifndef DE100_FILE_H
#define DE100_FILE_H

#include "base.h"
#include "time.h"
#include <stdbool.h>

// ═══════════════════════════════════════════════════════════════════════════
// ERROR CODES
// ═══════════════════════════════════════════════════════════════════════════

typedef enum {
    FILE_SUCCESS = 0,
    FILE_ERROR_NOT_FOUND,
    FILE_ERROR_ACCESS_DENIED,
    FILE_ERROR_ALREADY_EXISTS,
    FILE_ERROR_IS_DIRECTORY,
    FILE_ERROR_NOT_A_FILE,
    FILE_ERROR_DISK_FULL,
    FILE_ERROR_READ_FAILED,
    FILE_ERROR_WRITE_FAILED,
    FILE_ERROR_INVALID_PATH,
    FILE_ERROR_TOO_LARGE,
    FILE_ERROR_SIZE_MISMATCH,
    FILE_ERROR_UNKNOWN,
    
    FILE_ERROR_COUNT  // Sentinel for validation
} De100FileErrorCode;

// ═══════════════════════════════════════════════════════════════════════════
// RESULT STRUCTURES (Lean - No Error Message Buffers)
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
    bool success;
    De100FileErrorCode error_code;
} De100FileResult;

typedef struct {
    PlatformTimeSpec value;
    bool success;
    De100FileErrorCode error_code;
} De100FileTimeResult;

typedef struct {
    int64 value;  // -1 on error
    bool success;
    De100FileErrorCode error_code;
} De100FileSizeResult;

typedef struct {
    bool exists;
    bool success;  // false if check itself failed (e.g., permission error)
    De100FileErrorCode error_code;
} De100FileExistsResult;

// ═══════════════════════════════════════════════════════════════════════════
// API FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Get the last modification time of a file.
 *
 * @param filename Path to the file
 * @return De100FileTimeResult with modification time or error code
 *
 * Usage:
 *   De100FileTimeResult r = de100_file_get_mod_time("game.so");
 *   if (!r.success) {
 *       printf("Error: %s\n", de100_file_strerror(r.error_code));
 *   }
 */
De100FileTimeResult de100_file_get_mod_time(const char *filename);

/**
 * Compare two file modification times.
 *
 * @param a First time value
 * @param b Second time value
 * @return Difference in seconds (a - b). Positive if a is newer.
 */
real64 de100_file_time_diff(const PlatformTimeSpec *a, const PlatformTimeSpec *b);

/**
 * Copy a file from source to destination.
 * Overwrites destination if it exists.
 *
 * @param source Path to source file
 * @param dest Path to destination file
 * @return De100FileResult indicating success or failure
 */
De100FileResult de100_file_copy(const char *source, const char *dest);

/**
 * Check if a file exists (and is a regular file, not a directory).
 *
 * @param filename Path to the file
 * @return De100FileExistsResult with exists flag and any error
 *
 * Note: result.success can be true even if result.exists is false
 *       (file doesn't exist is not an error, just a fact)
 */
De100FileExistsResult de100_file_exists(const char *filename);

/**
 * Get the size of a file in bytes.
 *
 * @param filename Path to the file
 * @return De100FileSizeResult with size or error code
 */
De100FileSizeResult de100_file_get_size(const char *filename);

/**
 * Delete a file. Idempotent - returns success if file doesn't exist.
 *
 * @param filename Path to the file
 * @return De100FileResult indicating success or failure
 */
De100FileResult de100_file_delete(const char *filename);

// ═══════════════════════════════════════════════════════════════════════════
// ERROR HANDLING
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Get a human-readable error message for an error code.
 *
 * @param code Error code from any file operation
 * @return Static string describing the error (never NULL)
 */
const char *de100_file_strerror(De100FileErrorCode code);

#endif // DE100_FILE_H
