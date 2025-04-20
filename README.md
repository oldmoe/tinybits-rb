# tinybits-rb

A Ruby gem that wraps the tinybits packing/unpacking C library for space (both in memory and on disk) and time efficient encoding/decoding of JSON like structures

## Features
Tinybits applies the following optimizations to the encoded data:

- Integer compression (using the now defunct SQLite4 varint scheme)
- Floating point compression via float scaling + varint encoding
- string deduplication with back referencing (very effective for repeated hash keys)

The tinybits-rb gem strives to minimize overheads as well:

- String interning for hash keys
- Zero copy for deduplicated strings

## Tinybits vs oother Ruby schemaless serializers
|          | Avg encoded size (bytes) | Avg encoding time (us) | Avg decoding time (us) | Avg encoding memory (KB) | avg decoding memory (KB) |
| -------- | ------------------------ | ---------------------- | ---------------------- | ------------------------ | ------------------------ |
| Oj       | 3,096                    | 15.7                   | 39.6                   | 3.21                     | 11.13                    |
| JSON     | 3,096                    | 13.5                   | 24.6                   | 4.19                     | 10.55                    |
| CBOR     | 2,625                    | 9.5                    | 31.1                   | 2.81                     | 16.46                    |
| Msgpack  | 2,639                    | 8.1                    | 28.2                   | 2.74                     | 10.50                    |
| TinyBits | **1,458**                | **7.4**                | **17.9**               | **1.53**                 | **9.62**                 |


## Usage
```ruby
require 'tinybits'

doc = {
  "lib" => "tinybits",
  "ver" => 0.1,
  "authors" => ["Mohamed", "Hamza", "Zain"],
  "rb_authors" => ["Mohamed", "Zain"],
  "py_authors" => ["Mohamed", "Hamza"]
}

packer = TinyBits::Packer.new
pakced = packer.pack(doc)
puts packed.bytesize # => 76

unpacker = TinyBits::Unpacker.new
unpacked = unpacker.unpack(packed)
puts unpacked == doc # => true
```

## Installation

add it to your Gemfile

```bash
  bundle add tinybits
  bundle install
```

or add it via RubyGems

```bash
  gem install tinybits
```

