# YAPB - Yet Another Protocol Buffer

A minimal, zero-allocation binary serialization library in C.

## Features

- **Typed elements** - int8/16/32/64, float, double, blobs, nested packets
- **Network byte order** - portable across architectures
- **Sticky errors** - check once after a sequence of operations
- **Forward compatible** - new fields silently ignored by old readers
- **Zero dependencies** - pure C11, no allocations

## Motivation
I've used Google Protobufs and FlatBuffers in other projects, but
found them too heavy-weight for simple use cases.  They function as 
code generators, and create data structures that align with what 
needs to be transmitted on the wire. Wire data structures don't 
nessarily match the use case of app level data structures.  
This leads to extra complexity in mapping between wire format and
application formats. YAPB is designed to be a simple, minimal
library that allows direct serialization of primitive types and
nested packets, without the complexity of code generation or
schema definitions.  The codebase is small and can be aggressively
tested as well as fuzzed for robustness.

## Building

```bash
mkdir build && cd build
cmake ..
make
```

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `YAPB_BUILD_SHARED` | OFF | Build shared library instead of static |
| `YAPB_BUILD_TESTS` | ON | Build test suite |
| `YAPB_BUILD_FUZZERS` | OFF | Build fuzzing targets (requires clang) |

### Running Tests

```bash
cd build && ctest
```

### Fuzzing

```bash
cmake .. -DYAPB_BUILD_FUZZERS=ON -DCMAKE_C_COMPILER=clang
make
cd ../fuzzers && bash run.sh
```

## Quick Example

```c
#include "yapb.h"

/* Write */
uint8_t buf[256];
YAPB_Packet_t pkt;
YAPB_initialize(&pkt, buf, sizeof(buf));
int32_t temp = 25;
YAPB_push_i32(&pkt, &temp);
size_t len;
YAPB_finalize(&pkt, &len);
/* send buf[0..len] over the wire */

/* Read */
YAPB_Packet_t rpkt;
YAPB_load(&rpkt, buf, len);
int32_t out;
YAPB_pop_i32(&rpkt, &out); /* out == 25 */
```

## Documentation

See https://df00znet-dev.github.io/YAPB/

## License

LGPL-2.1 - see [LICENSE](LICENSE)
