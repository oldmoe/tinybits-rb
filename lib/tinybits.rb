require_relative './tinybits/version'
require 'tinybits_ext'

module TinyBits
  # packs an object to a binary string
  # @param obj [Object] The Ruby object to pack.
  # @return [String] The packed binary string (frozen).
  # @raise [RuntimeError] If packing fails due to unsupported types or other errors.
  # this is a convinience interface, a better way is to instantiate a TinyBits::Packer
  # object and use its #pack method
  def self.pack(object) = Packer.new.pack(object)

  # unpacks an object from a binary string
  # @param buffer [String] The Ruby string holding the packed buffer.
  # @return [Object] The unpacked Ruby Object (all strings within the object will be frozen).
  # @raise [RuntimeError] If unpacking fails due to unsupported types or malformed data.
  # this is a convinience interface, a better way is to instantiate a TinyBits::Unpacker
  # object and use its #unpack method
  def self.unpack(buffer) = Unpacker.new.unpack(buffer)
end