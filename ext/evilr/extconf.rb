require 'mkmf'
$CFLAGS << " -DRUBY19" if  RUBY_VERSION >= '1.9.0'
$CFLAGS << " -Wall " unless RUBY_PLATFORM =~ /solaris/
$CFLAGS << ' -g -ggdb -O0 -DDEBUG' if ENV['DEBUG']
$CFLAGS << " -Wconversion -Wsign-compare -Wno-unused-parameter -Wwrite-strings -Wpointer-arith -fno-common -pedantic -Wno-long-long" if ENV['STRICT']
$CFLAGS << (ENV['CFLAGS'] || '')
create_makefile("evilr")
