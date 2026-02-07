#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdalign.h>

/**
 * @file yapb.h
 * @brief YAPB - Yet Another Protocol Buffer format.
 *
 * Packet structure:
 *   - Header: 4 bytes pkt_len (network byte order, total packet size)
 *   - Data:   Each element = 1 byte type + value (network byte order)
 *   - For BLOB: type + 2 byte length + raw bytes
 *   - For NESTED_PKT: type + nested packet (with its own 4 byte header)
 *
 * Error handling:
 *   YAPB uses sticky errors (like errno/OpenGL). Once an error occurs, all
 *   subsequent pop/push calls return immediately with that error. Check the
 *   final state with YAPB_get_error() after a sequence of operations.
 *
 *   Pop functions do NOT modify the output value on error. This enables
 *   forward compatibility - initialize fields to defaults before popping,
 *   and they'll keep their default if the packet lacks that field:
 *
 *   @code
 *     uint16_t new_field = 42;  // default for older packets
 *     YAPB_pop_u16(pkt, &new_field);  // stays 42 if packet ended
 *   @endcode
 */

/** @defgroup types Types
 *  Core types, enumerations, and constants.
 */

/** @defgroup lifecycle Lifecycle
 *  Packet creation, finalization, and loading.
 */

/** @defgroup push Push Operations
 *  Functions to append typed elements to a packet in write mode.
 *
 *  All push functions check for sufficient buffer space and set a sticky
 *  error on failure. Values are stored in network byte order.
 */

/** @defgroup pop Pop Operations
 *  Functions to read typed elements from a packet in read mode.
 *
 *  All pop functions validate the next type tag and set a sticky error on
 *  mismatch. Output values are not modified on error or YAPB_STS_COMPLETE,
 *  enabling forward-compatible reads with default values.
 */

/** @defgroup query Query Operations
 *  Functions to inspect packet state without modifying it.
 */

/**
 * @ingroup types
 * @brief Element type tags stored in the wire format.
 *
 * Each element in a packet is prefixed with a one-byte type tag.
 * Tags 0x06-0x0D are reserved for future types.
 */
typedef enum {
    YAPB_INT8   = 0x00, /**< Signed 8-bit integer (1 byte value). */
    YAPB_INT16  = 0x01, /**< Signed 16-bit integer (2 byte value, network order). */
    YAPB_INT32  = 0x02, /**< Signed 32-bit integer (4 byte value, network order). */
    YAPB_INT64  = 0x03, /**< Signed 64-bit integer (8 byte value, network order). */
    YAPB_FLOAT  = 0x04, /**< IEEE 754 single-precision float (4 bytes, network order). */
    YAPB_DOUBLE = 0x05, /**< IEEE 754 double-precision float (8 bytes, network order). */
    // 0x06-0x0D reserved for future types
    YAPB_BLOB       = 0x0E, /**< Raw byte blob (2 byte length + N bytes). */
    YAPB_NESTED_PKT = 0x0F, /**< Nested packet (complete packet with its own header). */
} YAPB_Type_t;

/** @ingroup types
 *  @brief Packet is in write mode. */
#define YAPB_MODE_WRITE 0
/** @ingroup types
 *  @brief Packet is in read mode. */
#define YAPB_MODE_READ  1

/** @ingroup types
 *  @brief Size of the packet header in bytes. */
#define YAPB_HEADER_SIZE 4

/**
 * @ingroup types
 * @brief Result codes returned by all YAPB functions.
 *
 * Negative values are errors. YAPB_OK indicates success with more data
 * remaining. YAPB_STS_COMPLETE indicates success and the last element
 * has been consumed.
 */
typedef enum {
    YAPB_ERR_NO_MORE_ELEMENTS = -7, /**< No more elements to pop. */
    YAPB_ERR_INVALID_PACKET   = -6, /**< Packet data is malformed. */
    YAPB_ERR_TYPE_MISMATCH    = -5, /**< Next element type doesn't match the pop call. */
    YAPB_ERR_INVALID_MODE     = -4, /**< Operation not valid in current mode (read/write). */
    YAPB_ERR_BUFFER_TOO_SMALL = -3, /**< Buffer cannot hold the data. */
    YAPB_ERR_NULL_PTR         = -2, /**< A required pointer argument was NULL. */
    YAPB_ERR_UNKNOWN          = -1, /**< Unknown error. */
    YAPB_OK                   = 0,  /**< Success, more elements may follow. */
    YAPB_STS_COMPLETE         = 1,  /**< Success, last element consumed. */
} YAPB_Result_t;

/** @ingroup types
 *  @brief Size of the opaque YAPB_Packet_t storage in bytes. */
#define YAPB_PACKET_SIZE 48

/**
 * @ingroup types
 * @brief Opaque packet handle, stack-allocatable.
 *
 * Internals are hidden; use YAPB_initialize() or YAPB_load() to set up.
 */
typedef struct YAPB_Packet {
    alignas(max_align_t) unsigned char _opaque[YAPB_PACKET_SIZE];
} YAPB_Packet_t;

/**
 * @ingroup types
 * @brief Tagged union returned by YAPB_pop_next().
 *
 * The @c type field indicates which union member in @c val is valid.
 */
typedef struct YAPB_Element {
    YAPB_Type_t type; /**< Type tag of the popped element. */
    union {
        int8_t   i8;     /**< Valid when type == YAPB_INT8. */
        int16_t  i16;    /**< Valid when type == YAPB_INT16. */
        int32_t  i32;    /**< Valid when type == YAPB_INT32. */
        int64_t  i64;    /**< Valid when type == YAPB_INT64. */
        float    f;      /**< Valid when type == YAPB_FLOAT. */
        double   d;      /**< Valid when type == YAPB_DOUBLE. */
        struct { const uint8_t *data; uint16_t len; } blob; /**< Valid when type == YAPB_BLOB. Pointer into packet buffer. */
        YAPB_Packet_t nested; /**< Valid when type == YAPB_NESTED_PKT. */
    } val; /**< Element value (check @c type before accessing). */
} YAPB_Element_t;

/**
 * @ingroup lifecycle
 * @brief Initialize a packet for writing.
 *
 * Sets up the packet in write mode. The caller provides the backing buffer.
 * The header is zeroed as a safety measure in case YAPB_finalize() is forgotten.
 *
 * @param pkt    Packet structure to initialize.
 * @param buffer Buffer to write packet data into.
 * @param size   Size of the buffer (must be >= YAPB_HEADER_SIZE).
 * @return YAPB_OK on success, error code otherwise.
 */
YAPB_Result_t YAPB_initialize(YAPB_Packet_t *pkt, uint8_t *buffer, size_t size);

/**
 * @ingroup lifecycle
 * @brief Finalize the packet, writing the total length into the header.
 *
 * Must be called after all push operations are complete. The packet
 * data in the buffer is ready to transmit after this call.
 *
 * @param pkt     Packet in write mode.
 * @param out_len Output: total packet length including header. May be NULL.
 * @return YAPB_OK on success, error code otherwise.
 */
YAPB_Result_t YAPB_finalize(YAPB_Packet_t *pkt, size_t *out_len);

/**
 * @ingroup lifecycle
 * @brief Load a packet for reading from raw data.
 *
 * Validates the header length field and sets up the packet in read mode.
 * The data buffer must remain valid for the lifetime of all pop operations.
 *
 * @param pkt  Packet structure to initialize.
 * @param data Raw packet data (including header).
 * @param size Size of the data buffer.
 * @return YAPB_OK on success, error code otherwise.
 */
YAPB_Result_t YAPB_load(YAPB_Packet_t *pkt, const uint8_t *data, size_t size);

/**
 * @ingroup push
 * @brief Push a signed 8-bit integer.
 * @param pkt Packet in write mode.
 * @param val Pointer to the value.
 * @return YAPB_OK on success, error code otherwise.
 */
YAPB_Result_t YAPB_push_i8(YAPB_Packet_t *pkt, const int8_t *val);

/**
 * @ingroup push
 * @brief Push a signed 16-bit integer.
 * @param pkt Packet in write mode.
 * @param val Pointer to the value.
 * @return YAPB_OK on success, error code otherwise.
 */
YAPB_Result_t YAPB_push_i16(YAPB_Packet_t *pkt, const int16_t *val);

/**
 * @ingroup push
 * @brief Push a signed 32-bit integer.
 * @param pkt Packet in write mode.
 * @param val Pointer to the value.
 * @return YAPB_OK on success, error code otherwise.
 */
YAPB_Result_t YAPB_push_i32(YAPB_Packet_t *pkt, const int32_t *val);

/**
 * @ingroup push
 * @brief Push a signed 64-bit integer.
 * @param pkt Packet in write mode.
 * @param val Pointer to the value.
 * @return YAPB_OK on success, error code otherwise.
 */
YAPB_Result_t YAPB_push_i64(YAPB_Packet_t *pkt, const int64_t *val);

/**
 * @ingroup push
 * @brief Push a single-precision float.
 * @param pkt Packet in write mode.
 * @param val Pointer to the value.
 * @return YAPB_OK on success, error code otherwise.
 */
YAPB_Result_t YAPB_push_float(YAPB_Packet_t *pkt, const float *val);

/**
 * @ingroup push
 * @brief Push a double-precision float.
 * @param pkt Packet in write mode.
 * @param val Pointer to the value.
 * @return YAPB_OK on success, error code otherwise.
 */
YAPB_Result_t YAPB_push_double(YAPB_Packet_t *pkt, const double *val);

/**
 * @ingroup push
 * @brief Push a raw byte blob.
 * @param pkt  Packet in write mode.
 * @param data Pointer to blob data (may be NULL if len is 0).
 * @param len  Length of the blob (max 65535).
 * @return YAPB_OK on success, error code otherwise.
 */
YAPB_Result_t YAPB_push_blob(YAPB_Packet_t *pkt, const uint8_t *data, uint16_t len);

/**
 * @ingroup push
 * @brief Push a finalized nested packet.
 *
 * The nested packet (including its header) is copied into the parent.
 *
 * @param pkt    Packet in write mode.
 * @param nested Finalized packet to embed.
 * @return YAPB_OK on success, error code otherwise.
 */
YAPB_Result_t YAPB_push_nested(YAPB_Packet_t *pkt, const YAPB_Packet_t *nested);

/** @ingroup push
 *  @brief Push an unsigned 8-bit integer. */
static inline YAPB_Result_t YAPB_push_u8(YAPB_Packet_t *pkt, const uint8_t *val) {
    return YAPB_push_i8(pkt, (const int8_t *)val);
}
/** @ingroup push
 *  @brief Push an unsigned 16-bit integer. */
static inline YAPB_Result_t YAPB_push_u16(YAPB_Packet_t *pkt, const uint16_t *val) {
    return YAPB_push_i16(pkt, (const int16_t *)val);
}
/** @ingroup push
 *  @brief Push an unsigned 32-bit integer. */
static inline YAPB_Result_t YAPB_push_u32(YAPB_Packet_t *pkt, const uint32_t *val) {
    return YAPB_push_i32(pkt, (const int32_t *)val);
}
/** @ingroup push
 *  @brief Push an unsigned 64-bit integer. */
static inline YAPB_Result_t YAPB_push_u64(YAPB_Packet_t *pkt, const uint64_t *val) {
    return YAPB_push_i64(pkt, (const int64_t *)val);
}

/**
 * @ingroup pop
 * @brief Pop the next element regardless of its type.
 *
 * Reads the type tag, then dispatches to the appropriate typed pop function.
 * The result is returned in a tagged union. This is useful for generic
 * iteration over packet contents.
 *
 * @param pkt Packet in read mode.
 * @param out Output: tagged union with type and value.
 * @return YAPB_OK, YAPB_STS_COMPLETE, or error code.
 */
YAPB_Result_t YAPB_pop_next(YAPB_Packet_t *pkt, YAPB_Element_t *out);

/**
 * @ingroup pop
 * @brief Pop a signed 8-bit integer.
 * @param pkt Packet in read mode.
 * @param out Output value (unchanged on error).
 * @return YAPB_OK, YAPB_STS_COMPLETE, or error code.
 */
YAPB_Result_t YAPB_pop_i8(YAPB_Packet_t *pkt, int8_t *out);

/**
 * @ingroup pop
 * @brief Pop a signed 16-bit integer.
 * @param pkt Packet in read mode.
 * @param out Output value (unchanged on error).
 * @return YAPB_OK, YAPB_STS_COMPLETE, or error code.
 */
YAPB_Result_t YAPB_pop_i16(YAPB_Packet_t *pkt, int16_t *out);

/**
 * @ingroup pop
 * @brief Pop a signed 32-bit integer.
 * @param pkt Packet in read mode.
 * @param out Output value (unchanged on error).
 * @return YAPB_OK, YAPB_STS_COMPLETE, or error code.
 */
YAPB_Result_t YAPB_pop_i32(YAPB_Packet_t *pkt, int32_t *out);

/**
 * @ingroup pop
 * @brief Pop a signed 64-bit integer.
 * @param pkt Packet in read mode.
 * @param out Output value (unchanged on error).
 * @return YAPB_OK, YAPB_STS_COMPLETE, or error code.
 */
YAPB_Result_t YAPB_pop_i64(YAPB_Packet_t *pkt, int64_t *out);

/**
 * @ingroup pop
 * @brief Pop a single-precision float.
 * @param pkt Packet in read mode.
 * @param out Output value (unchanged on error).
 * @return YAPB_OK, YAPB_STS_COMPLETE, or error code.
 */
YAPB_Result_t YAPB_pop_float(YAPB_Packet_t *pkt, float *out);

/**
 * @ingroup pop
 * @brief Pop a double-precision float.
 * @param pkt Packet in read mode.
 * @param out Output value (unchanged on error).
 * @return YAPB_OK, YAPB_STS_COMPLETE, or error code.
 */
YAPB_Result_t YAPB_pop_double(YAPB_Packet_t *pkt, double *out);

/**
 * @ingroup pop
 * @brief Pop a raw byte blob.
 *
 * Returns a pointer directly into the packet buffer. The pointer is valid
 * as long as the underlying data buffer is alive. Do not free it.
 *
 * @param pkt  Packet in read mode.
 * @param data Output: pointer into packet buffer (unchanged on error).
 * @param len  Output: blob length in bytes (unchanged on error).
 * @return YAPB_OK, YAPB_STS_COMPLETE, or error code.
 */
YAPB_Result_t YAPB_pop_blob(YAPB_Packet_t *pkt, const uint8_t **data, uint16_t *len);

/**
 * @ingroup pop
 * @brief Pop a nested packet.
 *
 * The output packet is set up in read mode, pointing into the parent's
 * data buffer. The parent buffer must remain valid.
 *
 * @param pkt Packet in read mode.
 * @param out Output: nested packet ready for reading (unchanged on error).
 * @return YAPB_OK, YAPB_STS_COMPLETE, or error code.
 */
YAPB_Result_t YAPB_pop_nested(YAPB_Packet_t *pkt, YAPB_Packet_t *out);

/** @ingroup pop
 *  @brief Pop an unsigned 8-bit integer. */
static inline YAPB_Result_t YAPB_pop_u8(YAPB_Packet_t *pkt, uint8_t *out) {
    return YAPB_pop_i8(pkt, (int8_t *)out);
}
/** @ingroup pop
 *  @brief Pop an unsigned 16-bit integer. */
static inline YAPB_Result_t YAPB_pop_u16(YAPB_Packet_t *pkt, uint16_t *out) {
    return YAPB_pop_i16(pkt, (int16_t *)out);
}
/** @ingroup pop
 *  @brief Pop an unsigned 32-bit integer. */
static inline YAPB_Result_t YAPB_pop_u32(YAPB_Packet_t *pkt, uint32_t *out) {
    return YAPB_pop_i32(pkt, (int32_t *)out);
}
/** @ingroup pop
 *  @brief Pop an unsigned 64-bit integer. */
static inline YAPB_Result_t YAPB_pop_u64(YAPB_Packet_t *pkt, uint64_t *out) {
    return YAPB_pop_i64(pkt, (int64_t *)out);
}

/**
 * @ingroup query
 * @brief Get the sticky error state of a packet.
 *
 * Returns the current error state. A non-negative value means no error
 * has occurred (YAPB_OK or YAPB_STS_COMPLETE).
 *
 * @param pkt Packet to check.
 * @return YAPB_OK or YAPB_STS_COMPLETE if no error, negative error code otherwise.
 */
YAPB_Result_t YAPB_get_error(const YAPB_Packet_t *pkt);

/**
 * @ingroup query
 * @brief Count the number of elements in a packet.
 *
 * Scans the packet from the header without advancing the read position.
 * Nested packets count as a single element.
 *
 * @param pkt       Packet in read mode.
 * @param out_count Output: number of elements.
 * @return YAPB_OK on success, error code otherwise.
 */
YAPB_Result_t YAPB_get_elem_count(const YAPB_Packet_t *pkt, uint16_t *out_count);

/**
 * @ingroup query
 * @brief Get a human-readable string for a result code.
 * @param result Result code.
 * @return Static string description (never NULL).
 */
const char *YAPB_Result_str(YAPB_Result_t result);

/**
 * @ingroup query
 * @brief Get a const pointer to the packet's backing buffer and its length.
 *
 * In read mode, returns the buffer and the packet length from the header.
 * In write mode, returns the buffer and its length only after YAPB_finalize()
 * has been called; returns NULL if the packet has not been finalized.
 *
 * @param pkt     Packet to query.
 * @param out_len Output: packet length in bytes. May be NULL.
 * @return Pointer to the buffer, or NULL if pkt is NULL or not finalized in write mode.
 */
const uint8_t *YAPB_get_buffer(const YAPB_Packet_t *pkt, size_t *out_len);

/**
 * @ingroup query
 * @brief Check if a receive buffer contains a complete YAPB packet.
 *
 * Reads the 4-byte header to determine the expected packet length,
 * then checks if the buffer contains at least that many bytes.
 * Useful for framing packets from a stream (e.g., TCP socket).
 *
 * @param data Raw data buffer (may be a partial packet).
 * @param len  Number of bytes available in the buffer.
 * @return true if the buffer contains a complete packet, false otherwise.
 */
bool YAPB_check_complete(const uint8_t *data, size_t len);
