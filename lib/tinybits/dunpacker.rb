module TinyBits

  class DUnpacker
    
    def initialize(dict)
      @dict = Packer.new.pack(dict.collect{|a| a.to_s })
      @unpacker = Unpacker.new
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