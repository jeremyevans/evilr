#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#include <ruby.h> 
#ifdef RUBY19
#include <ruby/st.h> 
#else
#include <st.h> 
#endif

/* ruby uses a magic number for this, arguably the only time
 * it is more evil that evilr. */
#ifndef SAFE_LEVEL_MAX
#define SAFE_LEVEL_MAX 4
#endif

/* Partial struct definition == evil */
struct METHOD {
    VALUE recv;
    VALUE rclass;
    ID id;
};

#ifdef RUBY19
struct BLOCK {
  VALUE self;
  VALUE *lfp;
  VALUE *dfp;
  void *block_iseq;
  VALUE proc;
};
#else
/* More partial definition evil */
struct BLOCK {
  void *var;
  void *body;
  VALUE self;
};
#endif

/* Ruby 1.8.6 support */
#ifndef RCLASS_SUPER
#define RCLASS_SUPER(c) RCLASS(c)->super
#endif
#ifndef RCLASS_IV_TBL
#define RCLASS_IV_TBL(c) RCLASS(c)->iv_tbl
#endif
#ifndef RCLASS_M_TBL
#define RCLASS_M_TBL(c) RCLASS(c)->m_tbl
#endif

#define OBJECT_SIZE sizeof(struct RClass)
#define RBASIC_SET_KLASS(o, c) (RBASIC(o)->klass = c)
#define RBASIC_KLASS(o) (RBASIC(o)->klass)
#define RBASIC_FLAGS(o) (RBASIC(o)->flags)
#define IS_SINGLETON_CLASS(o) (FL_TEST(o, FL_SINGLETON))
#define HAS_SINGLETON_CLASS(o) (FL_TEST(RBASIC_KLASS(o), FL_SINGLETON))

#ifdef RUBY19
#define RCLASS_SET_SUPER(o, c) (RCLASS(o)->ptr->super = c)
#else
#define RCLASS_SET_SUPER(o, c) (RCLASS(o)->super = c)
#define ROBJECT_IVPTR(o) (ROBJECT(o)->iv_tbl)
extern int ruby_safe_level;
#endif

VALUE empty;
ID evilr__attached;
ID evilr__bind;
ID evilr__clone;

/* Helper functions */

/* Raise TypeError if an immediate value is given. */
static void evilr__check_immediate(VALUE self) {
  if (SPECIAL_CONST_P(self)) {
    rb_raise(rb_eTypeError, "can't use immediate value");
  }
}

/* Raise TypeError if either self or other is an immediate value. */
static void evilr__check_immediates(VALUE self, VALUE other) {
  evilr__check_immediate(self);
  evilr__check_immediate(other);
}

/* Raise TypeError if self doesn't have the given ruby internal type
 * number (e.g. T_OBJECT). */
static void evilr__check_type(unsigned int type, VALUE self) {
  if (BUILTIN_TYPE(self) != type) {
    rb_raise(rb_eTypeError, "incompatible type used");
  }
}

/* Return the ruby internal type number that instances of the
 * given class use. */
static unsigned int evilr__class_type(VALUE klass) {
  evilr__check_type(T_CLASS, klass);
  return BUILTIN_TYPE(rb_obj_alloc(klass));
}

/* Raise TypeError if instances of the given class don't have the
 * given ruby internal type number. */
static void evilr__check_class_type(unsigned int type, VALUE self) {
  if (evilr__class_type(self) != type) {
    rb_raise(rb_eTypeError, "incompatible type used");
  }
}

/* Raise TypeError if the given value has the ruby internal type number T_DATA. */
static void evilr__check_data_type(VALUE self) {
  if (BUILTIN_TYPE(self) == T_DATA) {
    rb_raise(rb_eTypeError, "incompatible type used");
  }
}

/* Return the next class in the super chain, skipping any iclasses (included modules).
 * Returns NULL if this is the last class in the super chain. */
static VALUE evilr__next_class(VALUE klass) {
  VALUE c;
  for (c = RCLASS_SUPER(klass); c && BUILTIN_TYPE(c) != T_CLASS; c = RCLASS_SUPER(c)); /* empty */
  return c;
}

/* If the given class includes no modules, return it.  Otherwise, return the last iclass
 * before the next class in the super chain. */
static VALUE evilr__iclass_before_next_class(VALUE klass) {
  VALUE c, i = NULL;
  for (c = RCLASS_SUPER(klass); c && BUILTIN_TYPE(c) != T_CLASS; i = c, c = RCLASS_SUPER(c)); /* empty */
  return i == NULL ? klass : i;
}

/* Walk the super chain from klass until either before or an iclass for mod is encountered.  If
 * before is encountered first, return NULL.  If an iclass for mod is encountered first, return
 * the iclass. */
static VALUE evilr__iclass_matching_before(VALUE klass, VALUE mod, VALUE before) {
  VALUE c;
  for (c = RCLASS_SUPER(klass); c && c != before; c = RCLASS_SUPER(c)) {
    if (BUILTIN_TYPE(c) == T_ICLASS && RBASIC_KLASS(c) == mod) {
      return c;
    }
  }
  return NULL;
}

/* If there is an iclass for mod anywhere in the super chain of klass, return the iclass.
 * Otherwise, return NULL. */
static VALUE evilr__iclass_matching(VALUE klass, VALUE mod) {
  return evilr__iclass_matching_before(klass, mod, NULL);
}

/* If self has a singleton class, set the superclass of the
 * singleton class to the given klass, keeping all modules
 * that are included in the singleton class.  Otherwise, set the
 * object's klass to the given klass. */
void evilr__reparent_singleton_class(VALUE self, VALUE klass) {
  VALUE self_klass = RBASIC_KLASS(self);

  if (IS_SINGLETON_CLASS(self_klass)) {
    RCLASS_SET_SUPER(evilr__iclass_before_next_class(self_klass), klass);
    rb_clear_cache_by_class(self_klass);
  } else {
    RBASIC_SET_KLASS(self, klass);
  }
}

/* Set the superclass of self to the given klass, keeping all
 * modules that are included in the class. */
void evilr__reparent_class(VALUE self, VALUE klass) {
  RCLASS_SET_SUPER(evilr__iclass_before_next_class(self), klass);
  rb_clear_cache_by_class(self);
}

/* Raise TypeError if self is an immediate value or if klass is
 * not a Class. */
void evilr__check_obj_and_class(VALUE self, VALUE klass) {
  evilr__check_immediates(self, klass);
  evilr__check_type(T_CLASS, klass);
}

/* If no arguments are given, return Object.  If an argument is
 * given and it is a class object whose instances use the ruby
 * internal class number T_OBJECT, return that class.  If an argument
 * is given and it isn't a class or it's instances don't use T_OBJECT,
 * return a TypeError.  Otherwise, return an ArgumentError. */
static VALUE evilr__optional_class(int argc, VALUE *argv) {
  VALUE klass;

  switch(argc) {
    case 0:
      klass = rb_cObject;
      break;
    case 1:
      klass = argv[0];
      evilr__check_class_type(T_OBJECT, klass);
      break;
    default:
      rb_raise(rb_eArgError, "wrong number of arguments: %i for 1", argc);
      break;
  }
  return klass;
}

/* Make the given klass the singleton class of self. */
void evilr__make_singleton(VALUE self, VALUE klass) {
  FL_SET(klass, FL_SINGLETON);
  RBASIC_SET_KLASS(self, klass);
  rb_singleton_class_attached(klass, self);
}

/* Check that super is a Class object whose instances use the same internal
 * type number as instances of klass, and that the instances don't use
 * type number T_DATA. */
void evilr__check_compatible_classes(VALUE klass, VALUE super) {
  evilr__check_immediate(super);
  evilr__check_type(T_CLASS, super);
  evilr__check_class_type(evilr__class_type(klass), super);
  evilr__check_data_type(rb_obj_alloc(klass));
}

/* Walk the super chain starting with the given iclass and include
 * the modules related to each iclass into the mod such that the
 * order of the initial iclass super chain and mod's super chain are
 * the same. */
void evilr__include_iclasses(VALUE mod, VALUE iclass) {
  if (iclass && BUILTIN_TYPE(iclass) == T_ICLASS) {
    evilr__include_iclasses(mod, RCLASS_SUPER(iclass));
    rb_include_module(mod, RBASIC_KLASS(iclass));
  }
  rb_clear_cache_by_class(mod);
}

/* Ruby methods */

/* call-seq:
 *   class=(klass) -> Object
 *
 * Modifies the receiver's class to be +klass+.  The receiver's current class
 * and any singleton class (and any modules that extend the object) are
 * ignored.  If the receiver is an immediate or instances of +klass+ don't use the
 * same internal type as the receiver, a +TypeError+ is raised.
 */
static VALUE evilr_class_e(VALUE self, VALUE klass) {
  evilr__check_immediate(self);
  evilr__check_type(evilr__class_type(klass), self);
  evilr__check_data_type(self);

  RBASIC_SET_KLASS(self, klass);
  return self;
}

/* call-seq:
 *   evilr_debug_print -> nil
 *
 * Prints to stdout the receiver and all entries in the receiver's klass's super chain,
 * using the pointers of the current entry, it's klass, iv_tbl, m_tbl, and super entry,
 * as well as the entry's flags.  If Class or Module is given, uses their
 * super chain, not the super chain of their klass. If the receiver is an immediate value,
 * a +TypeError+ is raised. */
static VALUE evilr_debug_print(VALUE self) {
  if (self == NULL) {
    return Qnil;
  }
  evilr__check_immediate(self);
  switch(BUILTIN_TYPE(self)) {
    case T_CLASS:
    case T_ICLASS:
    case T_MODULE:
      printf("self %p klass %p flags 0x%lx iv_tbl %p m_tbl %p super %p\n", (void *)self, (void *)RBASIC_KLASS(self), RBASIC_FLAGS(self), (void *)RCLASS_IV_TBL(self), (void *)RCLASS_M_TBL(self), (void *)RCLASS_SUPER(self));
      self = RCLASS_SUPER(self);
      break;
    default:
      printf("self %p klass %p flags 0x%lx iv_tbl/ptr %p\n", (void *)self, (void *)RBASIC_KLASS(self), RBASIC_FLAGS(self), (void *)ROBJECT_IVPTR(self));
      self = RBASIC_KLASS(self);
      break;
  }
  return evilr_debug_print(self);
}


/* call-seq:
 *   swap(other) -> self
 * 
 * Swap the contents of the receiver with +other+:
 *
 *   a = []
 *   b = {}
 *   a.swap(b) # => {}
 *   a # => {}
 *   b # => []
 *
 * You cannot swap a Class or Module except with another
 * Class or Module, and you can only swap a Class with a Class and
 * a Module with a Module (no swapping a Class with Module), and you
 * cannot swap immediate values.  If an invalid swap attempt is
 * detected, a  +TypeError+ is raised.*/
static VALUE evilr_swap(VALUE self, VALUE other) {
  char tmp[OBJECT_SIZE];
  evilr__check_immediates(self, other);
  if ((BUILTIN_TYPE(self) == T_MODULE || BUILTIN_TYPE(self) == T_CLASS ||
       BUILTIN_TYPE(other) == T_MODULE || BUILTIN_TYPE(other) == T_CLASS) &&
       BUILTIN_TYPE(self) != BUILTIN_TYPE(other)) {
    rb_raise(rb_eTypeError, "incompatible types used");
  }
  memcpy(tmp, ROBJECT(self), OBJECT_SIZE);
  memcpy(ROBJECT(self), ROBJECT(other), OBJECT_SIZE);
  memcpy(ROBJECT(other), tmp, OBJECT_SIZE);
  return self;
}

/* call-seq:
 *   swap_instance_variables(other) -> self
 * 
 * Swaps only the instance variables of the receiver and +other+.
 * You can only swap the instance variables between two objects that
 * use the internal type number T_OBJECT, or between Classes and Modules.
 * You cannot swap instance variables of immediate values, since they
 * do not have instance variables. Invalid swap attempts will raise
 * +TypeError+. */
static VALUE evilr_swap_instance_variables(VALUE self, VALUE other) {
#ifndef RUBY19
  struct st_table *tmp;
#endif
  evilr__check_immediates(self, other);

  switch(BUILTIN_TYPE(self)) {
    case T_OBJECT:
      if (BUILTIN_TYPE(other) != T_OBJECT) {
        goto bad_types;
      }
      break;
    case T_MODULE:
    case T_CLASS:
      if (BUILTIN_TYPE(other) != T_MODULE && BUILTIN_TYPE(other) != T_CLASS) {
        goto bad_types;
      }
      break;
    default:
bad_types:
      rb_raise(rb_eTypeError, "incompatible types used");
  }

#ifdef RUBY19
  if (BUILTIN_TYPE(self) == T_MODULE || BUILTIN_TYPE(self) == T_CLASS) {
    struct st_table *tmp;
    tmp = RCLASS_IV_TBL(self);
    RCLASS(self)->ptr->iv_tbl = RCLASS_IV_TBL(other);
    RCLASS(other)->ptr->iv_tbl = tmp;
  } else {
    char tmp[OBJECT_SIZE];
    memcpy(tmp, &(ROBJECT(self)->as), sizeof(ROBJECT(tmp)->as));
    memcpy(&(ROBJECT(self)->as), &(ROBJECT(other)->as), sizeof(ROBJECT(self)->as));
    memcpy(&(ROBJECT(other)->as), tmp, sizeof(ROBJECT(other)->as));
  }
#else
  /* RClass and RObject have iv_tbl at same position in the structure
   * so no funny business is needed */
  tmp = ROBJECT_IVPTR(self);
  ROBJECT(self)->iv_tbl = ROBJECT_IVPTR(other);
  ROBJECT(other)->iv_tbl = tmp;
#endif
  return self;
}

/* call-seq:
 *   swap_method_tables(other) -> self
 * 
 * Swap the method table of the receiver with the method table of the given
 * class or module.  If +other+ is not a class or module, raise a +TypeError+. */
static VALUE evilr_swap_method_tables(VALUE self, VALUE other) {
  struct st_table *tmp;

  evilr__check_immediate(other);
  if(BUILTIN_TYPE(other) != T_MODULE && BUILTIN_TYPE(other) != T_CLASS) {
    rb_raise(rb_eTypeError, "non-class or module used");
  }

  tmp = RCLASS_M_TBL(self);
  RCLASS(self)->m_tbl = RCLASS_M_TBL(other);
  RCLASS(other)->m_tbl = tmp;
  rb_clear_cache_by_class(self);
  rb_clear_cache_by_class(other);
  return self;
}

/* call-seq:
 *   swap_singleton_class(other) -> self
 * 
 * Swap the singleton classes of the receiver and +other+.  If either
 * the receiver or +other+ is an immediate, a +TypeError+ is raised.
 * If either object does not have a singleton class, an empty singleton
 * class is created for it before swapping. Any modules that extend
 * either object are swapped as well.
 * */
static VALUE evilr_swap_singleton_class(VALUE self, VALUE other) {
  VALUE tmp;

  evilr__check_immediates(self, other);

  /* Create singleton classes to be swapped if they doesn't exist */
  (void)rb_singleton_class(other);
  (void)rb_singleton_class(self);

  tmp = rb_obj_class(other);
  evilr__reparent_singleton_class(other, rb_obj_class(self));
  evilr__reparent_singleton_class(self, tmp);

  tmp = RBASIC_KLASS(self);
  RBASIC_SET_KLASS(self, RBASIC_KLASS(other));
  RBASIC_SET_KLASS(other, tmp);

  /* Attach each singleton class to its object */
  rb_singleton_class_attached(RBASIC_KLASS(self), self);
  rb_singleton_class_attached(RBASIC_KLASS(other), other);

  return self;
}

/* call-seq:
 *   unfreeze -> self
 * 
 * Unfreezes the given object.  Will raise a +SecurityError+ if
 * <tt>$SAFE</tt> > 0.  Has no effect if the object is not yet frozen. */
static VALUE evilr_unfreeze(VALUE self) {
  if (rb_safe_level() > 0) {
    rb_raise(rb_eSecurityError, "can't unfreeze objects when $SAFE > 0");
  }
  FL_UNSET(self, FL_FREEZE);
  return self;
}

/* call-seq:
 *   set_safe_level=(int) -> int
 * 
 * Sets the <tt>$SAFE</tt> level to the given integer.  If the number is
 * greater than 4, sets it to 4.  Allows lowering the <tt>$SAFE</tt> level
 * by passing an integer lower than the current level. Returns the value
 * passed in. */
static VALUE evilr_set_safe_level(VALUE self, VALUE safe) {
  int s = NUM2INT(safe);
  if (s > SAFE_LEVEL_MAX) {
    s = SAFE_LEVEL_MAX;
  }
#ifdef RUBY19
  rb_set_safe_level_force(s);
#else
  ruby_safe_level = s;
#endif
  return safe;
}

/* call-seq:
 *   uninclude(mod) -> mod || nil
 *
 * Unincludes the given module +mod+ from the receiver or any of the receiver's
 * ancestors.  Walks the super chain of the receiver, and if an iclass for +mod+ is
 * encountered, the super chain is modified to skip that iclass.  Returns +mod+ if
 * an iclass for mod was present in the super chain, and +nil+ otherwise. If +mod+ is
 * not a Module, a +TypeError+ is raised. */
static VALUE evilr_uninclude(VALUE klass, VALUE mod) {
  VALUE cur, prev;

  evilr__check_immediate(mod);
  evilr__check_type(T_MODULE, mod);

  for (prev = klass, cur = RCLASS_SUPER(klass); cur ; prev = cur, cur = RCLASS_SUPER(cur)) {
    if (BUILTIN_TYPE(prev) == T_CLASS) {
      rb_clear_cache_by_class(prev);
    }
    if (BUILTIN_TYPE(cur) == T_ICLASS && RBASIC_KLASS(cur) == mod) {
      RCLASS_SET_SUPER(prev, RCLASS_SUPER(cur));
      return mod;
    }
  }

  return Qnil;
}

/* call-seq:
 *   unextend(mod) -> mod || nil
 *
 * Unextends the given module +mod+ from the receiver.  If the receiver's class includes the
 * module, does not uninclude it, so this should not affect any other objects besides the
 * receiver.  If +mod+ already extended the object, returns +mod+, otherwise returns +nil+.
 * Raises +TypeError+ if +mod+ is not a Module or if the receiver is an immediate. */
static VALUE evilr_unextend(VALUE self, VALUE mod) {
  VALUE prev, cur;

  evilr__check_immediates(self, mod);
  evilr__check_type(T_MODULE, mod);

  self = rb_singleton_class(self);
  rb_clear_cache_by_class(self);
  for (prev = self, cur = RCLASS_SUPER(self); cur && BUILTIN_TYPE(cur) != T_CLASS; prev = cur, cur = RCLASS_SUPER(cur)) {
    if (BUILTIN_TYPE(cur) == T_ICLASS && RBASIC_KLASS(cur) == mod) {
      RCLASS_SET_SUPER(prev, RCLASS_SUPER(cur));
      return mod;
    }
  }

  return Qnil;
}

#define INCLUDE_BETWEEN_VAL(x) (x) ? (BUILTIN_TYPE(x) == T_ICLASS ? RBASIC_KLASS(x) : (x)) : Qnil
/* call-seq:
 *   include_between(mod){|p, c| } -> mod || nil
 *
 * Walks the receiver's super chain, yielding the previous and current entries in
 * the super chain at every step.  The first time the block returns +true+, +mod+ is
 * inserted into the super chain between the two values, and the method returns
 * immediately.  Raises +TypeError+ if +mod+ is not a Module.
 * If the block ever returns +true+, the return value is +mod+.  If
 * the block never returns +true+, the return value is +nil+.  On the first block call,
 * the first block argument is the receiver, and on the last block call, the last block
 * argument is +nil+. */
static VALUE evilr_include_between(VALUE klass, VALUE mod) {
  VALUE iclass, prev, cur;

  evilr__check_immediate(mod);
  evilr__check_type(T_MODULE, mod);

  /* Create ICLASS for module by inserting it and removing it.
   * If module already in super chain, will change it's position. */
  rb_include_module(klass, mod);
  iclass = evilr__iclass_matching(klass, mod);
  evilr_uninclude(klass, mod);

  for (prev = klass, cur = RCLASS_SUPER(klass); prev ; prev = cur, cur = cur ? RCLASS_SUPER(cur) : cur) {
    if (BUILTIN_TYPE(prev) == T_CLASS) {
      rb_clear_cache_by_class(prev);
    }
    if (rb_yield_values(2, INCLUDE_BETWEEN_VAL(prev), INCLUDE_BETWEEN_VAL(cur)) == Qtrue) {
      RCLASS_SET_SUPER(prev, iclass);
      RCLASS_SET_SUPER(iclass, cur);
      return mod;
    }
  }
  return Qnil;
}

/* call-seq:
 *   extend_between(mod){|p, c| } -> mod || nil
 *
 * Walks the receiver's singleton class's super chain until it reaches the receiver's
 * class, yielding the previous and current entries in the super chain at every step.
 * The first time the block returns +true+, +mod+ is inserted into the super chain
 * between the two values and the method returns immediately.  Raises +TypeError+ if
 * +mod+ is not a Module or if the receiver is an immediate.
 * If the block ever returns +true+, the return value is
 * +mod+.  If the block never returns +true+, the return value is +nil+.  On the first block call,
 * the first block argument is the receiver's singleton class, and on the last block call,
 * the last block argument is the receiver's class. */
static VALUE evilr_extend_between(VALUE self, VALUE mod) {
  VALUE sc, iclass, klass, prev, cur;

  evilr__check_immediates(self, mod);
  evilr__check_type(T_MODULE, mod);

  sc = rb_singleton_class(self);
  klass = rb_obj_class(self);
  rb_extend_object(self, mod);
  iclass = evilr__iclass_matching_before(sc, mod, klass);
  if (iclass == NULL) {
    rb_raise(rb_eArgError, "module already included in object's class");
  }
  evilr_unextend(self, mod);

  for (prev = sc, cur = RCLASS_SUPER(sc); prev && prev != klass; prev = cur, cur = cur ? RCLASS_SUPER(cur) : cur) {
    if (rb_yield_values(2, INCLUDE_BETWEEN_VAL(prev), INCLUDE_BETWEEN_VAL(cur)) == Qtrue) {
      RCLASS_SET_SUPER(prev, iclass);
      RCLASS_SET_SUPER(iclass, cur);
      return mod;
    }
  }
  return Qnil;
}

/* call-seq:
 *   detach_singleton -> self
 *
 * If the receiver is a singleton class, it is transformed into a
 * regular class and it is detached from the instance.  Note that
 * this means it becomes the class of the object to which it was previous
 * attached.  If the receiver is not a singleton class, has no effect.
 * Returns the receiver. */
static VALUE evilr_detach_singleton(VALUE klass) {
  if (IS_SINGLETON_CLASS(klass)) {
    FL_UNSET(klass, FL_SINGLETON);
    if (RCLASS_IV_TBL(klass)) {
      st_delete(RCLASS_IV_TBL(klass), (st_data_t*)&evilr__attached, 0);
    }
  }
  return klass;
}

/* call-seq:
 *   detach_singleton_class -> Class
 * 
 * If the receiver has a singleton class, it is detached from the
 * receiver and becomes the receiver's class.  If the receiver is
 * an immediate, a +TypeError+ is raised. Returns the (possibly new) class
 * of the receiver. */
static VALUE evilr_detach_singleton_class(VALUE self) {
  evilr__check_immediate(self);
  return evilr_detach_singleton(RBASIC_KLASS(self));
}

/* call-seq:
 *   dup_singleton_class(klass=Object) -> Class || nil
 * 
 * If the receiver has a singleton class, a copy of the class is returned,
 * and the superclass of that class is set to the given +klass+.  Any
 * modules that extend the object become modules included in the returned class.
 * If the receiver does not have a singleton class, +nil+ is returned and no
 * changes are made.  If the receiver is an immediate, a +TypeError+ is raised. */
static VALUE evilr_dup_singleton_class(int argc, VALUE *argv, VALUE self) {
  VALUE klass;
  evilr__check_immediate(self);
 
  if (!HAS_SINGLETON_CLASS(self)) {
    return Qnil;
  }
  klass = evilr__optional_class(argc, argv);
  self = rb_singleton_class_clone(self);
  evilr__reparent_class(self, klass);
  FL_UNSET(self, FL_SINGLETON);
  return self;
}

/* call-seq:
 *   push_singleton_class(klass) -> klass
 * 
 * Makes the given class the closest singleton class of the receiver, without changing any
 * existing singleton class relationships.  Designed to be used with +pop_singleton_class+
 * to implement a method table stack on an object.  If the receiver is an immediate or
 * +klass+ is not a class, raises +TypeError+. */
static VALUE evilr_push_singleton_class(VALUE self, VALUE klass) {
  evilr__check_obj_and_class(self, klass);
  evilr__reparent_class(evilr__iclass_before_next_class(klass), RBASIC_KLASS(self));
  evilr__make_singleton(self, klass);
  return klass;
}

/* call-seq:
 *   pop_singleton_class -> Class || nil
 * 
 * Removes the closest singleton class from the receiver and returns it.
 * If the receiver does not have a singleton class, does nothing and returns +nil+.
 * Designed to be used with +push_singleton_class+
 * to implement a method table stack on an object.  If the receiver is an immediate, 
 * raises +TypeError+. */
static VALUE evilr_pop_singleton_class(VALUE self) {
  VALUE klass;

  evilr__check_immediate(self);
  klass = RBASIC_KLASS(self);

  if (IS_SINGLETON_CLASS(klass)) {
    RBASIC_SET_KLASS(self, evilr__next_class(klass));  
  } else {
    klass = Qnil;
  }
  return klass;
}

/* call-seq:
 *   remove_singleton_classes -> nil
 * 
 * Removes all singleton classes from the receiver.  Designed to be used with
 * +push_singleton_class+ and +pop_singleton_class+ to implement a method table
 * stack on an object, this clears the stack.  If the receiver is an immediate,
 * raises +TypeError+. */
static VALUE evilr_remove_singleton_classes(VALUE self) {
  evilr__check_immediate(self);
  RBASIC_SET_KLASS(self, rb_obj_class(self));  
  return Qnil;
}

/* call-seq:
 *   set_singleton_class(klass) -> klass
 * 
 * Makes the given +klass+ the singleton class of the receiver,
 * ignoring any existing singleton class and modules extending the receiver.
 * Modules already included in +klass+ become modules that extend the receiver.
 * If the receiver is an immediate or +klass+ is not a Class,
 * raises +TypeError+. */
static VALUE evilr_set_singleton_class(VALUE self, VALUE klass) {
  evilr__check_obj_and_class(self, klass);
  RCLASS_SET_SUPER(evilr__iclass_before_next_class(klass), rb_obj_class(self));
  rb_clear_cache_by_class(klass);
  evilr__make_singleton(self, klass);
  return klass;
}

/* call-seq:
 *   remove_singleton_class -> klass || nil
 * 
 * Removes the singleton class of the receiver, detaching it from the
 * receiver and making it a regular class.  If the receiver does not
 * currently have a singleton class, returns +nil+.  If the receiver is an
 * immediate, raises +TypeError+. */
static VALUE evilr_remove_singleton_class(VALUE self) {
  VALUE klass;
  evilr__check_immediate(self);

  if (HAS_SINGLETON_CLASS(self)) {
    klass = evilr_detach_singleton_class(self);
    RBASIC_SET_KLASS(self, evilr__next_class(klass));
  } else {
    klass = Qnil;
  }
  return klass;
}

/* call-seq:
 *   singleton_class_instance -> Object || nil
 * 
 * Returns the object attached to the singleton class.
 * If the class does not have an object attached to it (possibly because
 * it isn't a singleton class), returns +nil+. */
static VALUE evilr_singleton_class_instance(VALUE klass) {
  VALUE obj;
  if(RCLASS_IV_TBL(klass) && st_lookup(RCLASS_IV_TBL(klass), evilr__attached, &obj)) {
    return obj;
  }
  return Qnil;
}

/* call-seq:
 *   to_module -> Module
 * 
 * Makes a copy of the class, converts the copy to a module, and returns it. The
 * returned module can be included in other classes. */
static VALUE evilr_to_module(VALUE klass) {
  VALUE mod, iclass;

  if (IS_SINGLETON_CLASS(klass)) {
    if((mod = evilr_singleton_class_instance(klass))) {
      mod = rb_singleton_class_clone(mod);
      (void)evilr_detach_singleton(mod);
    } else {
      rb_raise(rb_eTypeError, "singleton class without attached instance");
    }
  } else {
    mod = rb_obj_clone(klass);
  }

  RBASIC_SET_KLASS(mod, rb_cModule);
  iclass = RCLASS_SUPER(mod);
  RCLASS_SET_SUPER(mod, NULL);
  FL_UNSET(mod, T_MASK);
  FL_SET(mod, T_MODULE);
  evilr__include_iclasses(mod, iclass);

  return mod;
}

/* call-seq:
 *   to_class(klass=Object) -> Class
 * 
 * Makes a copy of the module, converts the copy to a class, and returns it. The
 * returned class can then have instances created from it. The +klass+ argument
 * sets the superclass of the returned class.  If +klass+ is not a Class,
 * raises +TypeError+. */
static VALUE evilr_to_class(int argc, VALUE *argv, VALUE self) {
  VALUE klass = evilr__optional_class(argc, argv);

  self = rb_obj_clone(self);
  RBASIC_SET_KLASS(self, rb_singleton_class(klass));
  RCLASS_SET_SUPER(self, klass);
  FL_UNSET(self, T_MASK);
  FL_SET(self, T_CLASS);
  return self;
}

/* call-seq:
 *   flags -> Integer
 * 
 * Returns the internal flags value of the receiver as an +Integer+.  Raises +TypeError+ 
 * if the receiver is an immediate. */
static VALUE evilr_flags(VALUE self) {
  evilr__check_immediate(self);
  return UINT2NUM(RBASIC_FLAGS(self));
}

/* call-seq:
 *   superclass=(klass) -> klass
 * 
 * Modifies the superclass of the current class to be the given
 * class.  Any modules included in the receiver remain included.
 * Raises +TypeError+ if klass is not a Class or if the receiver
 * and class are not compatible (where their instances use
 * different internal types). */
static VALUE evilr_superclass_e(VALUE klass, VALUE super) {
  VALUE iclass;
  evilr__check_compatible_classes(klass, super);
  iclass = evilr__iclass_before_next_class(klass);
  RCLASS_SET_SUPER(iclass, super);
  rb_clear_cache_by_class(klass);
  return super;
}

/* call-seq:
 *   inherit(*classes) -> self
 * 
 * Make copies of all given +classes+ as modules, and includes
 * those modules in the receiver. Raises +TypeError+ if any of the
 * +classes+ is not a Class or is not compatible with the receiver. */
static VALUE evilr_inherit(int argc, VALUE* argv, VALUE klass) {
  int i;

  for(i = 0; i < argc; i++) {
    evilr__check_compatible_classes(klass, argv[i]);
    rb_include_module(klass, evilr_to_module(argv[i]));
  }

  return klass;
}

/* call-seq:
 *   force_bind(object) -> Method
 * 
 * Returns a +Method+ object bound to the given object.  Doesn't
 * check that the method is compatible with the object, unlike +bind+. */
static VALUE evilr_force_bind(VALUE self, VALUE obj) {
  struct METHOD *data;

  evilr__check_immediate(obj);
  self = rb_funcall(self, evilr__clone, 0);
  /* Data_Get_Struct seems to complain about types on 1.9,
   * so skip the type check. */
  data = (struct METHOD*)DATA_PTR(self);
  data->rclass = CLASS_OF(obj);
  return rb_funcall(self, evilr__bind, 1, obj);
}

/* call-seq:
 *   self -> Object
 * 
 * Returns the self of the receiver, which is the default context
 * used in evaluating the receiver. */
static VALUE evilr_self(VALUE self) {
  struct BLOCK *data;
  data = (struct BLOCK*)DATA_PTR(self);
  return data->self;
}

/* call-seq:
 *   self=(object) -> object
 * 
 * Sets the self of the receiver to the given object, modifying the  
 * used in evaluating the receiver. Example:
 *
 *   p = Proc.new{self[:a]}
 *   h1 = {:a=>1}
 *   h2 = {:a=>2}
 *   p.self = h1
 *   p.call # => 1
 *   p.self = h2
 *   p.call # => 2
 * */
static VALUE evilr_self_e(VALUE self, VALUE obj) {
  struct BLOCK *data;
  data = (struct BLOCK*)DATA_PTR(self);
  data->self = obj;
  return data->self;
}

/* call-seq:
 *   segfault
 * 
 * Dereferences the NULL pointer, which should cause SIGSEGV 
 * (a segmentation fault), and the death of the process, though
 * it could possibly be rescued. */
static VALUE evilr_segfault(VALUE self) {
  self = *(char *)NULL;
  return self;
}

/* call-seq:
 *   seppuku
 * 
 * Kills the current process with SIGKILL, which should
 * terminate the process immediately without any recovery possible. */
static VALUE evilr_seppuku(VALUE self) {
  kill(getpid(), SIGKILL);
  return self;
}

/* call-seq:
 *   allocate -> object
 * 
 * Allocate memory for the new instance.  Basically a copy of
 * +rb_class_allocate_instance+. */
static VALUE evilr_empty_alloc(VALUE klass) {
  NEWOBJ(obj, struct RObject);
  OBJSETUP(obj, klass, T_OBJECT);
  return (VALUE)obj;
}

/* call-seq:
 *   new(*args)-> object
 * 
 * Allocates memory for the instance and then calls initialize
 * on the object. */
static VALUE evilr_empty_new(int argc, VALUE* argv, VALUE klass) {
  VALUE obj;
  obj = evilr_empty_alloc(klass);
  rb_obj_call_init(obj, argc, argv);
  return obj;
}

/* call-seq:
 *   superclass -> Class || nil
 * 
 * Returns the superclass of the class, or nil if current class is
 * +Empty+. Basically a copy of the standard superclass method,
 * without some checking. */
static VALUE evilr_empty_superclass(VALUE klass) {
  return klass == empty ? Qnil : evilr__next_class(klass);
}

/* call-seq:
 *   initialize -> self
 * 
 * Returns the receiver. */
static VALUE evilr_empty_initialize(VALUE self) {
  return self;
}

/* Empty is an almost completely empty class, even more basic than
 * BasicObject.  It has no parent class, and only implements
 * +allocate+, +new+, +initialize+, and +superclass+.  All other
 * behavior must be added by the user.  Note that if you want to
 * call a method defined in Object, Kernel, or BasicObject that
 * isn't defined in Empty, you can use <tt>UndefinedMethod#force_bind</tt>,
 * to do so:
 *
 *   Object.instance_method(:puts).force_bind(Empty.new).call()
 */
void Init_evilr(void) {
  empty = rb_define_class("Empty", rb_cObject);
  rb_define_alloc_func(empty, evilr_empty_alloc);
  rb_define_singleton_method(empty, "new", evilr_empty_new, -1);
  rb_define_singleton_method(empty, "superclass", evilr_empty_superclass, 0);
  rb_define_method(empty, "initialize", evilr_empty_initialize, 0);
  RCLASS_SET_SUPER(empty, NULL);
  rb_global_variable(&empty);

  evilr__attached = rb_intern("__attached__");
  evilr__bind = rb_intern("bind");
  evilr__clone = rb_intern("clone");

  rb_define_method(rb_cObject, "class=", evilr_class_e, 1);
  rb_define_method(rb_cObject, "evilr_debug_print", evilr_debug_print, 0);
  rb_define_method(rb_cObject, "extend_between", evilr_extend_between, 1);
  rb_define_method(rb_cObject, "flags", evilr_flags, 0);
  rb_define_method(rb_cObject, "detach_singleton_class", evilr_detach_singleton_class, 0);
  rb_define_method(rb_cObject, "dup_singleton_class", evilr_dup_singleton_class, -1);
  rb_define_method(rb_cObject, "pop_singleton_class", evilr_pop_singleton_class, 0);
  rb_define_method(rb_cObject, "push_singleton_class", evilr_push_singleton_class, 1);
  rb_define_method(rb_cObject, "remove_singleton_class", evilr_remove_singleton_class, 0);
  rb_define_method(rb_cObject, "remove_singleton_classes", evilr_remove_singleton_classes, 0);
  rb_define_method(rb_cObject, "set_singleton_class", evilr_set_singleton_class, 1);
  rb_define_method(rb_cObject, "swap", evilr_swap, 1);
  rb_define_method(rb_cObject, "swap_instance_variables", evilr_swap_instance_variables, 1);
  rb_define_method(rb_cObject, "swap_singleton_class", evilr_swap_singleton_class, 1);
  rb_define_method(rb_cObject, "unextend", evilr_unextend, 1);
  rb_define_method(rb_cObject, "unfreeze", evilr_unfreeze, 0);

  rb_define_method(rb_mKernel, "segfault", evilr_segfault, 0);
  rb_define_method(rb_mKernel, "seppuku", evilr_seppuku, 0);
  rb_define_method(rb_mKernel, "set_safe_level", evilr_set_safe_level, 1);

  rb_define_method(rb_cModule, "include_between", evilr_include_between, 1);
  rb_define_method(rb_cModule, "swap_method_tables", evilr_swap_method_tables, 1);
  rb_define_method(rb_cModule, "to_class", evilr_to_class, -1);
  rb_define_method(rb_cModule, "uninclude", evilr_uninclude, 1);

  rb_define_method(rb_cClass, "detach_singleton", evilr_detach_singleton, 0);
  rb_define_method(rb_cClass, "inherit", evilr_inherit, -1);
  rb_define_method(rb_cClass, "singleton_class_instance", evilr_singleton_class_instance, 0);
  rb_define_method(rb_cClass, "superclass=", evilr_superclass_e, 1);
  rb_define_method(rb_cClass, "to_module", evilr_to_module, 0);

  rb_define_method(rb_cUnboundMethod, "force_bind", evilr_force_bind, 1);

  rb_define_method(rb_cProc, "self", evilr_self, 0);
  rb_define_method(rb_cProc, "self=", evilr_self_e, 1);
}
