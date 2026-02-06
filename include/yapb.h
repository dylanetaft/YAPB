#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdalign.h>

/**
 * YAPB - Yet Another Protocol Buffer format
 *
 * Packet structure:
 *   Header: 4 bytes pkt_len (network byte order, total packet size)
 *   Data:   Each element = 1 byte type + value (network byte order)
 *           For BLOB: type + 2 byte length + raw bytes
 *           For NESTED_PKT: type + nested packet (with its own 4 byte header)
 *
 * Error handling:
 *   YAPB uses sticky errors (like errno/OpenGL). Once an error occurs, all
 *   subsequent pop/push calls return immediately with that error. Check the
 *   final state with YAPB_get_error() after a sequence of operations.
 *
 *   Pop functions do NOT modify the output value on error or YAPB_STS_COMPLETE.
 *   This enables forward compatibility - initialize fields to defaults before
 *   popping, and they'll keep their default if the packet lacks that field:
 *
 *     uint16_t new_field = 42;  // default for older packets
 *     YAPB_pop_u16(pkt, &new_field);  // stays 42 if packet ended
 */

typedef enum {
    YAPB_INT8   = 0x00,
    YAPB_INT16  = 0x01,
    YAPB_INT32  = 0x02,
    YAPB_INT64  = 0x03,
    YAPB_FLOAT  = 0x04,
    YAPB_DOUBLE = 0x05,
    // 0x06-0x0D reserved for future types
    YAPB_BLOB       = 0x0E,
    YAPB_NESTED_PKT = 0x0F,
} YAPB_Type_t;

#define YAPB_MODE_WRITE 0
#define YAPB_MODE_READ  1

#define YAPB_HEADER_SIZE 4

typedef enum {
    YAPB_ERR_NO_MORE_ELEMENTS = -7,
    YAPB_ERR_INVALID_PACKET   = -6,
    YAPB_ERR_TYPE_MISMATCH    = -5,
    YAPB_ERR_INVALID_MODE     = -4,
    YAPB_ERR_BUFFER_TOO_SMALL = -3,
    YAPB_ERR_NULL_PTR         = -2,
    YAPB_ERR_UNKNOWN          = -1,
    YAPB_OK                   = 0,
    YAPB_STS_COMPLETE         = 1,
} YAPB_Result_t;

#define YAPB_PACKET_SIZE 48

typedef struct YAPB_Packet {
    alignas(max_align_t) unsigned char _opaque[YAPB_PACKET_SIZE];
} YAPB_Packet_t;

/**
 * Tagged union for YAPB_pop_next(). The type field indicates which
 * union member is valid.
 */
typedef struct YAPB_Element {
    YAPB_Type_t type;
    union {
        int8_t   i8;
        int16_t  i16;
        int32_t  i32;
        int64_t  i64;
        float    f;
        double   d;
        struct { const uint8_t *data; uint16_t len; } blob;
        YAPB_Packet_t nested;
    } val;
} YAPB_Element_t;

/**
 * @brief Initialize a packet for writing.
 * @param pkt Packet structure to initialize.
 * @param buffer Buffer to write packet data into.
 * @param size Size of the buffer.
 * @return YAPB_OK on success, error code otherwise.
 */
YAPB_Result_t YAPB_initialize(YAPB_Packet_t *pkt, uint8_t *buffer, size_t size);

/**
 * @brief Finalize the packet, writing the header.
 * @param pkt Packet in write mode.
 * @param out_len Output: total packet length.
 * @return YAPB_OK on success, error code otherwise.
 */
YAPB_Result_t YAPB_finalize(YAPB_Packet_t *pkt, size_t *out_len);

/**
 * @brief Load a packet for reading.
 * @param pkt Packet structure to initialize.
 * @param data Raw packet data (including header).
 * @param size Size of the data.
 * @return YAPB_OK on success, error code otherwise.
 */
YAPB_Result_t YAPB_load(YAPB_Packet_t *pkt, const uint8_t *data, size_t size);

// ============ Push functions ============

YAPB_Result_t YAPB_push_i8(YAPB_Packet_t *pkt, const int8_t *val);
YAPB_Result_t YAPB_push_i16(YAPB_Packet_t *pkt, const int16_t *val);
YAPB_Result_t YAPB_push_i32(YAPB_Packet_t *pkt, const int32_t *val);
YAPB_Result_t YAPB_push_i64(YAPB_Packet_t *pkt, const int64_t *val);
YAPB_Result_t YAPB_push_float(YAPB_Packet_t *pkt, const float *val);
YAPB_Result_t YAPB_push_double(YAPB_Packet_t *pkt, const double *val);
YAPB_Result_t YAPB_push_blob(YAPB_Packet_t *pkt, const uint8_t *data, uint16_t len);
YAPB_Result_t YAPB_push_nested(YAPB_Packet_t *pkt, const YAPB_Packet_t *nested);

static inline YAPB_Result_t YAPB_push_u8(YAPB_Packet_t *pkt, const uint8_t *val) {
    return YAPB_push_i8(pkt, (const int8_t *)val);
}
static inline YAPB_Result_t YAPB_push_u16(YAPB_Packet_t *pkt, const uint16_t *val) {
    return YAPB_push_i16(pkt, (const int16_t *)val);
}
static inline YAPB_Result_t YAPB_push_u32(YAPB_Packet_t *pkt, const uint32_t *val) {
    return YAPB_push_i32(pkt, (const int32_t *)val);
}
static inline YAPB_Result_t YAPB_push_u64(YAPB_Packet_t *pkt, const uint64_t *val) {
    return YAPB_push_i64(pkt, (const int64_t *)val);
}

// ============ Generic pop ============

/**
 * @brief Pop the next element regardless of type.
 * @param pkt Packet in read mode.
 * @param out Output: tagged union with type and value.
 * @return YAPB_OK, YAPB_STS_COMPLETE, or error code.
 */
YAPB_Result_t YAPB_pop_next(YAPB_Packet_t *pkt, YAPB_Element_t *out);

// ============ Pop functions ============

YAPB_Result_t YAPB_pop_i8(YAPB_Packet_t *pkt, int8_t *out);
YAPB_Result_t YAPB_pop_i16(YAPB_Packet_t *pkt, int16_t *out);
YAPB_Result_t YAPB_pop_i32(YAPB_Packet_t *pkt, int32_t *out);
YAPB_Result_t YAPB_pop_i64(YAPB_Packet_t *pkt, int64_t *out);
YAPB_Result_t YAPB_pop_float(YAPB_Packet_t *pkt, float *out);
YAPB_Result_t YAPB_pop_double(YAPB_Packet_t *pkt, double *out);
YAPB_Result_t YAPB_pop_blob(YAPB_Packet_t *pkt, const uint8_t **data, uint16_t *len);
YAPB_Result_t YAPB_pop_nested(YAPB_Packet_t *pkt, YAPB_Packet_t *out);

static inline YAPB_Result_t YAPB_pop_u8(YAPB_Packet_t *pkt, uint8_t *out) {
    return YAPB_pop_i8(pkt, (int8_t *)out);
}
static inline YAPB_Result_t YAPB_pop_u16(YAPB_Packet_t *pkt, uint16_t *out) {
    return YAPB_pop_i16(pkt, (int16_t *)out);
}
static inline YAPB_Result_t YAPB_pop_u32(YAPB_Packet_t *pkt, uint32_t *out) {
    return YAPB_pop_i32(pkt, (int32_t *)out);
}
static inline YAPB_Result_t YAPB_pop_u64(YAPB_Packet_t *pkt, uint64_t *out) {
    return YAPB_pop_i64(pkt, (int64_t *)out);
}

/**
 * @brief Get the sticky error state of a packet.
 * @param pkt Packet to check.
 * @return YAPB_OK or YAPB_STS_COMPLETE if no error, negative error code otherwise.
 */
YAPB_Result_t YAPB_get_error(const YAPB_Packet_t *pkt);

/**
 * @brief Count the number of elements in a packet.
 * @param pkt Packet in read mode.
 * @param out_count Output: number of elements.
 * @return YAPB_OK on success, error code otherwise.
 * @note Does not advance the read position. Nested packets count as 1 element.
 */
YAPB_Result_t YAPB_get_elem_count(const YAPB_Packet_t *pkt, uint16_t *out_count);

/**
 * @brief Get a string representation of a result code.
 * @param result Result code.
 * @return String description.
 */
const char *YAPB_Result_str(YAPB_Result_t result);
