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

describe "Object#include_singleton_class" do
  after{GC.start}
  before do
    @o1 = Class.new.new
    @o2 = Class.new.new
    def @o2.a; 1; end
  end

  specify "should include argument's singleton class as a module in receiver's singleton class" do
    @o1.include_singleton_class(@o2)
    @o1.a.should == 1
    def @o2.b; 2; end
    def @o2.c; b; end
    @o1.b.should == 2
    @o1.c.should == 2

    def @o1.c; 3; end
    @o2.c.should == 2
    @o1.c.should == 3
  end

  specify "should create a singleton class if it doesn't exist" do
    @o2.include_singleton_class(@o1)
    def @o1.a; 3; end
    @o2.a.should == 3
  end

  specify "should raise an exception for immediate objects" do
    [0, :a, true, false, nil].each do |x|
      proc{x.include_singleton_class(@o2)}.should raise_error(TypeError)
      proc{@o1.include_singleton_class(x)}.should raise_error(TypeError)
    end
  end

  specify "should return self" do
    @o2.include_singleton_class(@o1).should equal(@o2)
  end
end

describe "Object#swap_singleton_class" do
  after{GC.start}
  before do
    @o1 = Class.new.new
    @o2 = Class.new.new
  end

  specify "should swap singleton class with argument" do
    def @o2.a; 1; end
    def @o1.b; 2; end
    @o1.swap_singleton_class(@o2)
    @o1.a.should == 1
    @o2.b.should == 2
    proc{@o1.b}.should raise_error(NoMethodError)
    proc{@o2.a}.should raise_error(NoMethodError)

    def @o2.c; 3; end
    def @o1.d; 4; end
    @o2.c.should == 3
    @o1.d.should == 4
    proc{@o2.d}.should raise_error(NoMethodError)
    proc{@o1.c}.should raise_error(NoMethodError)
  end

  specify "should handle singleton classes that don't exist" do
    @o2.swap_singleton_class(@o1)
    def @o2.c; 3; end
    def @o1.d; 4; end
    @o2.c.should == 3
    @o1.d.should == 4
    proc{@o2.d}.should raise_error(NoMethodError)
    proc{@o1.c}.should raise_error(NoMethodError)
  end

  specify "should raise an exception for immediate objects" do
    [0, :a, true, false, nil].each do |x|
      proc{x.swap_singleton_class(@o2)}.should raise_error(TypeError)
      proc{@o1.swap_singleton_class(x)}.should raise_error(TypeError)
    end
  end

  specify "should return self" do
    @o2.swap_singleton_class(@o1).should equal(@o2)
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

describe "Module#to_class" do
  after{GC.start}

  specify "should return the module in class form" do
    Module.new{def a() 1 end}.to_class.new.a.should == 1
  end

  specify "should accept an optional argument for the new class type" do
    Module.new{def a() 1 end}.to_class(Object).new.a.should == 1
    Module.new{def a() super + 2 end}.to_class(Class.new{def a() 1 end}).new.a.should == 3
  end

  specify "should raise exception if attempting to instantiate a builtin class other than Object" do
    proc{Module.new.to_class(Hash)}.should raise_error(TypeError)
    proc{Module.new.to_class(Array)}.should raise_error(TypeError)
    proc{Module.new.to_class(String)}.should raise_error(TypeError)
  end

  specify "should return copy of self (possibly reparented) if already a class" do
    Class.new{def a() 1 end}.to_class.new.a.should == 1
    Class.new{def a() 1 end}.to_class(Object).new.a.should == 1
    Class.new{def a() super + 2 end}.to_class(Class.new{def a() 1 end}).new.a.should == 3

    osc = Class.new
    c = Class.new(osc)
    sc = Class.new
    c.to_class(sc).superclass.should == sc
    c.superclass.should == osc
  end

  specify "should use Object as the default superclass" do
    Module.new.to_class.superclass.should == Object
  end

  specify "should use given class as the superclass" do
    c = Class.new
    Module.new.to_class(c).superclass.should == c
  end
end

describe "Class#to_module" do
  after{GC.start}

  specify "should return the class in module form" do
    c = Class.new{def a() 1 end}
    Class.new{include c.to_module}.new.a.should == 1
  end

  specify "should handle singleton classes without modifying them" do
    o = Object.new
    o.instance_eval{def a() 1 end}
    Class.new{include (class << o; self; end).to_module}.new.a.should == 1
    o.instance_eval{def a() super end}
    proc{o.a}.should raise_error(NoMethodError)
  end
end

describe "Class#detach_singleton" do
  after{GC.start}

  specify "should be a no-op on a non-singleton class" do
    Class.new{def a() 1 end}.detach_singleton.new.a.should == 1
  end

  specify "should detach singleton class from object" do
    o = Object.new
    o.instance_eval{def a() 1 end}
    sc = (class << o; self; end)
    sc.detach_singleton.singleton_class_instance.should == nil
  end

  specify "should allow another singleton class to be created" do
    o = Object.new
    o.instance_eval{def a() 1 end}
    sc = (class << o; self; end)
    sc.detach_singleton
    o.instance_eval{def a() super + 2 end}
    o.a.should == 3
  end
end

describe "Object#detach_singleton_class" do
  after{GC.start}

  specify "should raise an exception for immediate values" do
    proc{nil.detach_singleton_class}.should raise_error(TypeError)
  end

  specify "should be a no-op an object without a singleton class" do
    a = {:a=>1}
    a.detach_singleton_class
    a[:a].should == 1
  end

  specify "should return the class" do
    Object.new.detach_singleton_class.should == Object
    o = Object.new
    sc = (class << o; self; end)
    o.detach_singleton_class.should == sc
  end

  specify "should remove singleton status from singleton class" do
    o = Object.new
    o.instance_eval{def a() 1 end}
    o.detach_singleton_class.new.a.should == 1
  end

  specify "should detach singleton class from object, allowing new singleton class to be created" do
    o = Object.new
    o.instance_eval{def a() 1 end}
    o.detach_singleton_class
    o.instance_eval{def a() super + 2 end}
    o.a.should == 3
  end
end

describe "Class#singleton_class_instance" do
  after{GC.start}

  specify "should return nil for a non-singleton class" do
    Class.new.singleton_class_instance.should == nil
  end

  specify "should return instance attached to singleton class" do
    o = Object.new.instance_eval{def a() 1 end}
    (class << o; self; end).singleton_class_instance.should equal(o)
  end
end

describe "Object#flags" do
  after{GC.start}

  specify "should raise an exception for immediate values" do
    proc{nil.flags}.should raise_error(TypeError)
  end

  specify "should return a Fixnum" do
    Object.new.flags.should be_a_kind_of(Fixnum)
    Class.new.flags.should be_a_kind_of(Fixnum)
    Module.new.flags.should be_a_kind_of(Fixnum)
    Object.new.flags.should_not == Class.new.flags
    Module.new.flags.should_not == Class.new.flags
  end
end
