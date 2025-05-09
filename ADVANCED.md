# Advanced TinyBits features
## Multi-Object encoding (experimental)
Sometimes you want to encode multiple objects together, even though they are not all available at the same time (otherwise you could just put them in an array and pack them). TinyBits allows appending multiple objects to a single buffer. This also has the benefit of all objects sharing the same deduplication dictionary

### Example:

```ruby
  require 'tinybits'

  packer = TinyBits::Packer.new

  # encode objects as they arrive fromn a DB result set
  rs.each do |row|
    packer << row
  end

  # retrieve the full encoded buffer
  packed = packer.to_s
  
  #reset the buffer for later use
  packer.reset
  
  # create an unpacker object
  unpacker = TinyBits::Unpacker.new

  # if you just use unpacker#unpack it will only unpack the first object
  # instead we set the buffer for the unpacker object
  unpacker.buffer = packed

  results = []
  # check if we are not done
  while !unpacker.finished?
    # retrieve object one by one
    results << unpacker.pop
  end
```

## External dictionary support (experimental)
TinyBits is very useful when there are multiple objects that share hash keys and other strings. But what if you are packing and sending single objects instead of multiple ones? 
In this case string deduplication will not really help as it will not find duplicates, and you will be wasting bandwidth sending the same keys (and potentially some repeated values) over and over with each single object

TinyBits supports using an external dictionary, which is basically a list of strings (or any object that responds to #to_s) provided to the TinyBits packing and unpacking classes. 
TinyBits will use this dictionary for deduplication but it will not append it to the data.

Two new classes are provided, TinyBits::DPacker and TinyBits::DUnpacker, which behave exactly like Packer/Unpacker, execept their initializers expect a single argument, an array of strings

It is important that exactly the same arrays and in the same order are presented to the dpacker and dunpacker objects, any changes will result in a corrupted result

You can also do one off packing and unpacking with a dictionary by passing the dictionary to TinyBits.pack and TinyBits.unpack

### Example:

```ruby
  require 'tinybits'
  
  # a sample user object, no string deduplication possible
  user = {
    "first_name" => "Adam",
    "last_name" => "Anwar",
    "grade" => 3.7,
    "birth_date" => "2010-01-01",
    "mother_tongue" => "Arabic",
    "gender" => "male"
  } 
  
  # pack normally
  packed = TinyBits.pack(user)
  
  packed.bytesize # =>  96 bytes (very slightly less than what you would get from msgpack or cbor)
  
  # now let's create a dictionary
  dict = [
    "first_name",
    "last_name",
    "grade",
    "birth_date",
    "mother_tongue",
    "gender",
    "male", "
    female",
    "Arabic",
    "English",
    "French",
    "Spanish",
    "Chinese",
    "German"
  ]

  # pack using the dictionary
  dpacked = TinyBits.pack(user, dict)
  
  dpacked.bytesize # => 34 (much less than without a dictionary, we are now in ProtocolBuffers territory or better)
  
  # to unpack, you do the same as with packing, supply the dictionary
  unpacked = TinyBits.unpack(dpacked, dict)

  pp unpacked # =>  { "first_name" => "Adam", "last_name" => "Anwar", "grade" => 3.7, "birth_date" => "2010-01-01", "mother_tongue" => "Arabic", "gender" => "male" }
```


