#include <ruby.h> 
#ifdef RUBY19
#include <ruby/st.h> 
#else
#include <st.h> 
#endif

#ifndef SAFE_LEVEL_MAX
#define SAFE_LEVEL_MAX 4
#endif

extern int ruby_safe_level;

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

static VALUE evilr__debug_print(VALUE self) {
  if (self == NULL) {
    return Qnil;
  }
  evilr__check_immediate(self);
  switch(BUILTIN_TYPE(self)) {
    case T_CLASS:
    case T_ICLASS:
    case T_MODULE:
      printf("self %p klass %p flags %ld iv_tbl %p m_tbl %p super %p\n", (void *)self, (void *)RBASIC(self)->klass, RBASIC(self)->flags, (void *)ROBJECT(self)->iv_tbl, (void *)RCLASS(self)->m_tbl, (void *)RCLASS(self)->super);
      self = RCLASS(self)->super;
      break;
    default:
      printf("self %p klass %p flags %ld iv_tbl %p\n", (void *)self, (void *)RBASIC(self)->klass, RBASIC(self)->flags, (void *)ROBJECT(self)->iv_tbl);
      self = RBASIC(self)->klass;
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

  RBASIC(self)->klass = klass;
  return self;
}

static VALUE evilr_include_singleton_class(VALUE self, VALUE other) {
  evilr__check_immediate(self);
  evilr__check_immediate(other);

  /* Create singleton classes to be included if it doesn't exist */
  (void)rb_singleton_class(other);

  /* Loses current singleton class */
  RBASIC(self)->klass = RBASIC(other)->klass;

  return self;
}

static VALUE evilr_swap_singleton_class(VALUE self, VALUE other) {
  VALUE tmp;

  evilr__check_immediate(self);
  evilr__check_immediate(other);

  /* Create singleton classes to be swapped if they doesn't exist */
  (void)rb_singleton_class(other);
  (void)rb_singleton_class(self);

  tmp = RBASIC(self)->klass;
  RBASIC(self)->klass = RBASIC(other)->klass;
  RBASIC(other)->klass = tmp;

  /* Attach each singleton class to its object */
  rb_singleton_class_attached(RBASIC(self)->klass, self);
  rb_singleton_class_attached(RBASIC(other)->klass, other);

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
  ruby_safe_level = NUM2INT(safe);
  if (ruby_safe_level > SAFE_LEVEL_MAX) {
    ruby_safe_level = SAFE_LEVEL_MAX;
  }
  return safe;
}

static VALUE evilr_detach_singleton(VALUE klass) {
  if (FL_TEST(klass, FL_SINGLETON)) {
    FL_UNSET(klass, FL_SINGLETON);
    if (RCLASS(klass)->iv_tbl) {
      st_delete(RCLASS(klass)->iv_tbl, (st_data_t*)&evilr__attached, 0);
    }
  }
  return klass;
}

static VALUE evilr_detach_singleton_class(VALUE self) {
  evilr__check_immediate(self);
  return evilr_detach_singleton(RBASIC(self)->klass);
}

static VALUE evilr_singleton_class_instance(VALUE klass) {
  VALUE obj;
  if(RCLASS(klass)->iv_tbl && st_lookup(RCLASS(klass)->iv_tbl, evilr__attached, &obj)) {
    return obj;
  }
  return Qnil;
}

static VALUE evilr_to_module(VALUE klass) {
  VALUE mod;

  if (RBASIC(klass)->flags & FL_SINGLETON) {
    if((mod = evilr_singleton_class_instance(klass))) {
      mod = rb_singleton_class_clone(mod);
    } else {
      mod = rb_singleton_class_clone(klass);
      (void)evilr_detach_singleton(mod);
    }
  } else {
    mod = rb_obj_clone(klass);
  }
  RBASIC(mod)->klass = rb_cModule;
  RBASIC(mod)->flags &= ~ T_MASK;
  RBASIC(mod)->flags |= T_MODULE;
  return mod;
}

static VALUE evilr_flags(VALUE self) {
  evilr__check_immediate(self);
  return UINT2NUM(RBASIC(self)->flags);
}

void Init_evilr(void) {
  evilr__attached = rb_intern("__attached__");

  rb_define_method(rb_cObject, "class=", evilr_class_e, 1);
  rb_define_method(rb_cObject, "flags", evilr_flags, 0);
  rb_define_method(rb_cObject, "detach_singleton_class", evilr_detach_singleton_class, 0);
  rb_define_method(rb_cObject, "include_singleton_class", evilr_include_singleton_class, 1);
  rb_define_method(rb_cObject, "swap_singleton_class", evilr_swap_singleton_class, 1);
  rb_define_method(rb_cObject, "unfreeze", evilr_unfreeze, 0);

  rb_define_method(rb_mKernel, "set_safe_level", evilr_set_safe_level, 1);

  rb_define_method(rb_cClass, "detach_singleton", evilr_detach_singleton, 0);
  rb_define_method(rb_cClass, "singleton_class_instance", evilr_singleton_class_instance, 0);
  rb_define_method(rb_cClass, "to_module", evilr_to_module, 0);
}
