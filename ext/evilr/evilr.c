#include <ruby.h> 

void evilr__check_immediate(VALUE self) {
  if (SPECIAL_CONST_P(self)) {
    rb_raise(rb_eTypeError, "can't use immediate value");
  }
}

static VALUE evilr_share_singleton_class(VALUE self, VALUE other) {
  evilr__check_immediate(self);
  evilr__check_immediate(other);

  /* Create singleton class to be shared if it doesn't exist */
  (void)rb_singleton_class(other);

  RBASIC(self)->klass = RBASIC(other)->klass;
  return self;
}

void Init_evilr(void) {
  rb_define_method(rb_cObject, "share_singleton_class", evilr_share_singleton_class, 1);
}
