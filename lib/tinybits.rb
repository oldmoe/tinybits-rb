require_relative './tinybits/version'
require 'tinybits_ext'

module TinyBits
  def self.pack(object) = Packer.new.pack(object)
  def self.unpack(buffer) = Unpacker.new.unpack(buffer)
end