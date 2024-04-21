//  Partie implantation du module holdall.

#include "holdall.h"

//  struct holdall, holdall : implantation par tableau dynamique

#include <limits.h>
#include <stdint.h>

//  HA__CAPACITY_MIN : Taille minimale (et celle d'origine) du tableau
//    dynamique d'un fourretout
#define HA__CAPACITY_MIN 4

//  HA__CAPACITY_MUL : facteur par lequel est multipliée la taille du tableau
//    dynamique d'un fourretout lorsque celui-ci est plein
#define HA__CAPACITY_MUL 2


//  Structure ------------------------------------------------------------------

struct holdall {
  void **harr;
  size_t harr_size;
  size_t count;
};

//  Fonction auxiliaire --------------------------------------------------------

//  holdall_increase_capacity : multiplie la capacité du fourretout ha (c'est-
//    à-dire la taille de son tableau) par HA__CAPACITY_MUL. Renvoie -1 en cas
//    de dépassement de capacité, et ha reste inchangé. Renvoie sinon 0.
int holdall__increase_capacity(holdall *ha) {
  if (ha->harr_size > SIZE_MAX / HA__CAPACITY_MUL) {
    return -1;
  }
  void **t = realloc(ha->harr, ha->harr_size * HA__CAPACITY_MUL * sizeof *ha->harr);
  if (t == NULL) {
    return -1;
  }
  ha->harr = t;
  ha->harr_size *= HA__CAPACITY_MUL;
  return 0;
}

//  Fonctions ------------------------------------------------------------------

holdall *holdall_empty(void) {
  holdall *ha = malloc(sizeof *ha);
  if (ha == NULL) {
    return NULL;
  }
  ha->harr = malloc(HA__CAPACITY_MIN * sizeof *ha->harr);
  if (ha->harr == NULL) {
    free(ha);
    return NULL;
  }
  ha->harr_size = HA__CAPACITY_MIN;
  ha->count = 0;
  return ha;
}

void holdall_dispose(holdall **haptr) {
  free((*haptr)->harr);
  free(*haptr);
  *haptr = NULL;
}

int holdall_put(holdall *ha, void *ref) {
  if (ha->count == ha->harr_size - 1 && holdall__increase_capacity(ha) != 0) {
    return -1;
  }
  ha->harr[ha->count++] = ref;
  return 0;
}

size_t holdall_count(holdall *ha) {
  return ha->count;
}

int holdall_apply(holdall *ha,
    int (*fun)(void *)) {
  for (size_t i = 0; i < ha->count; ++i) {
    int r = fun(ha->harr[i]);
    if (r != 0) {
      return r;
    }
  }
  return 0;
}

int holdall_apply_context(holdall *ha,
    void *context, void *(*fun1)(void *context, void *ptr),
    int (*fun2)(void *ptr, void *resultfun1)) {
  for (size_t i = 0; i < ha->count; ++i) {
    int r = fun2(ha->harr[i], fun1(context, ha->harr[i]));
    if (r != 0) {
      return r;
    }
  }
  return 0;
}

int holdall_apply_context2(holdall *ha,
    void *context1, void *(*fun1)(void *context1, void *ptr),
    void *context2, int (*fun2)(void *context2, void *ptr, void *resultfun1)) {
  for (size_t i = 0; i < ha->count; ++i) {
    int r = fun2(context2, ha->harr[i], fun1(context1, ha->harr[i]));
    if (r != 0) {
      return r;
    }
  }
  return 0;
}

//  Extension ------------------------------------------------------------------

#if defined HOLDALL_WANT_EXT && HOLDALL_WANT_EXT != 0

//  memswitch : échange le contenu de src1 et src2, qui sont 2 zones mémoires
//    de tailles n
static void memswitch(char *src1, char *src2, size_t n) {
  if (n == 0) {
    return;
  }
  char t = *src1;
  *src1 = *src2;
  *src2 = t;
  memswitch(src1 + 1, src2 + 1, n - 1);
}

//  heapsort_down : il est supposé que base est l'adresse du premier composant
//    d'un tableau de longueur nmemb et de taille de composants size, que
//    nmemb >= 1, que k <= nmemb - 1 et que le tableau est un maximier sur
//    [ k + 1 ... nmemb - 1 ] relativement à la fonction de comparaison pointée
//    par compar. Descend le composant d'indice k à la bonne place de manière à
//    faire du tableau un maximier sur [ k ... nmemb - 1 ].
static void heapsort_down(char *base, size_t nmemb, size_t size,
    int (*compar)(const void *, const void *), size_t k) {
  size_t ni1 = 2 * k + 1;
  size_t ni2 = 2 * k + 2;
  size_t mi;
  if (ni1 >= nmemb) {
    return;
  } else if (ni2 >= nmemb) {
    mi = ni1;
  } else {
    mi = compar(base + ni1 * size, base + ni2 * size) >= 0 ? ni1 : ni2;
  }
  char *p = base + k * size;
  char *m = base + mi * size;
  if (compar(p, m) >= 0) {
    return;
  }
  memswitch (p, m, size);
  heapsort_down(base, nmemb, size, compar, mi);
}

void heapsort(void *base, size_t nmemb, size_t size,
    int (*compar)(const void *, const void *)) {
  if (nmemb <= 1) {
    return;
  }
  // Mise en tas
  for (size_t k = 1; k <= nmemb / 2; ++k) {
    heapsort_down(base, nmemb, size, compar, nmemb / 2 - k);
  }
  // Tri suffixe
  for (; nmemb > 1; --nmemb) {
    memswitch(base, (char *) base + (nmemb - 1) * size, size);
    heapsort_down (base, nmemb - 1, size, compar, 0);
  }
}

void holdall_sort(holdall *ha, int (*compar)(const void *, const void *)) {
  heapsort(ha->harr, ha->count, sizeof (void *), compar);
}

#endif
