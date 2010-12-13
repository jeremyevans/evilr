#include <ruby.h> 

static VALUE evilr_share_singleton_class(VALUE self, VALUE other) {
  (void)rb_singleton_class(self);
  (void)rb_singleton_class(other);
  ((struct RBasic*)self)->klass = ((struct RBasic*)other)->klass;
  return self;
}

void Init_evilr(void) {
  rb_define_method(rb_cObject, "share_singleton_class", evilr_share_singleton_class, 1);
}
