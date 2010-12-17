#include <ruby.h> 
#ifdef RUBY19
#include <ruby/st.h> 
#else
#include <st.h> 
#endif

#ifndef SAFE_LEVEL_MAX
#define SAFE_LEVEL_MAX 4
#endif

#ifndef RUBY19
extern int ruby_safe_level;
#endif

#define RBASIC_SET_KLASS(o, c) (RBASIC(o)->klass = c)
#define RBASIC_KLASS(o) (RBASIC(o)->klass)
#define RBASIC_FLAGS(o) (RBASIC(o)->flags)

#ifdef RUBY19
#define RCLASS_SET_SUPER(o, c) (RCLASS(o)->ptr->super = c)
#else
#define RCLASS_SET_SUPER(o, c) (RCLASS(o)->super = c)
#define ROBJECT_IV_INDEX_TBL(o) (ROBJECT(o)->iv_tbl)
#endif

ID evilr__attached;

static void evilr__check_immediate(VALUE self) {
  if (SPECIAL_CONST_P(self)) {
    rb_raise(rb_eTypeError, "can't use immediate value");
  }
}

static void evilr__check_type(unsigned int type, VALUE self) {
  if (BUILTIN_TYPE(self) != type) {
    rb_raise(rb_eTypeError, "incompatible type used");
  }
}

static unsigned int evilr__class_type(VALUE klass) {
  evilr__check_type(T_CLASS, klass);
  return BUILTIN_TYPE(rb_obj_alloc(klass));
}

static void evilr__check_class_type(unsigned int type, VALUE self) {
  if (evilr__class_type(self) != type) {
    rb_raise(rb_eTypeError, "incompatible type used");
  }
}


static VALUE evilr__debug_print(VALUE self) {
  if (self == NULL) {
    return Qnil;
  }
  evilr__check_immediate(self);
  switch(BUILTIN_TYPE(self)) {
    case T_CLASS:
    case T_ICLASS:
    case T_MODULE:
      printf("self %p klass %p flags %ld iv_tbl %p m_tbl %p super %p\n", (void *)self, (void *)RBASIC_KLASS(self), RBASIC_FLAGS(self), (void *)ROBJECT_IV_INDEX_TBL(self), (void *)RCLASS_M_TBL(self), (void *)RCLASS_SUPER(self));
      self = RCLASS_SUPER(self);
      break;
    default:
      printf("self %p klass %p flags %ld iv_tbl %p\n", (void *)self, (void *)RBASIC_KLASS(self), RBASIC_FLAGS(self), (void *)ROBJECT_IV_INDEX_TBL(self));
      self = RBASIC_KLASS(self);
      break;
  }
  return evilr__debug_print(self);
}


static VALUE evilr_class_e(VALUE self, VALUE klass) {
  evilr__check_immediate(self);
  evilr__check_type(evilr__class_type(klass), self);
  if (BUILTIN_TYPE(self) == T_DATA) {
    rb_raise(rb_eTypeError, "incompatible type used");
  }

  RBASIC_SET_KLASS(self, klass);
  return self;
}

static VALUE evilr_include_singleton_class(VALUE self, VALUE other) {
  evilr__check_immediate(self);
  evilr__check_immediate(other);

  /* Create singleton classes to be included if it doesn't exist */
  (void)rb_singleton_class(other);

  /* Loses current singleton class */
  RBASIC_SET_KLASS(self, RBASIC_KLASS(other));

  return self;
}

static VALUE evilr_swap_singleton_class(VALUE self, VALUE other) {
  VALUE tmp;

  evilr__check_immediate(self);
  evilr__check_immediate(other);

  /* Create singleton classes to be swapped if they doesn't exist */
  (void)rb_singleton_class(other);
  (void)rb_singleton_class(self);

  tmp = RBASIC_KLASS(self);
  RBASIC_SET_KLASS(self, RBASIC_KLASS(other));
  RBASIC_SET_KLASS(other, tmp);

  /* Attach each singleton class to its object */
  rb_singleton_class_attached(RBASIC_KLASS(self), self);
  rb_singleton_class_attached(RBASIC_KLASS(other), other);

  return self;
}

static VALUE evilr_unfreeze(VALUE self) {
  if (rb_safe_level() > 0) {
    rb_raise(rb_eSecurityError, "can't unfreeze objects when $SAFE > 0");
  }
  FL_UNSET(self, FL_FREEZE);
  return self;
}

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

static VALUE evilr_detach_singleton(VALUE klass) {
  if (FL_TEST(klass, FL_SINGLETON)) {
    FL_UNSET(klass, FL_SINGLETON);
    if (RCLASS_IV_TBL(klass)) {
      st_delete(RCLASS_IV_TBL(klass), (st_data_t*)&evilr__attached, 0);
    }
  }
  return klass;
}

static VALUE evilr_detach_singleton_class(VALUE self) {
  evilr__check_immediate(self);
  return evilr_detach_singleton(RBASIC_KLASS(self));
}

static VALUE evilr_remove_singleton_class(VALUE self) {
  VALUE klass, c;
  evilr__check_immediate(self);

  if (FL_TEST(RBASIC_KLASS(self), FL_SINGLETON)) {
    klass = evilr_detach_singleton_class(self);
    for (c = RCLASS_SUPER(klass); BUILTIN_TYPE(c) != T_CLASS; c = RCLASS_SUPER(c)); /* empty */
    RBASIC_SET_KLASS(self, c);
  } else {
    klass = Qnil;
  }
  return klass;
}

static VALUE evilr_singleton_class_instance(VALUE klass) {
  VALUE obj;
  if(RCLASS_IV_TBL(klass) && st_lookup(RCLASS_IV_TBL(klass), evilr__attached, &obj)) {
    return obj;
  }
  return Qnil;
}

static VALUE evilr_to_module(VALUE klass) {
  VALUE mod;

  if (FL_TEST(klass, FL_SINGLETON)) {
    if((mod = evilr_singleton_class_instance(klass))) {
      mod = rb_singleton_class_clone(mod);
    } else {
      mod = rb_singleton_class_clone(klass);
      (void)evilr_detach_singleton(mod);
    }
  } else {
    mod = rb_obj_clone(klass);
  }

  RBASIC_SET_KLASS(mod, rb_cModule);
  FL_UNSET(mod, T_MASK);
  FL_SET(mod, T_MODULE);
  return mod;
}

static VALUE evilr_to_class(int argc, VALUE *argv, VALUE self) {
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

  self = rb_obj_clone(self);
  RBASIC_SET_KLASS(self, rb_singleton_class(klass));
  RCLASS_SET_SUPER(self, klass);
  FL_UNSET(self, T_MASK);
  FL_SET(self, T_CLASS);
  return self;
}

static VALUE evilr_flags(VALUE self) {
  evilr__check_immediate(self);
  return UINT2NUM(RBASIC_FLAGS(self));
}

void Init_evilr(void) {
  evilr__attached = rb_intern("__attached__");

  rb_define_method(rb_cObject, "class=", evilr_class_e, 1);
  rb_define_method(rb_cObject, "flags", evilr_flags, 0);
  rb_define_method(rb_cObject, "detach_singleton_class", evilr_detach_singleton_class, 0);
  rb_define_method(rb_cObject, "include_singleton_class", evilr_include_singleton_class, 1);
  rb_define_method(rb_cObject, "swap_singleton_class", evilr_swap_singleton_class, 1);
  rb_define_method(rb_cObject, "remove_singleton_class", evilr_remove_singleton_class, 0);
  rb_define_method(rb_cObject, "unfreeze", evilr_unfreeze, 0);

  rb_define_method(rb_mKernel, "set_safe_level", evilr_set_safe_level, 1);

  rb_define_method(rb_cModule, "to_class", evilr_to_class, -1);

  rb_define_method(rb_cClass, "detach_singleton", evilr_detach_singleton, 0);
  rb_define_method(rb_cClass, "singleton_class_instance", evilr_singleton_class_instance, 0);
  rb_define_method(rb_cClass, "to_module", evilr_to_module, 0);
}
