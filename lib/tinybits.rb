require_relative './tinybits/version'
require 'tinybits_ext'
require_relative './tinybits/dpacker'
require_relative './tinybits/dunpacker'

module TinyBits
  # packs an object to a binary string
  # @param obj [Object] The Ruby object to pack.
  # @return [String] The packed binary string (frozen).
  # @raise [RuntimeError] If packing fails due to unsupported types or other errors.
  # this is a convinience interface, a better way is to instantiate a TinyBits::Packer
  # object and use its #pack method
  def self.pack(object, dict = nil)
    if dict
      DPacker.new(dict).pack(object)     
    else
      Packer.new.pack(object)
    end
  end

  # unpacks an object from a binary string
  # @param buffer [String] The Ruby string holding the packed buffer.
  # @param symbolize_keys [Boolean] unpack hash keys that are strings as symbols.
  # @return [Object] The unpacked Ruby Object (all strings within the object will be frozen).
  # @raise [RuntimeError] If unpacking fails due to unsupported types or malformed data.
  # this is a convinience interface, a better way is to instantiate a TinyBits::Unpacker
  # object and use its #unpack method
  def self.unpack(buffer, dict = nil, symbolize_keys: false)
    if dict
      DUnpacker.new(dict, symbolize_keys: symbolize_keys).unpack(buffer)
    else
      Unpacker.new(symbolize_keys: symbolize_keys).unpack(buffer)
    end
  end
end