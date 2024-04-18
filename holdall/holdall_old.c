//  Partie implantation du module holdall.

#include "holdall.h"

//  struct holdall, holdall : implantation par liste dynamique simplement
//    chainée.

//  Si la macroconstante HOLDALL_PUT_TAIL est définie et que sa macro-évaluation
//    donne un entier non nul, l'insertion dans la liste a lieu en queue. Dans
//    le cas contraire, elle a lieu en tête.

typedef struct choldall choldall;

struct choldall {
  void *ref;
  choldall *next;
};

struct holdall {
  choldall *head;
#if defined HOLDALL_PUT_TAIL && HOLDALL_PUT_TAIL != 0
  choldall **tailptr;
#endif
  size_t count;
};

holdall *holdall_empty(void) {
  holdall *ha = malloc(sizeof *ha);
  if (ha == NULL) {
    return NULL;
  }
  ha->head = NULL;
#if defined HOLDALL_PUT_TAIL && HOLDALL_PUT_TAIL != 0
  ha->tailptr = &ha->head;
#endif
  ha->count = 0;
  return ha;
}

void holdall_dispose(holdall **haptr) {
  if (*haptr == NULL) {
    return;
  }
  choldall *p = (*haptr)->head;
  while (p != NULL) {
    choldall *t = p;
    p = p->next;
    free(t);
  }
  free(*haptr);
  *haptr = NULL;
}

int holdall_put(holdall *ha, void *ref) {
  choldall *p = malloc(sizeof *p);
  if (p == NULL) {
    return -1;
  }
  p->ref = ref;
#if defined HOLDALL_PUT_TAIL && HOLDALL_PUT_TAIL != 0
  p->next = NULL;
  *ha->tailptr = p;
  ha->tailptr = &p->next;
#else
  p->next = ha->head;
  ha->head = p;
#endif
  ha->count += 1;
  return 0;
}

size_t holdall_count(holdall *ha) {
  return ha->count;
}

int holdall_apply(holdall *ha,
    int (*fun)(void *)) {
  for (const choldall *p = ha->head; p != NULL; p = p->next) {
    int r = fun(p->ref);
    if (r != 0) {
      return r;
    }
  }
  return 0;
}

int holdall_apply_context(holdall *ha,
    void *context, void *(*fun1)(void *context, void *ptr),
    int (*fun2)(void *ptr, void *resultfun1)) {
  for (const choldall *p = ha->head; p != NULL; p = p->next) {
    int r = fun2(p->ref, fun1(context, p->ref));
    if (r != 0) {
      return r;
    }
  }
  return 0;
}

int holdall_apply_context2(holdall *ha,
    void *context1, void *(*fun1)(void *context1, void *ptr),
    void *context2, int (*fun2)(void *context2, void *ptr, void *resultfun1)) {
  for (const choldall *p = ha->head; p != NULL; p = p->next) {
    int r = fun2(context2, p->ref, fun1(context1, p->ref));
    if (r != 0) {
      return r;
    }
  }
  return 0;
}

#if defined HOLDALL_WANT_EXT && HOLDALL_WANT_EXT != 0

void choldall__qsort_add_head(choldall **c1, choldall *c2) {
  c2->next = *c1;
  *c1 = c2;
}

choldall *choldall__qsort_link(choldall *c1, choldall *c2) {
  if (c1 != NULL) {
    while (c1->next != NULL) {
      c1 = c1->next;
    }
    c1->next = c2;
  }
  if (c2 == NULL) {
    return c1;
  }
  while (c2->next != NULL) {
    c2 = c2->next;
  }
  return c2;
}

choldall *choldall__qsort(choldall *c, int (*compar)(const void *, const void *)) {
  if (c == NULL) return NULL;
  if (c->next == NULL) return c;
  choldall *c_lt = NULL;
  choldall *c_gt = NULL;
  choldall *c_eq = c;

  const void *p = c->ref;
  choldall *cur = c->next;
  c->next = NULL;

  while (cur != NULL) {
    choldall *t = cur->next;
    int r = compar(cur->ref, p);
    if (r > 0) {
      choldall__qsort_add_head(&c_gt, cur);
    } else if (r < 0) {
      choldall__qsort_add_head(&c_lt, cur);
    } else {
      choldall__qsort_add_head(&c_eq, cur);
    }
    cur = t;
  }
  c_lt = choldall__qsort(c_lt, compar);
  c_gt = choldall__qsort(c_gt, compar);

  choldall *l = choldall__qsort_link(c_lt, c_eq);
  choldall__qsort_link(l, c_gt);
  return c_lt;
}

void holdall_sort(holdall *ha,
    int (*compar)(const void *, const void *)) {
  choldall__qsort(ha->head, compar);
}

#endif
