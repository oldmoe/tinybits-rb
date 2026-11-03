require 'minitest/autorun'
require 'tinybits'
require 'time'

class String
  def to_hex
    "0x" + self.bytes.map{|b| b.to_i.to_s(16) }.join
  end
end

class TinyBitsTest < Minitest::Test
  def setup
    @packer = TinyBits::Packer.new
    @unpacker = TinyBits::Unpacker.new
  end

  def test_primitive_types
    # Test nil, boolean, integer, float
    test_data = [nil, true, false, 42, -123, 3.14159]
    
    test_data.each do |value|
      packed = @packer.pack(value)
      value.nil? ? assert_nil(@unpacker.unpack(packed)) : assert_equal(value, @unpacker.unpack(packed))
    end
  end

  def test_strings
    strings = ["hello", "world", "unicode: ñáéíóú 你好 👋", "", "a" * 1000]
    
    strings.each do |str|
      packed = @packer.pack(str)
      assert_equal str, @unpacker.unpack(packed)
    end
  end

  def test_string_length_boundaries
    [30, 31, 32, 127, 128, 129, 159, 160, 16_415, 16_416, 70_000].each do |n|
      str = "x" * (n - 1) + "!"
      assert_equal [str, str, 7], @unpacker.unpack(@packer.pack([str, str, 7])), "length #{n}"
    end
  end

  def test_arrays
    arrays = [
      [],
      [1, 2, 3],
      ["a", "b", "c"],
      [1, "two", 3.0, nil, true, false],
      [1, [2, 3], 4]  # Nested array
    ]
    
    arrays.each do |arr|
      packed = @packer.pack(arr)
      assert_equal arr, @unpacker.unpack(packed)
    end
  end

  def test_hashes
    hashes = [
      {},
      {"a" => 1, "b" => 2},
      {"key" => "value"},
      {"nested" => {"inner" => 42}},
      {"mixed" => [1, 2, {"a" => "b"}]},
      {1 => "one", 2 => "two"}
    ]
    
    hashes.each do |hash|
      packed = @packer.pack(hash)
      assert_equal hash, @unpacker.unpack(packed)
    end
  end

  def test_complex_nested_structures
    complex = {
      "array" => [1, 2, 3],
      "hash" => {"a" => 1, "t" => 2},
      "nested" => [
        {"name" => "item1", "values" => [1, 2, 3]},
        {"name" => "item2", "values" => [4, 5, 6]}
      ],
      "mixed" => [1, {"x" => "y"}, [true, false, nil]]
    }
    
    packed = @packer.pack(complex)
    assert_equal complex, @unpacker.unpack(packed)
  end

  def test_time_objects
    times = [
      Time.now,
      Time.utc(2023, 1, 1, 12, 0, 0),
      Time.new(2023, 1, 1, 12, 0, 0, "+09:00")
    ]
    
    times.each do |time|
      packed = @packer.pack(time)
      unpacked = @unpacker.unpack(packed)
      
      # Compare with small delta because floating point conversion might lose some precision
      assert_in_delta time.to_f, unpacked.to_f, 0.001
      assert_equal time.utc_offset, unpacked.utc_offset
    end
  end

  def test_symbols
    symbols = [:symbol, :another_symbol, :"symbol with spaces"]
    
    symbols.each do |sym|
      packed = @packer.pack(sym)
      # Symbols are packed as strings
      assert_equal sym.to_s, @unpacker.unpack(packed)
    end
  end

  def test_custom_to_tinybits
    # Create a test class with to_tinybits method
    person_class = Class.new do
      attr_accessor :name, :age
      
      def initialize(name, age)
        @name = name
        @age = age
      end
      
      def to_tinybits
        {"name" => @name, "age" => @age}
      end
    end
    
    person = person_class.new("Alice", 30)
    packed = @packer.pack(person)
    unpacked = @unpacker.unpack(packed)
    
    assert_equal({"name" => "Alice", "age" => 30}, unpacked)
  end

  def test_packer_reset
    @packer.pack("test")
    assert_equal "test", @unpacker.unpack(@packer.to_s)
    
    @packer.reset
    assert_equal "", @packer.to_s
    
    @packer.pack("after reset")
    assert_equal "after reset", @unpacker.unpack(@packer.to_s)
  end

  def test_multiobject_packing
    @packer.push("first")
    @packer.push(42)
    @packer.push({"key" => "value"})
    
    buffer = @packer.to_s
    @unpacker.buffer = buffer
    
    assert_equal "first", @unpacker.pop
    assert_equal 42, @unpacker.pop
    assert_equal({"key" => "value"}, @unpacker.pop)
    assert_nil @unpacker.pop  # No more objects
    assert @unpacker.finished?
  end

  def test_push_operator
    @packer << "first"
    @packer << 42
    
    buffer = @packer.to_s
    @unpacker.buffer = buffer
    
    assert_equal "first", @unpacker.pop
    assert_equal 42, @unpacker.pop
  end

  def test_dictionary_roundtrip
    dict = ["status", "headers", "body"]
    [42, "a long body " * 10, { "status" => 200, "headers" => { "body" => "x" } }].each do |obj|
      assert_equal obj, TinyBits.unpack(TinyBits.pack(obj, dict), dict)
    end
    dpacker = TinyBits::DPacker.new(dict)
    dpacker << "status"
    dpacker << 7
    dunpacker = TinyBits::DUnpacker.new(dict)
    dunpacker.buffer = dpacker.to_s
    assert_equal ["status", 7], [dunpacker.pop, dunpacker.pop]
    assert dunpacker.finished?
  end

  def test_dictionary_option_reuse
    dict = ["status", "headers", "body"]
    packer = TinyBits::Packer.new(dict: dict)
    unpacker = TinyBits::Unpacker.new(dict: dict)
    objs = [{ "status" => 200, "body" => "status" }, ["headers", "other", "other"], "body"]
    3.times do
      objs.each do |obj|
        packed = packer.pack(obj)
        assert_equal TinyBits.pack(obj, dict), packed
        assert_operator packed.bytesize, :<=, @packer.pack(obj).bytesize
        assert_equal obj, unpacker.unpack(packed)
      end
    end
    packer.reset
    assert_equal "", packer.to_s
    packer << "body"
    packer << "other"
    packer << "other"
    unpacker.buffer = packer.to_s
    assert_equal ["body", "other", "other"], [unpacker.pop, unpacker.pop, unpacker.pop]
    assert unpacker.finished?
    assert unpacker.unpack(packer.pack("status")).frozen?
  end

  def test_dictionary_ids_match_packed_dictionary
    # only unique 2..128 byte strings take an id: status=0, sym=1, body=2, "42"=3
    dict = ["a", "status", "status", "x" * 200, :sym, "", "body", 42]
    packed = TinyBits::Packer.new(dict: dict).pack(["body", "42", "sym", "status"])
    assert_equal [0x0C, 0x62, 0x63, 0x61, 0x60], packed.bytes
    assert_equal ["body", "42", "sym", "status"], TinyBits::Unpacker.new(dict: dict).unpack(packed)
  end

  def test_dictionary_larger_than_cache
    dict = (0...300).map { |i| "key#{i}" }
    obj = (0...300).to_h { |i| ["key#{i}", "key#{299 - i}"] }
    packed = TinyBits.pack(obj, dict)
    assert_equal obj, TinyBits.unpack(packed, dict)
    assert_operator packed.bytesize, :<, @packer.pack(obj).bytesize
  end

  def test_unpack_empties_the_pop_buffer
    packer = TinyBits::Packer.new
    packer << "from a"
    packer << "more from a"
    @unpacker.buffer = packer.to_s
    assert_equal "from a", @unpacker.pop
    packer.reset
    packer << "first of b"
    packer << "second of b"
    b = packer.to_s.dup
    assert_equal "first of b", @unpacker.unpack(b)
    b.replace("x" * 5000)
    GC.start
    assert @unpacker.finished?
    assert_nil @unpacker.pop
  end

  def test_failed_unpack_does_not_shift_string_ids
    bad = @packer.pack(["aa", "bb", 1.5])[0..-3]
    assert_raises(RuntimeError) { @unpacker.unpack(bad) }
    assert_equal ["cc", "cc"], @unpacker.unpack(@packer.pack(["cc", "cc"]))
  end

  def test_pop_survives_buffer_mutation
    packer = TinyBits::Packer.new
    packer << "one"
    packer << "two"
    s = packer.to_s.dup
    @unpacker.buffer = s
    assert_equal "one", @unpacker.pop
    s.replace("y" * 5000)
    GC.start
    assert_equal "two", @unpacker.pop
  end

  def test_pop_survives_gc_compaction
    packer = TinyBits::Packer.new
    objs = 50.times.map { |i| { "ab" => "cd#{i}" } }
    objs.each { |o| packer << o }
    @unpacker.buffer = packer.to_s.dup # small enough to be an embedded (movable) string
    got = 10.times.map { @unpacker.pop }
    GC.verify_compaction_references(expand_heap: true, toward: :empty)
    got.concat(40.times.map { @unpacker.pop })
    assert_equal objs, got
  end

  def test_full_dedupe_cache_keeps_refs
    strs = (0...256).map { |i| format("s%03d", i) }
    packed = @packer.pack(strs + strs)
    # strings + their tags + array header + a ref per repeat (+1 varint byte for ids >= 31)
    assert_equal strs.sum(&:bytesize) + 256 + 3 + 256 + 225, packed.bytesize
    assert_equal strs + strs, @unpacker.unpack(packed)
  end

  def test_pop_single_byte_last_object
    [42, nil, true, false, "ab"].each do |last|
      packer = TinyBits::Packer.new
      packer << "ab"
      packer << last
      @unpacker.buffer = packer.to_s

      assert_equal "ab", @unpacker.pop
      refute @unpacker.finished?, "finished? before #{last.inspect}"
      last.nil? ? assert_nil(@unpacker.pop) : assert_equal(last, @unpacker.pop)
      assert @unpacker.finished?
    end
  end

  def test_method_aliases
    # Test all the method aliases
    value = {"test" => 123}
    
    packed1 = @packer.pack(value)
    @packer.reset
    packed2 = @packer.encode(value)
    @packer.reset
    packed3 = @packer.dump(value)
    
    assert_equal packed1, packed2
    assert_equal packed2, packed3
    
    unpacked1 = @unpacker.unpack(packed1)
    unpacked2 = @unpacker.decode(packed2)
    unpacked3 = @unpacker.load(packed3)
    
    assert_equal value, unpacked1
    assert_equal value, unpacked2
    assert_equal value, unpacked3
  end

  def test_large_structures
    # Test with a large array
    large_array = (1..1000).to_a
    packed = @packer.pack(large_array)
    assert_equal large_array, @unpacker.unpack(packed)
    
    # Test with a large hash
    large_hash = (1..1000).to_h { |i| [i.to_s, i] }
    packed = @packer.pack(large_hash)
    #puts packed.to_hex
    assert_equal large_hash, @unpacker.unpack(packed)
  end

  def test_string_deduplication
    # The library should deduplicate identical strings
    strings = ["duplicate", "duplicate", "duplicate"]
    array = strings * 10  # 30 identical strings
    
    packed = @packer.pack(array)
    assert_equal array, @unpacker.unpack(packed)
    
    # This is a bit implementation-specific, but we can test that the packed
    # representation is smaller than it would be without deduplication
    # For example, we could compare with JSON encoding which doesn't deduplicate
    require 'json'
    assert packed.bytesize < array.to_json.bytesize
  end

  def test_error_handling
    # Test with unsupported type
    unsupported_obj = Object.new  # plain object without to_tinybits
    
    assert_raises(RuntimeError) do
      @packer.pack(unsupported_obj)
    end
    
  end

  def test_finished_predicate
    @unpacker.buffer = @packer.pack("test")
    assert_equal false, @unpacker.finished?
    
    @unpacker.pop
    assert_equal true, @unpacker.finished?
  end

  def test_unpacker_without_buffer
    assert_raises(RuntimeError) do
      @unpacker.pop  # Should raise error when no buffer is set
    end

    assert_raises(RuntimeError) do
      @unpacker.finished?  # Should raise error when no buffer is set
    end
  end

  def test_symbolize_keys
    data = [{ id: 1, "name" => "x", nested: { list: [{ k: 1 }] } }, { 7 => "int key" }]
    packed = @packer.pack(data)
    assert_equal [{ "id" => 1, "name" => "x", "nested" => { "list" => [{ "k" => 1 }] } }, { 7 => "int key" }],
                 @unpacker.unpack(packed)
    symbolizer = TinyBits::Unpacker.new(symbolize_keys: true)
    assert_equal [{ id: 1, name: "x", nested: { list: [{ k: 1 }] } }, { 7 => "int key" }], symbolizer.unpack(packed)
    # repeated keys come back as deduplicated strings first, then as the same symbol
    rows = Array.new(5) { |i| { id: i, randomNumber: i * 2 } }
    assert_equal rows, symbolizer.unpack(@packer.pack(rows))
    # values stay strings
    assert_equal({ s: "v" }, symbolizer.unpack(@packer.pack({ s: "v" })))
  end

  def test_int64_range
    [2**62 - 1, 2**62, 2**63 - 1, -(2**62), -(2**62) - 1, -(2**63) + 1].each do |n|
      assert_equal [n, n], @unpacker.unpack(@packer.pack([n, n])), n.to_s
    end
    assert_raises(RuntimeError) { TinyBits::Packer.new.pack(2**63) }
  end

  def test_binary_strings_round_trip_as_binary
    blob = "\xFF\x00abc".b
    out = @unpacker.unpack(@packer.pack(["text", blob, { k: blob }]))
    assert_equal ["text", blob, { "k" => blob }], out
    assert_equal Encoding::BINARY, out[1].encoding
    assert_equal Encoding::UTF_8, out[0].encoding
    assert_equal Encoding::BINARY, @unpacker.unpack(@packer.pack("".b)).encoding
  end

  def test_nan_and_infinity
    nan, inf, ninf = @unpacker.unpack(@packer.pack([Float::NAN, Float::INFINITY, -Float::INFINITY]))
    assert nan.nan?
    assert_equal Float::INFINITY, inf
    assert_equal(-Float::INFINITY, ninf)
  end

  def test_symbolize_keys_with_pop_and_dictionary
    symbolizer = TinyBits::Unpacker.new(symbolize_keys: true)
    packer = TinyBits::Packer.new
    packer << { a: 1 }
    packer << { b: 2 }
    symbolizer.buffer = packer.to_s
    assert_equal [{ a: 1 }, { b: 2 }], [symbolizer.pop, symbolizer.pop]
    dict = ["status", "body"]
    obj = { "status" => 200, "body" => "ok" }
    assert_equal({ status: 200, body: "ok" }, TinyBits.unpack(TinyBits.pack(obj, dict), dict, symbolize_keys: true))
    assert_equal({ status: 200, body: "ok" }, TinyBits.unpack(TinyBits.pack(obj), symbolize_keys: true))
  end
end