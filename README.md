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

## Synopsis
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

