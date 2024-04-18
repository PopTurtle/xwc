#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include "hashtable.h"
#include "holdall.h"
#include "wordcounter.h"

#include <locale.h>

#define STR(s)  #s
#define XSTR(s) STR(s)//?


#define MAX_W_LEN 32//


#define BGCOLOR_WHITE "\x1b[107m"
#define BGCOLOR_RESET "\x1b[49m"

#define FGCOLOR_BLACK "\x1b[30m"
#define FGCOLOR_RESET "\x1b[39m"

#define FORMAT_INPUT_START BGCOLOR_WHITE FGCOLOR_BLACK
#define FORMAT_INPUT_STOP BGCOLOR_RESET FGCOLOR_RESET

#define CHR_AUX(s) ((int) ((#s)[0]))
#define CHR(s) CHR_AUX(s)


#define ARGS__RESTRICT r
#define ARGS__ONLY_ALPHA_NUM p
#define ARGS__LIMIT_WLEN i

#define ARGS__SORT_REVERSE R

#define ARGS__SORT_TYPE s

#define ARGS__SORT_LEXICAL l
#define ARGS__SORT_TYPE_LEXICAL "lexicographical"
#define ARGS__SORT_VAL_LEXICAL 2

#define ARGS__SORT_NUMERIC n
#define ARGS__SORT_TYPE_NUMERIC "numeric"
#define ARGS__SORT_VAL_NUMERIC 1

#define ARGS__SORT_NONE S
#define ARGS__SORT_TYPE_NONE "none"
#define ARGS__SORT_VAL_NONE 0


typedef struct args args;
struct args {
  const char **file;
  int filecount;

  bool filtered;
  const char *filter_fn;

  bool only_alpha_num;

  int sort_type;
  bool sort_reversed;
};


typedef struct textfile textfile;
struct textfile {
  char *filename;
  FILE *curr_file;
  bool is_stdin;
  bool is_open;
};

void textfile_dispose(textfile **t);
textfile *textfile_new(const char *filename, const char *default_fn);
int textfile_popen(textfile *t);
int textfile_pclose(textfile *t);


void args_dispose(args *a);
args *args_init(int argc, char *argv[], int *error);


static int rword_put(const word *w);
static int rword_put_filter(const word *w);



int main(int argc, char *argv[]) {
  int r = EXIT_SUCCESS;
  // Récupèration des arguments// !!!!!!!!!!!!!!!!!!!!!!!!!// !!!!!!!!!!!!!!!!!!!!!!!!!// !!!!!!!!!!!!!!!!!!!!!!!!!// !!!!!!!!!!!!!!!!!!!!!!!!!// !!!!!!!!!!!!!!!!!!!!!!!!!// !!!!!!!!!!!!!!!!!!!!!!!!!
  int arg_err;
  args *a = args_init(argc, argv, &arg_err); // !!!!!!!!!!!!!!!!!!!!!!!!!
  if (arg_err != 0) {
    // Arguments incohérents
    return EXIT_FAILURE;
  }
  if (a == NULL) {
    goto error_capacity;
  }

  // Locale
  setlocale(LC_COLLATE, "");

  // Création du compteur de mots
  wordcounter *wc = wc_empty(a->filtered);
  if (wc == NULL) {
    goto error_capacity;
  }

  // Application du filtre si demandé
  if (a->filtered) {
    textfile *tf = textfile_new(a->filter_fn, "restrict");
    if (tf == NULL) {
      goto error_capacity;
    }
    if (textfile_popen(tf) != 0) {
      goto error_read;
    }
    if (wc_file_add_filtered(wc, tf->curr_file) != 0) {
      textfile_dispose(&tf);
      goto error_capacity;
    }
    int r = textfile_pclose(tf);
    textfile_dispose(&tf);
    if (r != 0) {
      goto error_read;
    }
  }
  
  // Analyse des différents fichiers
  for (int i = 0; i < a->filecount; ++i) {
    int channel = START_CHANNEL + i;
    textfile *t = textfile_new(a->file[i], "DEFAULT");
    if (t ==  NULL) {
      goto error_capacity;
    }
    if (textfile_popen(t) != 0) {
      goto error_read;
    }
    if (wc_filecount(wc, t->curr_file, 0, a->only_alpha_num, channel) != 0) {
      textfile_dispose(&t);
      goto error_capacity;
    }
    int r = textfile_pclose(t);
    textfile_dispose(&t);
    if (r != 0) {
      goto error_read;
    }
  }

  // Tri si demandé
  if (a->sort_type != ARGS__SORT_VAL_NONE) {
    void (*sort_fun)(wordcounter *) = NULL;
    if (a->sort_type == ARGS__SORT_VAL_LEXICAL) {
      sort_fun = a->sort_reversed ? wc_sort_lexical_reverse : wc_sort_lexical;
    } else if (a->sort_type == ARGS__SORT_VAL_NUMERIC) {
      sort_fun = a->sort_reversed ? wc_sort_count_reverse : wc_sort_count;
    }
    if (sort_fun != NULL) {
      sort_fun(wc);
    }
  }

  // Affiche les entetes
  if (a->filtered) { // - === "" !! !! ! !!
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

  //wc_sort_lexical(wc);

  int (*wd)(const word *) = a->filtered ? rword_put_filter : rword_put;
  wc_apply(wc, (int (*)(word *))wd);
  goto dispose;





error_capacity:
  r = EXIT_FAILURE;
  fprintf(stderr, "*** Error capacity\n");
  goto dispose; //?
error_read:
  r = EXIT_FAILURE;
  fprintf(stderr, "*** Error while reading a file\n");
  goto dispose;
dispose:
  wc_dispose(&wc);
  args_dispose(a);
  
  return r;
}





#define DISPLAY_WORD(w)                                                        \
  printf("%s\t", word_str(w));                                                 \
  for (int k = word_channel(w); k > 1; --k) {                                  \
    fputc('\t', stdout);                                                       \
  }                                                                            \
  printf("%zu\n", word_count(w));                                              \


static int rword_put(const word *w) {
  if (word_channel(w) == MULTI_CHANNEL) {
    return 0;
  }
  DISPLAY_WORD(w);
  return 0;
}

static int rword_put_filter(const word *w) {
  if (word_channel(w) == MULTI_CHANNEL || word_channel(w) == UNDEFINED_CHANNEL) {
    return 0;
  }
  DISPLAY_WORD(w);
  return 0;
}




#define ARGS__OPT_STRING \
  XSTR(ARGS__RESTRICT) ":" \
  XSTR(ARGS__ONLY_ALPHA_NUM) \
  XSTR(ARGS__LIMIT_WLEN) ":" \
  XSTR(ARGS__SORT_REVERSE) \
  XSTR(ARGS__SORT_LEXICAL) \
  XSTR(ARGS__SORT_NUMERIC) \
  XSTR(ARGS__SORT_NONE) \
  XSTR(ARGS__SORT_TYPE) ":"


#define ARGS__IFSORT(ARGS_PTR, SORT_TYPE) \
  if (opt == CHR(ARGS__SORT_ ## SORT_TYPE) \
      || (opt == CHR(ARGS__SORT_TYPE) \
      && strcmp(optarg, ARGS__SORT_TYPE_ ## SORT_TYPE) == 0)) { \
    ARGS_PTR->sort_type = ARGS__SORT_VAL_ ## SORT_TYPE; \
  }

// NULL - check err, si err == NULL , si err == 0: capacity, si err autre, err
args *args_init(int argc, char *argv[], int *error) {
  if (error == NULL) {
    return NULL;
  }
  *error = 0;
  args *a = malloc(sizeof *a);
  if (a == NULL) {
    return NULL;
  }

  a->filtered = false;
  a->only_alpha_num = false;
  
  a->sort_type = ARGS__SORT_VAL_NONE;
  a->sort_reversed = false;

  printf("OPT STRING: %s\n", ARGS__OPT_STRING);/////////////////
  printf("%c\n", CHR(ARGS__RESTRICT));/////////////////
  printf("%c\n", CHR(ARGS__ONLY_ALPHA_NUM));/////////////////

  int opt;
  opterr = 0;
  while ((opt = getopt(argc, argv, ARGS__OPT_STRING)) != -1) {
    printf("%d: %c / arg: %s\n", optind, opt, optarg); /////////////////
    if (opt == CHR(ARGS__RESTRICT)) {
      a->filtered = true;
      a->filter_fn = optarg;
    } else if (opt == CHR(ARGS__ONLY_ALPHA_NUM)) {
      a->only_alpha_num = true;
    } else if (opt == CHR(ARGS__LIMIT_WLEN)) {
      printf("Want to limit w len to %s\n", optarg);/////////////////
    } else if (opt == CHR(ARGS__SORT_REVERSE)) {
      a->sort_reversed = true;
    }

    ARGS__IFSORT(a, LEXICAL)
    else ARGS__IFSORT(a, NUMERIC)
    else ARGS__IFSORT(a, NONE)
    else if (opt == CHR(ARGS__SORT_TYPE)) {
      printf("Tri inconnu au bataillon\n");/////////////////
    }
  }

  printf("Sort type: %d Reversed: %d\n", a->sort_type, a->sort_reversed ? 1 : 0);/////////////////

  // Récupère les noms des fichier
  a->filecount = argc - optind;
  bool no_file = a->filecount == 0;

  if (no_file) {
    a->filecount = 1;
  }

  a->file = malloc((size_t) (a->filecount) * sizeof *a->file);
  if (a->file == NULL) {
    free(a);
    return NULL;
  }

  printf("args::Files:\n");/////////////////
  if (no_file) {
    a->file[0] = "-";
  } else {
    for (int i = 0; i < a->filecount; ++i) {
      a->file[i] = argv[optind + i];
      printf(" - %d : %s\n", i + 1, argv[optind + i]);/////////////////
    }
  }


  return a;
}

void args_dispose(args *a) {
  free(a->file);
  free(a);
}


void textfile_dispose(textfile **t) {
  if ((*t)->is_open) {
    textfile_pclose(*t);
  }
  free((*t)->filename);
  free(*t);
  *t = NULL;
}

textfile *textfile_new(const char *filename, const char *default_fn) {
  textfile *t = malloc(sizeof *t);
  if (t == NULL) {
    return NULL;
  }
  t->is_stdin = strcmp(filename, "-") == 0;
  const char *fn = t->is_stdin ? default_fn : filename;
  char *s = malloc(strlen(fn) + 1);
  if (s == NULL) {
    free(t);
    return NULL;
  }
  strcpy(s, fn);
  t->filename = s;
  t->is_open = false;
  return t;
}

int textfile_popen(textfile *t) {
    FILE *f;
    if (t->is_stdin) {
      // Read on stdin
      f = stdin;
      printf(
          FORMAT_INPUT_START "--- starts reading for %s FILE"
          FORMAT_INPUT_STOP "\n",
          t->filename);
    } else {
      // Read a file
      f = fopen(t->filename, "r");
      if (!f) {
        fprintf(stderr, "*** Could not open file: %s\n", t->filename);
        return -1;
      }
    }
    t->curr_file = f;
    t->is_open = true;
    return 0;
}

int textfile_pclose(textfile *t) {
    t->is_open = false;
    if (!feof(t->curr_file)) {
      fprintf(stderr, "*** ERR: %s\n", t->filename);
    }
    if (!t->is_stdin && fclose(t->curr_file) != 0) {
      return -1;
    }
    if (t->is_stdin) {
      printf(
          FORMAT_INPUT_START "--- ends reading for %s FILE"
          FORMAT_INPUT_STOP "\n",
          t->filename);
      clearerr(stdin);
    }
    return 0;
}
