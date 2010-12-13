$:.unshift(File.join(File.dirname(File.dirname(File.expand_path(__FILE__)))), 'lib')
require 'evilr'

describe "Object#class=" do
  after{GC.start} # GC after spec to make sure nothing broke
  before do
    @o = Class.new.new
    @c = Class.new
  end

  specify "should make given object this object's class" do
    @o.should_not be_a_kind_of(@c)
    @o.class = @c
    @o.should be_a_kind_of(@c)
  end

  specify "should raise an exception for immediate objects" do
    [0, :a, true, false, nil].each do |x|
      proc{x.class = @c}.should raise_error(TypeError)
    end
  end

  specify "should raise an exception if a class is not used as an argument" do
    proc{@o.class = @o}.should raise_error(TypeError)
  end

  specify "should raise an exception if a class used is not the same underlying type" do
    proc{@o.class = Hash}.should raise_error(TypeError)
  end
end

describe "Object#share_singleton_class" do
  after{GC.start}
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

  specify "should create a singleton class if it doesn't exist" do
    @o2.share_singleton_class(@o1)
    def @o1.a; 3; end
    @o2.a.should == 3
  end

  specify "should raise an exception for immediate objects" do
    [0, :a, true, false, nil].each do |x|
      proc{x.share_singleton_class(@o2)}.should raise_error(TypeError)
      proc{@o1.share_singleton_class(x)}.should raise_error(TypeError)
    end
  end

  specify "should return self" do
    @o2.share_singleton_class(@o1).should equal(@o2)
  end
end

describe "Object#unfreeze" do
  after{GC.start}
  before do
    @o = Object.new
    @o.freeze
  end

  specify "should unfreeze object" do
    @o.unfreeze
    @o.frozen?.should == false
    @o.instance_variable_set(:@a, 1)
    @o.instance_variable_get(:@a).should == 1
  end

  specify "should be idempotent" do
    @o.frozen?.should == true
    @o.unfreeze
    @o.frozen?.should == false
    @o.unfreeze
    @o.frozen?.should == false
    @o.instance_variable_set(:@a, 1)
    @o.instance_variable_get(:@a).should == 1
  end

  specify "should be no-op for immediate objects" do
    [0, :a, true, false, nil].each do |x|
      proc{x.unfreeze}.should_not raise_error
    end
  end

  specify "should not be allowed if $SAFE > 0" do
    x = @o
    Thread.new do
      $SAFE  = 1
      proc{x.unfreeze}.should raise_error(SecurityError)
    end.join
  end

  specify "should return self" do
    @o.unfreeze.should equal(@o)
  end
end

describe "Kernel#set_safe_level" do
  after{GC.start}

  specify "should allow the lowering of the $SAFE level" do
    $SAFE = 1
    set_safe_level(0)
    $SAFE.should == 0
  end
end
