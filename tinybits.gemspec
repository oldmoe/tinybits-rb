require_relative './lib/tinybits/version'

Gem::Specification.new "tinybits", "0.1" do |s|
  s.name        = 'tinybits'
  s.version     = TinyBits::VERSION
  s.licenses    = ['MIT']
  s.summary     = "Very fast and compact serialization for Ruby!"
  s.description = "TinyBits is a Ruby gem that wraps the TinyBits C serializartion library, offering Rubyists the power of serializion with intger/float compression and string deduplication!"
  s.authors     = ["Mohamed Hassan"]
  s.email       = 'oldmoe@gamil.com'
  s.files       = Dir.glob([
                    "lib/**/*.rb",
                    "ext/**/*.{c,h,rb}"
                  ])
  s.homepage    = "https://github.com/oldmoe/tinybits-rb"
  s.metadata    = { "source_code_uri" => "https://github.com/oldmoe/tinybits-rb" }
  s.required_ruby_version = '>= 3.0.0'
  s.require_paths = ['lib']
  s.extensions = ['ext/tinybits/extconf.rb']
end
