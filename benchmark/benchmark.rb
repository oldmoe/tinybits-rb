require 'oj'
require 'json'
require 'msgpack'
require 'cbor'
require 'tinybits'
require 'benchmark/ips'
require 'lz4-ruby'
require 'zstd-ruby'

RubyVM::YJIT.enable

packer = TinyBits::Packer.new
unpacker = TinyBits::Unpacker.new

mpacker = MessagePack::Packer.new
munpacker = MessagePack::Unpacker.new

encoders = {
  oj: proc{|doc| Oj.dump(doc)},
  json: proc{|doc| JSON.dump(doc)},
  cbor: proc{|doc| CBOR.encode(doc)},
  msgpack: proc{|doc| mpacker.reset; mpacker.pack(doc).to_s },
  tinybit: proc{|doc| packer.pack(doc)}
}

decoders = {
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
  Benchmark.ips do |x|
    encoders.each_pair do |title, encoder|
      x.report(title) { encoder.call(data) }
    end
  end
  puts
  puts "encoding + LZ4 benchmark"
  puts "------------------------------------"
  Benchmark.ips do |x|
    encoders.each_pair do |title, encoder|
      x.report(title) { LZ4.compress(encoder.call(data)) }
    end
  end
  puts
  puts "encoding + Zstd benchmark"
  puts "------------------------------------"
  Benchmark.ips do |x|
    encoders.each_pair do |title, encoder|
      x.report(title) { Zstd.compress(encoder.call(data)) }
    end
  end
  puts
  puts "decoding benchmark"
  puts "------------------------------------"
  Benchmark.ips do |x|
    decoders.each_pair do |title, decoder|
      x.report(title) { decoder.call(encoded[title][doc]) }
    end
  end
  puts
  puts "decoding + LZ4 benchmark"
  puts "------------------------------------"
  Benchmark.ips do |x|
    decoders.each_pair do |title, decoder|
      compressed = LZ4.compress(encoded[title][doc])
      x.report(title) { decoder.call(LZ4.decompress(compressed)) }
    end
  end
  puts
  puts "decoding + Zstd benchmark"
  puts "------------------------------------"
  Benchmark.ips do |x|
    decoders.each_pair do |title, decoder|
      compressed = Zstd.compress(encoded[title][doc])
      x.report(title) { decoder.call(Zstd.decompress(compressed)) }
    end
  end
  puts
  puts "===================================="
  puts
end
