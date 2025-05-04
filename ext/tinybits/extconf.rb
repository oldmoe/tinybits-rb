require 'mkmf'

$CFLAGS << " -O3 -march=native"

dir_config('tinybits_ext')
create_makefile('tinybits_ext')
