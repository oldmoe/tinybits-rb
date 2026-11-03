# ruby benchmark/dictionary.rb [seconds per measurement, default 0.1]
# packs/unpacks each json doc with and without a dictionary built from the doc's keys
require 'json'
require 'tinybits'

RubyVM::YJIT.enable

SECS = (ARGV[0] || 0.1).to_f

def keys_of(obj, acc = [])
  case obj
  when Hash then obj.each { |k, v| acc << k; keys_of(v, acc) }
  when Array then obj.each { |v| keys_of(v, acc) }
  end
  acc
end

def record_arrays(obj, acc = [])
  case obj
  when Hash then obj.each_value { |v| record_arrays(v, acc) }
  when Array
    acc << obj if obj.size > 1 && obj.all?(Hash)
    obj.each { |v| record_arrays(v, acc) }
  end
  acc
end

def ops_per_sec
  n = 0
  t0 = Process.clock_gettime(Process::CLOCK_MONOTONIC)
  deadline = t0 + SECS
  while (now = Process.clock_gettime(Process::CLOCK_MONOTONIC)) < deadline
    100.times { yield }
    n += 100
  end
  n / (now - t0)
end

def rss_kb = File.read("/proc/self/status")[/VmRSS:\s+(\d+)/, 1].to_i

idle_rss = rss_kb
dir = File.expand_path("../json", __dir__)
puts "#{RUBY_DESCRIPTION}"
puts format("%-25s %8s %8s %12s %12s %12s %12s", "doc", "bytes", "dict b", "pack/s", "dict pack/s", "unpack/s", "dict unpk/s")
Dir[File.join(dir, "*.json")].sort.each do |f|
  doc = JSON.load_file(f)
  # a single record from the doc's largest array of records, packed with the doc's dictionary
  record = record_arrays(doc).max_by(&:size)&.first
  dict = keys_of(doc).uniq
  packer, unpacker = TinyBits::Packer.new, TinyBits::Unpacker.new
  dpacker, dunpacker = TinyBits::Packer.new(dict: dict), TinyBits::Unpacker.new(dict: dict)
  { File.basename(f) => doc, "#{File.basename(f, '.json')} record" => record }.each do |name, obj|
    next unless obj
    plain, dpacked = packer.pack(obj), dpacker.pack(obj)
    raise "roundtrip #{name}" unless dunpacker.unpack(dpacked) == unpacker.unpack(plain)
    puts format("%-25s %8d %8d %12.0f %12.0f %12.0f %12.0f", name, plain.bytesize, dpacked.bytesize,
      ops_per_sec { packer.pack(obj) }, ops_per_sec { dpacker.pack(obj) },
      ops_per_sec { unpacker.unpack(plain) }, ops_per_sec { dunpacker.unpack(dpacked) })
  end
end
puts "RSS idle #{idle_rss / 1024.0} MB, after load #{(rss_kb / 1024.0).round(1)} MB"
