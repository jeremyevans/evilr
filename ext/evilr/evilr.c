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


#define RBASIC_SET_KLASS(o, c) (RBASIC(o)->klass = c)
#define RBASIC_KLASS(o) (RBASIC(o)->klass)
#define RBASIC_FLAGS(o) (RBASIC(o)->flags)
#define IS_SINGLETON_CLASS(o) (FL_TEST(o, FL_SINGLETON))
#define HAS_SINGLETON_CLASS(o) (FL_TEST(RBASIC_KLASS(o), FL_SINGLETON))

#ifdef RUBY19
#define RCLASS_SET_SUPER(o, c) (RCLASS(o)->ptr->super = c)

size_t OBJECT_SIZE = sizeof(VALUE) * 5;
#else
#define RCLASS_SET_SUPER(o, c) (RCLASS(o)->super = c)
#define ROBJECT_IVPTR(o) (ROBJECT(o)->iv_tbl)

size_t OBJECT_SIZE = sizeof(VALUE) * 2 + sizeof(unsigned long) + sizeof(void *) * 2;
extern int ruby_safe_level;
#endif

ID evilr__attached;

static void evilr__check_immediate(VALUE self) {
  if (SPECIAL_CONST_P(self)) {
    rb_raise(rb_eTypeError, "can't use immediate value");
  }
}

static void evilr__check_immediates(VALUE self, VALUE other) {
  evilr__check_immediate(self);
  evilr__check_immediate(other);
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

static void evilr__check_data_type(VALUE self) {
  if (BUILTIN_TYPE(self) == T_DATA) {
    rb_raise(rb_eTypeError, "incompatible type used");
  }
}

static VALUE evilr__next_class(VALUE klass) {
  VALUE c;
  for (c = RCLASS_SUPER(klass); c && BUILTIN_TYPE(c) != T_CLASS; c = RCLASS_SUPER(c)); /* empty */
  return c;
}

static VALUE evilr__iclass_before_next_class(VALUE klass) {
  VALUE c, i = NULL;
  for (c = RCLASS_SUPER(klass); c && BUILTIN_TYPE(c) != T_CLASS; i = c, c = RCLASS_SUPER(c)); /* empty */
  return i == NULL ? klass : i;
}

static VALUE evilr__iclass_matching_before(VALUE klass, VALUE mod, VALUE before) {
  VALUE c;
  for (c = RCLASS_SUPER(klass); c && c != before; c = RCLASS_SUPER(c)) {
    if (BUILTIN_TYPE(c) == T_ICLASS && RBASIC_KLASS(c) == mod) {
      return c;
    }
  }
  return NULL;
}

static VALUE evilr__iclass_matching(VALUE klass, VALUE mod) {
  return evilr__iclass_matching_before(klass, mod, NULL);
}

void evilr__reparent_singleton_class(VALUE self, VALUE klass) {
  VALUE self_klass = RBASIC_KLASS(self);

  if (IS_SINGLETON_CLASS(self_klass)) {
    RCLASS_SET_SUPER(evilr__iclass_before_next_class(self_klass), klass);
  } else {
    RBASIC_SET_KLASS(self, klass);
  }
}

void evilr__reparent_class(VALUE self, VALUE klass) {
  RCLASS_SET_SUPER(evilr__iclass_before_next_class(self), klass);
}

void evilr__check_obj_and_class(VALUE self, VALUE klass) {
  evilr__check_immediates(self, klass);
  evilr__check_type(T_CLASS, klass);
}

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

void evilr__make_singleton(VALUE self, VALUE klass) {
  FL_SET(klass, FL_SINGLETON);
  RBASIC_SET_KLASS(self, klass);
  rb_singleton_class_attached(klass, self);
}

void evilr__check_compatible_classes(VALUE klass, VALUE super) {
  evilr__check_immediate(super);
  evilr__check_type(T_CLASS, super);
  evilr__check_class_type(evilr__class_type(klass), super);
  evilr__check_data_type(rb_obj_alloc(klass));
}

void evilr__include_iclasses(VALUE mod, VALUE iclass) {
  if (iclass && BUILTIN_TYPE(iclass) == T_ICLASS) {
    evilr__include_iclasses(mod, RCLASS_SUPER(iclass));
    rb_include_module(mod, RBASIC_KLASS(iclass));
  }
}


static VALUE evilr_class_e(VALUE self, VALUE klass) {
  evilr__check_immediate(self);
  evilr__check_type(evilr__class_type(klass), self);
  evilr__check_data_type(self);

  RBASIC_SET_KLASS(self, klass);
  return self;
}

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

static VALUE evilr_swap_instance_variables(VALUE self, VALUE other) {
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
  struct st_table *tmp;
  tmp = ROBJECT_IVPTR(self);
  ROBJECT(self)->iv_tbl = ROBJECT_IVPTR(other);
  ROBJECT(other)->iv_tbl = tmp;
#endif
  return self;
}

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

static VALUE evilr_detach_singleton(VALUE klass) {
  if (IS_SINGLETON_CLASS(klass)) {
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

static VALUE evilr_push_singleton_class(VALUE self, VALUE klass) {
  evilr__check_obj_and_class(self, klass);
  evilr__reparent_class(evilr__iclass_before_next_class(klass), RBASIC_KLASS(self));
  evilr__make_singleton(self, klass);
  return klass;
}

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

static VALUE evilr_remove_singleton_classes(VALUE self) {
  evilr__check_immediate(self);
  RBASIC_SET_KLASS(self, rb_obj_class(self));  
  return Qnil;
}

static VALUE evilr_set_singleton_class(VALUE self, VALUE klass) {
  evilr__check_obj_and_class(self, klass);
  RCLASS_SET_SUPER(evilr__iclass_before_next_class(klass), rb_obj_class(self));
  evilr__make_singleton(self, klass);
  return klass;
}

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

static VALUE evilr_singleton_class_instance(VALUE klass) {
  VALUE obj;
  if(RCLASS_IV_TBL(klass) && st_lookup(RCLASS_IV_TBL(klass), evilr__attached, &obj)) {
    return obj;
  }
  return Qnil;
}

static VALUE evilr_to_module(VALUE klass) {
  VALUE mod, iclass;

  if (IS_SINGLETON_CLASS(klass)) {
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
  iclass = RCLASS_SUPER(mod);
  RCLASS_SET_SUPER(mod, NULL);
  FL_UNSET(mod, T_MASK);
  FL_SET(mod, T_MODULE);
  evilr__include_iclasses(mod, iclass);

  return mod;
}

static VALUE evilr_to_class(int argc, VALUE *argv, VALUE self) {
  VALUE klass = evilr__optional_class(argc, argv);

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

static VALUE evilr_superclass_e(VALUE klass, VALUE super) {
  VALUE iclass;
  evilr__check_compatible_classes(klass, super);
  iclass = evilr__iclass_before_next_class(klass);
  RCLASS_SET_SUPER(iclass, super);
  return super;
}

static VALUE evilr_inherit(int argc, VALUE* argv, VALUE klass) {
  int i;

  for(i = 0; i < argc; i++) {
    evilr__check_compatible_classes(klass, argv[i]);
    rb_include_module(klass, evilr_to_module(argv[i]));
  }

  return klass;
}

static VALUE evilr_force_bind(VALUE self, VALUE obj) {
  struct METHOD *data;

  evilr__check_immediate(obj);
  self = rb_funcall(self, rb_intern("clone"), 0);
  /* Data_Get_Struct seems to complain about types on 1.9,
   * so skip the type check. */
  data = (struct METHOD*)DATA_PTR(self);
  data->rclass = CLASS_OF(obj);
  return rb_funcall(self, rb_intern("bind"), 1, obj);
}

static VALUE evilr_self(VALUE self) {
  struct BLOCK *data;
  data = (struct BLOCK*)DATA_PTR(self);
  return data->self;
}

static VALUE evilr_self_e(VALUE self, VALUE obj) {
  struct BLOCK *data;
  data = (struct BLOCK*)DATA_PTR(self);
  data->self = obj;
  return data->self;
}

static VALUE evilr_segfault(VALUE self) {
  self = *(char *)NULL;
  return self;
}

static VALUE evilr_seppuku(VALUE self) {
  kill(getpid(), SIGKILL);
  return self;
}

void Init_evilr(void) {
  evilr__attached = rb_intern("__attached__");

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
