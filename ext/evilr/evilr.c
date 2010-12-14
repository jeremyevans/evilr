#include <ruby.h> 

#ifndef SAFE_LEVEL_MAX
#define SAFE_LEVEL_MAX 4
#endif

extern int ruby_safe_level;

static int evilr__obj_type(VALUE self) {
  return RBASIC(self)->flags & T_MASK;
}

static void evilr__check_immediate(VALUE self) {
  if (SPECIAL_CONST_P(self)) {
    rb_raise(rb_eTypeError, "can't use immediate value");
  }
}

static void evilr__check_type(int type, VALUE self) {
  if (evilr__obj_type(self) != type) {
    rb_raise(rb_eTypeError, "incompatible type used");
  }
}

static int evilr__class_type(VALUE klass) {
  evilr__check_type(T_CLASS, klass);
  return evilr__obj_type(rb_obj_alloc(klass));
}


static VALUE evilr_class_e(VALUE self, VALUE klass) {
  evilr__check_immediate(self);
  evilr__check_type(evilr__class_type(klass), self);
  if (evilr__obj_type(self) == T_DATA) {
    rb_raise(rb_eTypeError, "incompatible type used");
  }

  RBASIC(self)->klass = klass;
  return self;
}

static VALUE evilr_include_singleton_class(VALUE self, VALUE other) {
  VALUE tmp;

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

void Init_evilr(void) {
  rb_define_method(rb_cObject, "class=", evilr_class_e, 1);
  rb_define_method(rb_cObject, "include_singleton_class", evilr_include_singleton_class, 1);
  rb_define_method(rb_cObject, "swap_singleton_class", evilr_swap_singleton_class, 1);
  rb_define_method(rb_cObject, "unfreeze", evilr_unfreeze, 0);

  rb_define_method(rb_mKernel, "set_safe_level", evilr_set_safe_level, 1);
}
