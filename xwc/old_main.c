#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
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
  char *s;
  long unsigned int count;
  int file_id;
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
  int (*sort_compare)(const word *, const word *);
};

//  str_hashfun : l'une des fonctions de pré-hachage conseillées par Kernighan
//    et Pike pour les chaines de caractères. //////////////////////////////////
static size_t str_hashfun(const char *s); /////////////////////////////////////

//  rfree2 : libère les zones mémoires associées à p1 et p2, puis renvoie 0

static void word_dispose(word *p) {
  free(p->s);
  free(p);
}

static int rword_dispose(word *p) {
  word_dispose(p);
  return 0;
}

//
static int word_display(word *w);

//
static int word_display_filter(word *w);

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
  holdall *haw = holdall_empty();
  if (haw == NULL) {
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
        if (p->file_id == i) {
          ++p->count;
        } else {
          p->file_id = 0;
        }
      } else {
        // Réalloue ; copie le mot
        char *s = malloc(strlen(w) + 1);
        if (s == NULL) {
          goto error_capacity;
        }
        // Créer un compteur pour ce mot
        p = malloc(sizeof *p);
        if (p == NULL) {
          free(s);
          goto error_capacity;
        }
        strcpy(s, w);
        p->s = s;
        p->count = 1;
        p->file_id = i;
        p->filtered = false;
        // Ajoute dans la hashtable
        if (holdall_put(haw, p) != 0) {
          word_dispose(p);
          goto error_capacity;
        }
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
  //if (a->sorted) {
    //holdall_sort(haw, (int (*)(const void *, const void *))a->sort_compare);
  //}
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
  int (*wd)(word *) = a->filtered ? word_display_filter : word_display;
  holdall_apply(haw, (int (*)(void *))wd);
  goto dispose_partial;
error_capacity:
  r = EXIT_FAILURE;
  goto dispose; //?
error_read:
  r = EXIT_FAILURE;
  fprintf(stderr, "*** Error while reading a file\n");
  goto dispose;
dispose:
  holdall_apply(haw, (int (*)(void *))rword_dispose);
dispose_partial:
  hashtable_dispose(&ht);
  holdall_dispose(&haw);
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

#define DISPLAY_WORD(w)                                                        \
  printf("%s\t", w->s);                                                         \
  for (int k = w->file_id; k > 1; --k) {                                       \
    fputc('\t', stdout);                                                       \
  }                                                                            \
  printf("%zu\n", w->count);                                                   \

int word_display(word *w) {
  if (w->file_id == 0) {
    return 0;
  }
  DISPLAY_WORD(w);
  word_dispose(w);
  return 0;
}

int word_display_filter(word *w) {
  if (!w->filtered || w->file_id == 0) {
    return 0;
  }
  DISPLAY_WORD(w);
  word_dispose(w);
  return 0;
}

//

//
int word_ccmp(const word *p1, const word *p2) {
  return p1->count - p2->count > 0 ? 1 : -1;
}

args *args_init(int argc, char *argv[]) {
  args *a = malloc(sizeof *a);
  if (a == NULL) {
    return NULL;
  }

  int opt;
  opterr = 0;
  while ((opt = getopt(argc, argv, "lns:")) != -1) {
    printf("%d: %c / arg: %s\n", optind, opt, optarg);
    switch (opt) {
      case 'l':
      case 'n':
      case 's':
        a->sorted = true;
        break;
      default:
        fprintf(stderr, "Unrecognised option in expression '%s'\n", argv[optind - 1]);
        exit(EXIT_FAILURE);
    }
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
  a->sort_compare = word_ccmp;
  return a;
}

void args_dispose(args *a) {
  free(a->file);
  free(a);
}
