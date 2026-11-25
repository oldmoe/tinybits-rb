require 'minitest/autorun'
require 'rbconfig'

# Packer and Unpacker are write-barrier protected. The Unpacker keeps the buffer, the dedupe strings and a cache of map
# keys; each store goes through RB_OBJ_WRITE. The child makes one kind of store into an Unpacker made old first: a new
# key into the cache. Without the barrier GC.verify_internal_consistency reports "WB miss (O->Y)". Runs in a child.
class TestWriteBarriers < Minitest::Test
  CHILD = <<~'RUBY'
    require 'tinybits'
    u = TinyBits::Unpacker.new
    data = TinyBits::Packer.new.pack({ "a key the cache has not seen #{rand(1 << 30)}" => 1 })
    u.buffer = data
    4.times { GC.start }      # the unpacker and its buffer are old; nothing remembered
    obj = u.pop               # the new key is interned now (young) and cached in the unpacker
    GC.verify_internal_consistency
    abort "bad unpack #{obj.inspect}" unless obj.values == [1]
    puts 'ok'
  RUBY

  def test_a_new_cached_key
    out = IO.popen([RbConfig.ruby, '-I', File.expand_path('../lib', __dir__), '-e', CHILD], err: %i[child out], &:read)
    assert $?.success?, out[0, 1500]
    refute_match(/WB miss/, out)
    assert_equal 'ok', out.lines.last&.strip, out[0, 1500]
  end
end
