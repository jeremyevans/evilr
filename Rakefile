require "rake"
require "rake/clean"

CLEAN.include %w'*.core rdoc ext/evilr/Makefile ext/evilr/evilr.o ext/evilr/evilr.so lib tmp evilr-*.gem'

desc "Build the gem"
task :gem do
  sh %{gem build evilr.gemspec}
end

begin
  require 'rake/extensiontask'
  Rake::ExtensionTask.new('evilr')
rescue LoadError
end

begin
  require "spec/rake/spectask"

  Spec::Rake::SpecTask.new("spec" => :compile) do |t|
    t.spec_files = ["spec/evilr_spec.rb"]
  end
  task :default => :spec

rescue LoadError
end

desc "Start an IRB shell using the extension"
task :irb => :compile do
  require 'irb'
  $:.unshift 'lib'
  require 'evilr'
  c = IRB.conf
  def c.[]=(k,v)
    super unless k == :SCRIPT
  end
  IRB.start
end

