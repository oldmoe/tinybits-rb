<img src="https://github.com/oldmoe/tinybits/blob/main/TinyBitsLogo.svg"/>

# tinybits-rb

A Ruby gem that wraps the [tinybits packing/unpacking C library](https://github.com/oldmoe/tinybits) for space (both in memory and on disk) and time efficient encoding/decoding of JSON like structures

## Features
Tinybits applies the following optimizations to the encoded data:

- Integer compression (using the now defunct SQLite4 varint scheme)
- Floating point compression via float scaling + varint encoding
- string deduplication with back referencing (very effective for repeated hash keys)

The tinybits-rb gem strives to minimize overheads as well:

- String interning for hash keys
- Zero copy for deduplicated strings

### Tinybits supports serailizing from/to the following Ruby data type
- NUMBER (integer or float, up to 64bit values)
- String
- Symbol
- Hash
- Array
- True
- False
- Nil
- Time
- Any object that responds to `to_tinybits`

### Tinybits supports packing multiple objects into the same buffer
- Objects can be added discretely, they don't need to be provided all at once
- All objects will share the same deduplication dictionary
- Works great if you are generating objects and want to append them to an exisiting buffer

The unpacking process will need to have access to the whole buffer though, for deduplication

## Tinybits vs other Ruby schemaless serializers
This is the average of the 11 different json documents stored in the json folder in this repo
|          | Avg encoded size (bytes) | Avg encoding time (us) | Avg decoding time (us) | Avg encoding memory (KB) | avg decoding memory (KB) |
| -------- | ------------------------ | ---------------------- | ---------------------- | ------------------------ | ------------------------ |
| Oj       | 3,096                    | 15.7                   | 39.6                   | 3.21                     | 11.13                    |
| JSON     | 3,096                    | 13.5                   | 24.6                   | 4.19                     | 10.55                    |
| CBOR     | 2,625                    | 9.5                    | 31.1                   | 2.81                     | 16.46                    |
| Msgpack  | 2,639                    | 8.1                    | 28.2                   | 2.74                     | 10.50                    |
| TinyBits | **1,458**                | **7.4**                | **17.9**               | **1.53**                 | **9.62**                 |

From these results (based on this specific data set)  we can deduce the following:
- TinyBits produces packed data that is ~45% smaller than the second smallest format (CBOR).
- TinyBits packs data at a ~9% faster rate than the second fastest encoder (Msgpack)
- TinyBits unpacks data at a ~27% faster rate than the second fastest decoder (Ruby's stdlib JSON)
- TinyBits uses the least amount of memory of all the encoders/decoders tested

## Usage

You can use the default packer and unpacker interfaces:
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
Optionally you can use the more convenient, but slower, interface
```ruby
require 'tinybits'

doc = {
  "lib" => "tinybits",
  "ver" => 0.1,
  "authors" => ["Mohamed", "Hamza", "Zain"],
  "rb_authors" => ["Mohamed", "Zain"],
  "py_authors" => ["Mohamed", "Hamza"]
}

pakced = TinyBits.pack(doc)
puts packed.bytesize # => 76

unpacked = TinyBits.unpack(packed)
puts unpacked == doc # => true
```
You can also use the `push` and `pop` methods to incrementally encode and decode groups of object that all share the same deduplicated string set
```ruby
require 'tinybits'

packer = TinyBits::Packer.new
unpacker = TinyBits::Unpacker.new

objects = [{"abc": 123}, {"abc": [1, 2, "abc"]}, ["xyz", "abc", "xyz", 7.6] ]
objects.each do |obj|
  packer << obj
end

buffer = packer.to_s

unpacker.buffer = buffer

while(res = unpacker.pop)
  pp res
end
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

