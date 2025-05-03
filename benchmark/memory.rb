require 'oj'
require 'json'
require 'msgpack'
require 'cbor'
require 'tinybits'
require 'benchmark/ips'
require 'lz4-ruby'
require 'zstd-ruby'
require 'benchmark/memory'

RubyVM::YJIT.enable

packer = TinyBits::Packer.new
unpacker = TinyBits::Unpacker.new

mpacker = MessagePack::Packer.new
munpacker = MessagePack::Unpacker.new

encoders = {
  marshal: proc{|doc| Marshal.dump(doc)},
  oj: proc{|doc| Oj.dump(doc)},
  json: proc{|doc| JSON.dump(doc)},
  cbor: proc{|doc| CBOR.encode(doc)},
  msgpack: proc{|doc| mpacker.reset; mpacker.pack(doc).to_s },
  tinybit: proc{|doc| packer.pack(doc)}
}

decoders = {
  marshal: proc{|doc| Marshal.load(doc)},
  oj: proc{|doc| Oj.load(doc)},
  json: proc{|doc| JSON.load(doc)},
  cbor: proc{|doc| CBOR.decode(doc)},
  msgpack: proc{|doc| munpacker.reset; munpacker.feed(doc).unpack },
  tinybit: proc{|doc| unpacker.unpack(doc)}
}

encoded = {}

docs = {}

Dir.each_child('../json') do |child|
  docs[child] = JSON.load_file("../json/#{child}")
end

docs.each_pair do |doc, data|
  encoders.each_pair do |k, v|
    encoded[k] = {} unless encoded[k].is_a?(Hash)
    encoded[k][doc] = encoders[k].call(data)
  end
end

docs.each_pair do |doc, data|
  puts "benchmarking document: #{doc}"
  puts "------------------------------------"
  puts "size benchmark"
  puts "------------------------------------"
  puts "encoder\t\traw\tlz4\tzstd"
  encoders.each_pair do |title, encoder|
    packed = encoder.call(data)
    puts "#{title}\t\t#{packed.bytesize}\t#{LZ4.compress(packed).bytesize}\t#{Zstd.compress(packed).bytesize}"
  end
  puts
  puts "encoding benchmark"
  puts "------------------------------------"
  Benchmark.memory do |x|
    encoders.each_pair do |title, encoder|
      x.report(title) { 1000.times{encoder.call(data)} }
    end
  end
  puts
  puts "decoding benchmark"
  puts "------------------------------------"
  Benchmark.memory do |x|
    decoders.each_pair do |title, decoder|
      packed = encoded[title][doc]
      x.report(title) { 1000.times{decoder.call(packed)} }
    end
    x.compare!
  end
  puts
  puts "===================================="
  puts
end
