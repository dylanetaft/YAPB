# CLAUDE.md - YAPB Guide

## What is YAPB?

YAPB (Yet Another Protocol Buffer) is a minimal C11 binary serialization library. It packs typed elements into a buffer with a 4-byte length header, all in network byte order. Zero allocations, sticky error handling, forward-compatible reads.

## Build

```bash
mkdir build && cd build
cmake ..
make
```

Options: `-DYAPB_BUILD_SHARED=ON`, `-DYAPB_BUILD_TESTS=ON` (default), `-DYAPB_BUILD_FUZZERS=ON`.

Tests use munit (fetched automatically). Run with `ctest` or `./tests/test_yapb`.

## Project Layout

```
include/yapb.h     Public API (opaque types, all declarations)
src/yapb.c         Implementation (real struct is _YAPB_Packet_t, hidden)
tests/             munit test suite
fuzzers/           LLVM fuzzer + corpus generator
cmake/             Package config for find_package(yapb)
```

## API Patterns

### Write a packet
```c
uint8_t buf[256];
YAPB_Packet_t pkt;
YAPB_initialize(&pkt, buf, sizeof(buf));
int32_t val = 42;
YAPB_push_i32(&pkt, &val);
size_t len;
YAPB_finalize(&pkt, &len);
// buf[0..len] is ready to send
```

### Read a packet
```c
YAPB_Packet_t pkt;
YAPB_load(&pkt, received_data, received_len);
int32_t val;
YAPB_pop_i32(&pkt, &val);
```

### Generic iteration with pop_next
```c
YAPB_Element_t elem;
while (YAPB_pop_next(&pkt, &elem) >= 0) {
    switch (elem.type) {
        case YAPB_INT32: printf("%d\n", elem.val.i32); break;
        case YAPB_BLOB:  /* elem.val.blob.data, elem.val.blob.len */ break;
        // ...
    }
}
```

### Access buffer / check completeness
```c
// Get the raw buffer pointer and length (write mode: only after finalize)
size_t len;
const uint8_t *buf = YAPB_get_buffer(&pkt, &len);

// Check if a receive buffer has a complete packet (for TCP framing)
if (YAPB_check_complete(recv_buf, recv_len)) {
    YAPB_load(&pkt, recv_buf, recv_len);
    // ... pop elements ...
}
```

## Critical Details

1. **All push/pop functions take pointers** - `YAPB_push_i32(&pkt, &val)` not `val`.

2. **Sticky errors** - Once an error occurs, all subsequent calls return that error. Check with `YAPB_get_error()` or just check the final call's return. This means you can chain calls without checking each one.

3. **Forward compatibility** - Pop functions do NOT modify the output on error. Set defaults before popping:
   ```c
   uint16_t version = 1;  // default
   YAPB_pop_u16(&pkt, &version);  // unchanged if packet ended
   ```

4. **YAPB_OK vs YAPB_STS_COMPLETE** - `YAPB_OK` (0) means success with more data. `YAPB_STS_COMPLETE` (1) means success and you consumed the last element. Both are >= 0. Errors are < 0.

5. **Opaque type** - `YAPB_Packet_t` is stack-allocated but opaque (48-byte aligned buffer). Never access its members directly. The real struct is internal to yapb.c.

6. **Blob pointers are borrowed** - `YAPB_pop_blob()` returns a pointer into the packet's data buffer. Don't free it. It's valid as long as the original data buffer is alive.

7. **Nested packets share the parent buffer** - `YAPB_pop_nested()` sets up a read-mode packet pointing into the parent's buffer. The parent data must stay alive.

8. **Network byte order** - All multi-byte values are big-endian on the wire. The API handles conversion transparently.

9. **Buffer access** - `YAPB_get_buffer()` returns a `const uint8_t *` to the packet's backing buffer and optionally its length via `out_len`. Returns NULL if pkt is NULL or if in write mode and not yet finalized.

10. **Packet completeness check** - `YAPB_check_complete()` is a standalone function (no packet needed) that checks if a raw data buffer contains a complete YAPB packet by reading the 4-byte header length. Useful for TCP stream framing.

11. **Finalization is one-shot** - `YAPB_finalize()` can only be called once per initialized packet. Calling it again returns `YAPB_ERR_INVALID_MODE`. All push functions also reject writes after finalization.

## Type Tags

| Tag | Constant | Value bytes |
|-----|----------|------------|
| 0x00 | YAPB_INT8 | 1 |
| 0x01 | YAPB_INT16 | 2 |
| 0x02 | YAPB_INT32 | 4 |
| 0x03 | YAPB_INT64 | 8 |
| 0x04 | YAPB_FLOAT | 4 |
| 0x05 | YAPB_DOUBLE | 8 |
| 0x0E | YAPB_BLOB | 2 (length) + N |
| 0x0F | YAPB_NESTED_PKT | 4 (header) + payload |

## Using as a Dependency

```cmake
find_package(yapb REQUIRED)
target_link_libraries(myapp PRIVATE yapb::yapb)
```

Or add as a subdirectory:
```cmake
add_subdirectory(yapb)
target_link_libraries(myapp PRIVATE yapb)
```
