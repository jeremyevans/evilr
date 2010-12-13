$:.unshift(File.join(File.dirname(File.dirname(File.expand_path(__FILE__)))), 'lib')
require 'evilr'

describe "Object#share_singleton_class" do
  before do
    @o1 = Class.new.new
    @o2 = Class.new.new
    def @o2.a; 1; end
  end
  after do
    GC.start # GC after spec to make sure nothing broke
  end

  specify "should share singleton class with argument" do
    @o1.share_singleton_class(@o2)
    @o1.a.should == 1
    def @o2.b; 2; end
    def @o2.c; b; end
    @o1.b.should == 2
    @o1.c.should == 2
  end

  specify "should create a singleton class if it doesn't exist" do
    @o2.share_singleton_class(@o1)
    def @o1.a; 3; end
    @o2.a.should == 3
  end
end
