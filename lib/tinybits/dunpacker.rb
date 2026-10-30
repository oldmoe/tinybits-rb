module TinyBits

  class DUnpacker
    
    # @param dict [[String]] the dictionary the packer used (order is significant).
    # @param symbolize_keys [Boolean] unpack hash keys that are strings as symbols.
    def initialize(dict, symbolize_keys: false)
      @dict = Packer.new.pack(dict.collect{|a| a.to_s })
      @unpacker = Unpacker.new(symbolize_keys: symbolize_keys)
      @dict_popped = false
    end

    def unpack(buffer)
      self.buffer = buffer
      pop
    end

    def buffer=(buffer)
      @unpacker.buffer = "#{@dict}#{buffer}" 
      @dict_popped = false         
    end

    def pop
      if !@dict_popped
        @unpacker.pop
        @dict_popped = true              
      end
      @unpacker.pop
    end

    def finished?
      @unpacker.finished?
    end

  end

end