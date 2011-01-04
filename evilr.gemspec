EVILR_FILES = %w(MIT-LICENSE Rakefile README.rdoc) + Dir["{ext,spec}/**/*.{c,h,rb}"]
EVILR_GEMSPEC = Gem::Specification.new do |s|
  s.name = 'evilr'
  s.version = '1.0.0'
  s.platform = Gem::Platform::RUBY
  s.has_rdoc = true
  s.extra_rdoc_files = ["MIT-LICENSE", "README.rdoc", "ext/evilr/evilr.c"]
  s.rdoc_options += ["--quiet", "--line-numbers", "--inline-source", '--title', "evilr: Do things you shouldn't", '--main', 'README.rdoc']
  s.summary = "Do things you shouldn't"
  s.author = "Jeremy Evans"
  s.email = "code@jeremyevans.net"
  s.homepage = "http://github.com/jeremyevans/evilr"
  s.required_ruby_version = ">= 1.8.6"
  s.files = EVILR_FILES
  s.extensions << 'ext/evilr/extconf.rb'
  s.description = <<END
evilr allows you to do things you shouldn't.  It's inspired by
evil.rb, but written in C instead of using ruby with DL.
END
end
