module TinyBits
 
  class DPacker
    
    # creates a new TinyBits::DPacker object
    # @param dict [[String]] an array of strings to use as a dictiontary (order is significant).
    # @return [TinyBits::DPacker] the dpacker object.
    # @raise [RuntimeError] the dict is nil or if any member doesn't respnd to to_s.
    def initialize(dict)
      @dict = dict.collect{|a| a.to_s }
      @dict_set = false
      @packer = Packer.new
    end

    def reset
      @packer.reset
      @dict_set = false
    end

    # packs an object to a binary string using the stored dictionary
    # @param obj [Object] The Ruby object to pack.
    # @return [String] The packed binary string (frozen).
    # @raise [RuntimeError] If packing fails due to unsupported types or other errors.
    def pack(object) 
      reset
      self << object
      to_s
    end

    # push an object to an inceremental buffer
    # @param obj [Object] The Ruby object to pack.
    # @return [Integer] The number of bytes written to the buffer.
    # @raise [RuntimeError] If packing fails due to unsupported types or other errors.
    def <<(object)
      if !@dict_set
        @dict_size = @packer << @dict
        @dict_set = true                
      end
      @packer << object
    end

    # return the packed buffer
    # @return [String] The packed buffer (minus the dictionary).
    def to_s
      res = @packer.to_s
      res[@dict_size, res.bytesize - @dict_size]
    end

  end
end