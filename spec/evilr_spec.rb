$:.unshift(File.join(File.dirname(File.dirname(File.expand_path(__FILE__)))), 'lib')
require 'evilr'

describe "Object#share_singleton_class" do
  before do
    @o1 = Class.new.new
    @o2 = Class.new.new
    def @o2.a; 1; end
  end

  specify "should share singleton class with argument" do
    @o1.share_singleton_class(@o2)
    @o1.a.should == 1
    def @o2.b; 2; end
    def @o2.c; b; end
    @o1.b.should == 2
    @o1.c.should == 2
  end
end
