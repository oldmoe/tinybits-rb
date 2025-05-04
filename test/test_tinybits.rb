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
      assert_equal value, @unpacker.unpack(packed)
    end
  end

  def test_strings
    strings = ["hello", "world", "unicode: ñáéíóú 你好 👋", "", "a" * 1000]
    
    strings.each do |str|
      packed = @packer.pack(str)
      assert_equal str, @unpacker.unpack(packed)
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
end