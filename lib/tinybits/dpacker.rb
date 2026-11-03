module TinyBits

  # a Packer with an external dictionary, same as Packer.new(dict: dict)
  class DPacker < Packer

    # creates a new TinyBits::DPacker object
    # @param dict [[String]] an array of strings to use as a dictiontary (order is significant).
    # @return [TinyBits::DPacker] the dpacker object.
    # @raise [RuntimeError] the dict is nil or if any member doesn't respnd to to_s.
    def initialize(dict)
      super(dict: dict)
    end

  end
end
