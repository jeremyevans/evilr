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

describe "Object#dup_singleton_class" do
  after{GC.start}

  specify "should raise an exception for immediate values" do
    proc{nil.dup_singleton_class}.should raise_error(TypeError)
  end

  specify "should return nil if the object has no singleton class" do
    Object.new.dup_singleton_class.should == nil
  end

  specify "should return a copy of the singleton class as a non-singleton class" do
    o = Object.new
    o.instance_eval{def a() 1 end}
    o.dup_singleton_class.new.a.should == 1
  end

  specify "should make the given class a subclass of Object by default" do
    o = Object.new
    o.instance_eval{def a() 1 end}
    o.dup_singleton_class.superclass.should == Object
  end

  specify "should accept a class argument to make the dup a subclass of" do
    o = Object.new
    o.instance_eval{def a() 1 end}
    c = Class.new
    o.dup_singleton_class(c).superclass.should == c
  end

  specify "should handle extended modules by making them included modules in the class" do
    o = Object.new
    o.instance_eval{def a() [1] + super end}
    o.extend(Module.new{def a() [2] + super end})
    c = Class.new{def a() [4] + super end; include Module.new{def a() [8] end}}
    o.dup_singleton_class(c).new.a.should == [1, 2, 4, 8]
  end
end

describe "Object\#{push,pop}_singleton_class" do
  after{GC.start}

  specify "both should raise an exception for immediate values" do
    proc{nil.push_singleton_class(Class.new)}.should raise_error(TypeError)
    proc{nil.pop_singleton_class}.should raise_error(TypeError)
  end

  specify "push_singleton_class should raise an exception if a class is not given" do
    proc{{}.push_singleton_class(Object.new)}.should raise_error(TypeError)
  end

  specify "both should return the class" do
    c = Class.new
    o = {}
    o.push_singleton_class(c).should == c
    o.pop_singleton_class.should == c
  end

  specify "pop_singleton_class should return nil if no singleton class exists" do
    {}.pop_singleton_class.should == nil
  end

  specify "should add and remove singleton classes" do
    o = Class.new{def a() [1] end}.new
    o.a.should == [1]
    o.push_singleton_class(Class.new{def a() [2] + super end})
    o.a.should == [2, 1]
    o.push_singleton_class(Class.new{def a() [4] + super end})
    o.a.should == [4, 2, 1]
    o.push_singleton_class(Class.new{def a() [8] + super end})
    o.a.should == [8, 4, 2, 1]
    o.pop_singleton_class
    o.a.should == [4, 2, 1]
    o.push_singleton_class(Class.new{def a() [16] + super end})
    o.a.should == [16, 4, 2, 1]
    o.pop_singleton_class
    o.a.should == [4, 2, 1]
    o.pop_singleton_class
    o.a.should == [2, 1]
    o.pop_singleton_class
    o.a.should == [1]
  end

  specify "should handle modules included in the classes" do
    o = Class.new{def a() [1] end}.new
    o.a.should == [1]
    o.push_singleton_class(Class.new{def a() [2] + super end; include Module.new{def a() [32] + super end}})
    o.a.should == [2, 32, 1]
    o.push_singleton_class(Class.new{def a() [4] + super end; include Module.new{def a() [64] + super end}})
    o.a.should == [4, 64, 2, 32, 1]
    o.push_singleton_class(Class.new{def a() [8] + super end; include Module.new{def a() [128] + super end}})
    o.a.should == [8, 128, 4, 64, 2, 32, 1]
    o.pop_singleton_class
    o.a.should == [4, 64, 2, 32, 1]
    o.push_singleton_class(Class.new{def a() [16] + super end; include Module.new{def a() [256] + super end}})
    o.a.should == [16, 256, 4, 64, 2, 32, 1]
    o.pop_singleton_class
    o.a.should == [4, 64, 2, 32, 1]
    o.pop_singleton_class
    o.a.should == [2, 32, 1]
    o.pop_singleton_class
    o.a.should == [1]
  end

  specify "pop_singleton_class should have no effect if the object has no singleton class" do
    o = Class.new{def a() [1] end}.new
    o.pop_singleton_class
    o.a.should == [1]
  end
end

describe "Objec#remove_singleton_classes" do
  after{GC.start}

  specify "should raise an exception for immediate values" do
    proc{nil.remove_singleton_classes}.should raise_error(TypeError)
  end

  specify "should return nil" do
    {}.remove_singleton_classes.should == nil
  end

  specify "should add and remove singleton classes" do
    o = Class.new{def a() [1] end}.new
    o.a.should == [1]
    o.push_singleton_class(Class.new{def a() [2] + super end})
    o.a.should == [2, 1]
    o.push_singleton_class(Class.new{def a() [4] + super end})
    o.a.should == [4, 2, 1]
    o.push_singleton_class(Class.new{def a() [8] + super end})
    o.a.should == [8, 4, 2, 1]
    o.remove_singleton_classes
    o.a.should == [1]
  end

  specify "should handle modules included in the classes" do
    o = Class.new{def a() [1] end}.new
    o.a.should == [1]
    o.push_singleton_class(Class.new{def a() [2] + super end; include Module.new{def a() [32] + super end}})
    o.a.should == [2, 32, 1]
    o.push_singleton_class(Class.new{def a() [4] + super end; include Module.new{def a() [64] + super end}})
    o.a.should == [4, 64, 2, 32, 1]
    o.push_singleton_class(Class.new{def a() [8] + super end; include Module.new{def a() [128] + super end}})
    o.a.should == [8, 128, 4, 64, 2, 32, 1]
    o.remove_singleton_classes
    o.a.should == [1]
  end

  specify "should have no effect if the object has no singleton classes" do
    o = Class.new{def a() [1] end}.new
    o.remove_singleton_classes
    o.a.should == [1]
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

  specify "should keep existing class hierarchy the same, other than the singleton classes" do
    oc1 = Class.new
    oc2 = Class.new
    o1 = oc1.new
    o2 = oc2.new
    o1.swap_singleton_class(o2)
    o1.class.should == oc1
    o2.class.should == oc2
  end

  specify "should also swap modules that extend the class" do
    oc1 = Class.new{def a() [16] end}
    oc2 = Class.new{def a() [32] end}
    o1 = oc1.new
    o2 = oc2.new
    o1.instance_eval{def a() [1] + super end; extend Module.new{def a() [2] + super end}}
    o2.instance_eval{def a() [4] + super end; extend Module.new{def a() [8] + super end}}
    o1.swap_singleton_class(o2)
    o1.a.should == [4, 8, 16]
    o2.a.should == [1, 2, 32]
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

  specify "should not modify the class's superclass" do
    c = Class.new{def a() [1] end}
    sc = Class.new(c){def a() [2] + (super rescue [0]) end}
    Class.new{include sc.to_module}.new.a.should == [2, 0]
    sc.superclass.should == c
    sc.new.a.should == [2, 1]
  end

  specify "should not modify the class's superclass if modules are included" do
    c = Class.new{def a() [1] end}
    sc = Class.new(c){def a() [2] + (super rescue [0]) end; include Module.new}
    Class.new{include sc.to_module}.new.a.should == [2, 0]
    sc.superclass.should == c
    sc.new.a.should == [2, 1]
  end

  specify "should handle the order of multiple included modules correctly" do
    c = Class.new{def a() [1] end}
    sc = Class.new(c){def a() [2] + (super rescue [0]) end}
    sc.send :include, Module.new{def a() [4] + (super rescue [0]) end}
    sc.send :include, Module.new{def a() [8] + (super rescue [0]) end}
    Class.new{include sc.to_module}.new.a.should == [2, 8, 4, 0]
    sc.superclass.should == c
    sc.new.a.should == [2, 8, 4, 1]
  end

  specify "should handle singleton classes without modifying them" do
    o = Object.new
    o.instance_eval{def a() 1 end}
    Class.new{include (class << o; self; end).to_module}.new.a.should == 1
    o.instance_eval{def a() super end}
    proc{o.a}.should raise_error(NoMethodError)
  end

  specify "should handle singleton classes without modifying them if modules are included" do
    o = Object.new
    o.instance_eval{def a() 1 end}
    o.extend Module.new
    Class.new{include (class << o; self; end).to_module}.new.a.should == 1
    o.instance_eval{def a() super end}
    proc{o.a}.should raise_error(NoMethodError)
  end

  specify "should include modules included in class" do
    c = Class.new{def a() [1] + super end; include Module.new{def a() [2] + (super rescue [0]) end}}
    Class.new{def a() [4] + super end; include c.to_module}.new.a.should == [4, 1, 2, 0]
  end

  specify "should not include superclass or modules included in superclass" do
    c = Class.new{def a() [1] + super end; include Module.new{def a() [2] end}}
    sc = Class.new(c){def a() [4] + super end; include Module.new{def a() [8] + (super rescue [0]) end}}
    Class.new{def a() [16] + super end; include sc.to_module}.new.a.should == [16, 4, 8, 0]
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

describe "Object#set_singleton_class" do
  after{GC.start}

  specify "should raise an exception for immediate values" do
    proc{nil.set_singleton_class(Class.new)}.should raise_error(TypeError)
  end

  specify "should raise an exception for non-class arguments" do
    proc{Object.new.set_singleton_class(Object.new)}.should raise_error(TypeError)
  end

  specify "should return the class" do
    c = Class.new
    Object.new.set_singleton_class(c).should == c
  end

  specify "should make object the new singleton class's instance" do
    o = Object.new
    o.set_singleton_class(Class.new).singleton_class_instance.should == o
  end

  specify "should make class the object's singleton class" do
    o = Object.new
    c = Class.new{def a() 1 end}
    o.set_singleton_class(c)
    (class << o; self; end).should == c
    o.a.should == 1
  end

  specify "should replace an existing singleton class" do
    o = Object.new
    o.instance_eval{def a() 3 end}
    c = Class.new{def a() 1 + (super rescue 10) end}
    o.set_singleton_class(c)
    o.a.should == 11
  end

  specify "should remove any modules currently extending the class" do
    o = Object.new
    o.instance_eval{def a() 3 end}
    o.extend(Module.new{def a() 4 end})
    c = Class.new{def a() 1 + (super rescue 10) end}
    o.set_singleton_class(c)
    o.a.should == 11
  end

  specify "should keep any existing class" do
    oc = Class.new
    o = oc.new
    c = Class.new
    sc = Class.new(c)
    o.set_singleton_class(sc)
    o.class.should == oc
  end

  specify "should make modules included in class as modules that extend the new class" do
    oc = Class.new{def a() [1] end}
    o = oc.new
    c = Class.new{def a() [2] + super end}
    sc = Class.new(c){def a() [4] + super end; include Module.new{def a() [8] + super end}}
    o.set_singleton_class(sc)
    o.a.should == [4, 8, 1]
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

  specify "should detach singleton class from object, becoming the object's actual class" do
    o = Object.new
    o.instance_eval{def a() 1 end}
    sc = (class << o; self; end)
    o.detach_singleton_class
    o.class.should == sc
  end

  specify "should have singleton class's methods remain in the method chain" do
    o = Object.new
    o.instance_eval{def a() 1 end}
    o.detach_singleton_class
    o.instance_eval{def a() super + 2 end}
    o.a.should == 3
  end
end

describe "Object#remove_singleton_class" do
  after{GC.start}

  specify "should raise an exception for immediate values" do
    proc{nil.remove_singleton_class}.should raise_error(TypeError)
  end

  specify "should be a no-op an object without a singleton class" do
    a = {:a=>1}
    a.remove_singleton_class
    a[:a].should == 1
  end

  specify "should return nil if the class does not exist" do
    Object.new.remove_singleton_class.should == nil
  end

  specify "should return the class if it exists" do
    o = Object.new
    sc = (class << o; self; end)
    o.remove_singleton_class.should == sc
  end

  specify "should remove singleton status from singleton class" do
    o = Object.new
    o.instance_eval{def a() 1 end}
    o.remove_singleton_class.new.a.should == 1
  end

  specify "should remove singleton class from object, restoring the object's original class" do
    o = Object.new
    o.instance_eval{def a() 1 end}
    sc = (class << o; self; end)
    o.remove_singleton_class
    o.class.should == Object
  end

  specify "should have singleton class's methods not remain in the method chain" do
    o = Object.new
    o.instance_eval{def a() 1 end}
    o.remove_singleton_class
    o.instance_eval{def a() super + 2 end}
    proc{o.a}.should raise_error(NoMethodError)
  end

  specify "should handle modules that extend the object" do
    o = Object.new
    o.instance_eval{def a() 1 end}
    o.extend(Module.new{def b() 2 end})
    o.remove_singleton_class
    proc{o.a}.should raise_error(NoMethodError)
    proc{o.b}.should raise_error(NoMethodError)
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

describe "Class#superclass=" do
  after{GC.start}

  specify "should raise an exception for immediate values" do
    proc{Class.new.superclass = nil}.should raise_error(TypeError)
  end

  specify "should raise an exception for non-class arguments" do
    proc{Class.new.superclass = Object.new}.should raise_error(TypeError)
  end

  specify "should raise an exception for incompatible types" do
    proc{Class.new.superclass = String}.should raise_error(TypeError)
  end

  specify "should change the superclass of the class" do
    c = Class.new
    c2 = Class.new
    c.superclass.should == Object
    c.superclass = c2
    c.superclass.should == c2
  end

  specify "should keep any included modules" do
    c = Class.new{def a() [1] + super end; include Module.new{def a() [2] + (super rescue [0]) end}}
    c2 = Class.new{def a() [4] + super end; include Module.new{def a() [8] + (super rescue [0]) end}}
    c.new.a.should == [1, 2, 0]
    c2.new.a.should == [4, 8, 0]
    c.superclass = c2
    c.new.a.should == [1, 2, 4, 8, 0]
  end

  specify "should ignore existing superclasses and modules included in them" do
    c = Class.new{def a() [1] + super end; include Module.new{def a() [2] + (super rescue [0]) end}}
    c2 = Class.new(c){def a() [4] + super end; include Module.new{def a() [8] + (super rescue [0]) end}}
    c2.new.a.should == [4, 8, 1, 2, 0]
    c2.superclass = Object
    c2.new.a.should == [4, 8, 0]
  end
end

describe "Class#inherit" do
  after{GC.start}

  specify "should raise an exception for immediate values" do
    proc{Class.new.inherit nil}.should raise_error(TypeError)
  end

  specify "should raise an exception for non-class arguments" do
    proc{Class.new.inherit Object.new}.should raise_error(TypeError)
  end

  specify "should raise an exception for incompatible types" do
    proc{Class.new.inherit String}.should raise_error(TypeError)
  end

  specify "should include the classes as modules" do
    c = Class.new{def a() [1] + (super rescue [0]) end}
    c2 = Class.new{def a() [2] + super end}
    sc = Class.new{def a() [4] + super end; inherit c, c2}
    sc.new.a.should == [4, 2, 1, 0]
  end

  specify "should keep any included modules" do
    c = Class.new{def a() [1] + super end; include Module.new{def a() [2] + (super rescue [0]) end}}
    c2 = Class.new{def a() [4] + super end; include Module.new{def a() [8] + (super rescue [0]) end}}
    sc = Class.new{def a() [16] + super end; inherit c, c2}
    sc.new.a.should == [16, 4, 8, 1, 2, 0]
  end

  specify "should ignore superclasses of arguments, and keep superclass of current class" do
    c = Class.new{def a() [1] + super end; include Module.new{def a() [2] + (super rescue [0]) end}}
    sc1 = Class.new(c){def a() [4] + super end; include Module.new{def a() [8] + (super rescue [0]) end}}
    sc2 = Class.new(c){def a() [16] + super end; include Module.new{def a() [32] + (super rescue [0]) end}}
    Class.new{def a() [64] + super end; inherit sc1, sc2}.new.a.should == [64, 16, 32, 4, 8, 0]
    Class.new(sc2){def a() [64] + super end; inherit sc1}.new.a.should == [64, 4, 8, 16, 32, 1, 2, 0]
    Class.new(sc2){def a() [64] + super end; inherit sc1}.superclass.should == sc2
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
