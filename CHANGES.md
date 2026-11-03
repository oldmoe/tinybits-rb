# Changes

## 0.9.0 (2026-11-03)

- Dictionaries are now handled natively in the C library: `Packer.new(dict: [...])` and `Unpacker.new(dict: [...])`. The dictionary is loaded once, so packing with a dictionary costs about the same as without one, and unpacking with one is faster than without. `DPacker`, `DUnpacker` and `TinyBits.pack/unpack(obj, dict)` keep working and produce the same bytes as before
- Faster packing (dedupe hash lookups, cheaper Integer, Float and Array access)
- Faster unpacking (bulk hash construction, no instance variable lookups, faster tag dispatch)
- Hash keys are unpacked as interned frozen strings shared across decoded objects, roughly halving the objects kept by retained records
- Fix: string deduplication silently stopped referencing some strings once 256 strings were cached
- Fix: `pop` could read stale memory after GC compaction moved the buffer
- Fix: `buffer=` keeps its own frozen copy, modifying the original string while popping is safe
- Fix: `unpack` ends a `buffer=` session, a later `pop` finds an empty buffer instead of reading the unpacked string
- Fix: a failed `unpack` no longer corrupts the strings returned by the next `unpack`

## 0.8.0 (2026-10-30)

- `Unpacker.new(symbolize_keys: true)` returns hash keys that are strings as symbols, also accepted by `DUnpacker.new` and `TinyBits.unpack`
- Binary (ASCII-8BIT) strings are packed as blobs and unpacked as binary strings
- Integers outside the Fixnum range but within int64 are packed as integers
- NaN, Infinity and -Infinity are unpacked
- Fix: blobs were unpacked starting at their length prefix

## 0.7.0 (2026-10-30)

- Fix: long strings were unpacked starting at their length bytes
- Fix: `pop` and `finished?` treated the last byte of the buffer as its end
