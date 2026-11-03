module TinyBits

  # an Unpacker with an external dictionary, same as Unpacker.new(dict: dict)
  class DUnpacker < Unpacker

    # @param dict [[String]] the dictionary the packer used (order is significant).
    # @param symbolize_keys [Boolean] unpack hash keys that are strings as symbols.
    def initialize(dict, symbolize_keys: false)
      super(dict: dict, symbolize_keys: symbolize_keys)
    end

  end

end
