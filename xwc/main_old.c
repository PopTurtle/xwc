#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hashtable.h"
#include "holdall.h"

#define STR(s)  #s
#define XSTR(s) STR(s)

#define MAX_W_LEN 32

#define BGCOLOR_WHITE "\x1b[107m"
#define BGCOLOR_RESET "\x1b[49m"

#define FGCOLOR_BLACK "\x1b[30m"
#define FGCOLOR_RESET "\x1b[39m"

#define FORMAT_INPUT_START BGCOLOR_WHITE FGCOLOR_BLACK
#define FORMAT_INPUT_STOP BGCOLOR_RESET FGCOLOR_RESET

// Structure compteur mot
typedef struct word word;
struct word {
  char *w;
  long unsigned int c;
  int f;
  bool filtered;
};

// Struct arguments du main
typedef struct args args;
struct args {
  const char **file;
  int filecount;

  bool filtered;
  const char *filter_fn;

  bool sorted;
  int (*sort_compare)(const void *, const void *);
};

//  str_hashfun : l'une des fonctions de pré-hachage conseillées par Kernighan
//    et Pike pour les chaines de caractères. //////////////////////////////////
static size_t str_hashfun(const char *s); /////////////////////////////////////

//  rfree2 : libère les zones mémoires associées à p1 et p2, puis renvoie 0
static int rfree2(void *p1, void *p2);

//
static int word_display(char *s, word *w);

//
static int word_display_filter(char *s, word *w);

//
static args *args_init(int argc, char *argv[]);

//
static void args_dispose(args *a);

int main(int argc, char *argv[]) {
  int r = EXIT_SUCCESS;
  // temp?
  args *a = args_init(argc, argv);
  if (a == NULL) {
    goto error_capacity;
  }
  // start
  hashtable *ht = hashtable_empty((int (*)(const void *, const void *))strcmp,
      (size_t (*)(const void *))str_hashfun);
  if (ht == NULL) {
    goto error_capacity;
  }
  holdall *hastr = holdall_empty();
  if (hastr == NULL) {
    goto error_capacity;
  }
  char w[MAX_W_LEN + 1];
  for (int i = 1; i < a->filecount + 1; ++i) {
    FILE *f;
    bool is_stdin = a->file[i - 1][0] == '-';
    if (is_stdin) {
      // Read on stdin
      f = stdin;
      printf(
          FORMAT_INPUT_START "--- starts reading for #%d FILE"
          FORMAT_INPUT_STOP "\n",
          i);
    } else {
      // Read a file
      f = fopen(a->file[i - 1], "r");
      if (!f) {
        fprintf(stderr, "*** Could not open file: %s\n", a->file[i - 1]);
        goto error_read;
      }
    }
    while (fscanf(f, "%" XSTR(MAX_W_LEN) "s", w) == 1) {
      word *p = hashtable_search(ht, w);
      if (p != NULL) {
        if (p->f == i) {
          ++p->c;
        } else {
          p->f = 0;
        }
      } else {
        // Réalloue ; copie le mot
        char *s = malloc(strlen(w) + 1);
        if (s == NULL) {
          goto error_capacity;
        }
        strcpy(s, w);
        if (holdall_put(hastr, s) != 0) {
          goto error_capacity;
        }
        // Créer un compteur pour ce mot
        p = malloc(sizeof *p);
        if (p == NULL) {
          goto error_capacity;
        }
        p->w = s;
        p->c = 1;
        p->f = i;
        p->filtered = false;
        // Ajoute dans la hashtable
        if (hashtable_add(ht, s, p) == NULL) {
          goto error_capacity;
        }
      }
    }
    if (!feof(f)) {
      goto error_read;
    }
    if (!is_stdin && fclose(f) != 0) {
      goto error_read;
    }
    if (is_stdin) {
      printf(
          FORMAT_INPUT_START "--- ends reading for #%d FILE"
          FORMAT_INPUT_STOP "\n",
          i);
      clearerr(stdin);
    }
  }
  // Filtre? les mots
  if (a->filtered) {
    FILE *f = fopen(a->filter_fn, "r");
    if (!f) {
      goto error_read;
    }
    while (fscanf(f, "%" XSTR(MAX_W_LEN) "s", w) == 1) {
      word *p = hashtable_search(ht, w);
      if (p != NULL) {
        p->filtered = true;
      }
    }
    if (!feof(f) || fclose(f) != 0) {
      goto error_read;
    }
  }

  // tri





  // Affiche les entetes
  if (a->filtered) {
    printf("%s\t", a->filter_fn);
  } else {
    fputc('\t', stdin);
  }
  for (int i = 0; i < a->filecount; ++i) {
    if (a->file[i][0] == '-') { // !!!!!!!!!!!!!!
      printf("\"\"\t");
      continue;
    }
    printf("%s\t", a->file[i]);
  }
  fputc('\n', stdout);
  // AFFICHAGE DES COMPTEURS
  int (*wd)(void *, void *)
    = (int (*)(void *,
      void *))(a->filtered ? word_display_filter : word_display);
  holdall_apply_context(hastr, ht, (void *(*)(void *,
      void *))hashtable_remove,
      wd);
  goto dispose_partial;
error_capacity:
  r = EXIT_FAILURE;
  goto dispose; //?
error_read:
  r = EXIT_FAILURE;
  fprintf(stderr, "*** Error while reading a file\n");
  goto dispose;
dispose:
  holdall_apply_context(hastr, ht, (void *(*)(void *,
      void *))hashtable_remove, rfree2);
dispose_partial:
  hashtable_dispose(&ht);
  holdall_dispose(&hastr);
  args_dispose(a);
  return r;
}

// -----------------------------------------------------------------------------

size_t str_hashfun(const char *s) {
  size_t h = 0;
  for (const unsigned char *p = (const unsigned char *) s; *p != '\0'; ++p) {
    h = 37 * h + *p;
  }
  return h;
}

int rfree2(void *p1, void *p2) {
  free(p1);
  free(p2);
  return 0;
}

#define DISPLAY_WORD(s, w)                                                     \
  printf("%s\t", s);                                                           \
  for (int k = w->f; k > 1; --k) {                                             \
    fputc('\t', stdout);                                                       \
  }                                                                            \
  printf("%zu\n", w->c);                                                       \

int word_display(char *s, word *w) {
  if (w->f == 0) {
    return 0;
  }
  DISPLAY_WORD(s, w);
  free(s);
  free(w);
  return 0;
}

int word_display_filter(char *s, word *w) {
  if (!w->filtered || w->f == 0) {
    return 0;
  }
  DISPLAY_WORD(s, w);
  free(s);
  free(w);
  return 0;
}

//

//
int word_ccmp(const word *p1, const word *p2) {
  return p1->c - p2->c > 0 ? 1 : -1;
}

args *args_init(int argc, char *argv[]) {
  args *a = malloc(sizeof *a);
  if (a == NULL) {
    return NULL;
  }
  // Récupère les noms des fichier ; aucun paramètre accepté
  a->filecount = 0;
  a->file = malloc((size_t) (argc) * sizeof *a->file);
  if (a->file == NULL) {
    free(a);
    return NULL;
  }
  for (int i = 0; i < argc - 1; ++i) {
    a->file[i] = argv[i + 1];
    ++a->filecount;
  }
  // Filter
  a->filter_fn = "./txt/filter.txt";
  // Filtered?
  a->filtered = false;

  // sort
  a->sorted = true;
  a->sort_compare = (int (*)(const void *, const void *))word_ccmp;

  return a;
}

void args_dispose(args *a) {
  free(a->file);
  free(a);
}
