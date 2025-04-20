require "rake/clean"
require "rake/extensiontask"

Rake::ExtensionTask.new "tinybits_ext" do |ext|
  ext.lib_dir = "lib"
  ext.ext_dir = "ext/tinybits"  
  ext.source_pattern = "*.c"  
end

task :recompile => [:clean, :compile]

task :default => [:compile]

CLEAN.include 'lib/*.o', 'lib/*.so', 'lib/*.so.*', 'lib/*.a'

task :build do
  require_relative './lib/tinybits/version'
  version = TinyBits::VERSION
  
  puts 'Building tinybits...'
  `gem build tinybits.gemspec`
end

task :release do
  require_relative './lib/tinybits/version'
  version = TinyBits::VERSION
  
  puts 'Building tinybits...'
  `gem build tinybits.gemspec`

  puts "Pushing tinybits #{version}..."
  `gem push tinybits-#{version}.gem`

  puts "Cleaning up..."
  `rm *.gem`
end

