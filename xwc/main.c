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

// 

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


// 


typedef struct wordstream wordstream;
struct wordstream {
  char *filename;
  bool is_stdin;
  bool is_open;
  FILE *stream;
};
//  struct args, args : représente les arguments de l'executable.
typedef struct args args;
struct args {
  //  Les noms de fichiers passés (*file), et le nombre de fichier (filecount)
  //const char **file;
  wordstream **file;
  int filecount;

  //  Défini si un filtre est appliqué, et si oui le nom du fichier du filtre
  //    est associé à filter_fn
  bool filtered;
  //const char *filter_fn;
  wordstream *filter;

  //  Défini si les caractères de ponctuations seront considérés
  //    comme des espace
  bool only_alpha_num;

  // Défini la longueur maximale d'un mot
  size_t max_w_len;

  //  Tri utilisé pour l'affichage des compteurs, qui est égal à une des
  //    maccro-constantes de nom ARGS__SORT_VAL_*
  int sort_type;

  // Défini si le tri se fait dans l'ordre inverse
  bool sort_reversed;
};



//typedef struct textfile textfile;
//struct textfile {
//  char *filename;
//  FILE *curr_file;
//  bool is_stdin;
//  bool is_open;
//};

//void textfile_dispose(textfile **t);
//textfile *textfile_new(const char *filename, const char *default_fn);
//int textfile_popen(textfile *t);
//int textfile_pclose(textfile *t);


wordstream *wordstream_new(const char *filename, const char *default_fn);
wordstream *wordstream_stdin(const char *default_fn);
void wordstream_pdispose(wordstream **w);
int wordstream_popen(wordstream *w);
int wordstream_pclose(wordstream *w);
void wordstream_pfn(wordstream *w, FILE *stream);


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
    wordstream *ws = a->filter;// wordstream_new(a->filter_fn, "restrict");
    if (ws == NULL) {
      goto error_capacity;
    }
    if (wordstream_popen(ws) != 0) {
      goto error_read;
    }
    int rf = wc_file_add_filtered(wc, ws->stream, a->max_w_len, a->only_alpha_num);
    if (rf != 0) {
      //wordstream_pdispose(&ws);
      if (rf == 2) {
        goto error_read;
      }
      goto error_capacity;
    }
    if (wordstream_pclose(ws) != 0) {
      goto error_read;
    }
  }
  
  // Analyse des différents fichiers
  for (int i = 0; i < a->filecount; ++i) {
    int channel = START_CHANNEL + i;
    wordstream *ws = a->file[i]; //wordstream_new(a->file[i], "DEFAULT");
    if (ws == NULL) {
      goto error_capacity;
    }
    if (wordstream_popen(ws) != 0) {
      goto error_read;
    }
    int rc = wc_filecount(wc, ws->stream, a->max_w_len, a->only_alpha_num, channel);
    if (rc != 0) {
      //wordstream_pdispose(&ws);
      if (rc == 2) {
        goto error_read;
      }
      goto error_capacity;
    }
    int r = wordstream_pclose(ws);
    //wordstream_pdispose(&ws);
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
    wordstream_pfn(a->filter, stdout);
  }
  fputc('\t', stdin);
  for (int i = 0; i < a->filecount; ++i) {
    wordstream_pfn(a->file[i], stdout);
    printf("\t");
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


#define ARGS__SORT_COND(SORT_TYPE) \
     opt == CHR(ARGS__SORT_ ## SORT_TYPE) \
  || (opt == CHR(ARGS__SORT_TYPE) \
  && strcmp(optarg, ARGS__SORT_TYPE_ ## SORT_TYPE) == 0)


// typedef struct args args;
// struct args {
//   wordstream **file;
//   int filecount;

//   bool filtered;
//   wordstream *filter;

//   bool only_alpha_num;
//   size_t max_w_len;

//   int sort_type;
//   bool sort_reversed;
// };

int args__set_filtered(args *a, const char *filter_fn) {
  a->filter = wordstream_new(filter_fn, "restrict");
  if (a->filter == NULL) {
    return -1;
  }
  a->filtered = true;
  return 0;
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

  // Valeurs par défaut
  a->filtered = false;
  a->filter = NULL;
  a->only_alpha_num = false;
  a->max_w_len = 0;
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
      args__set_filtered(a, optarg);
    } else if (opt == CHR(ARGS__ONLY_ALPHA_NUM)) {
      a->only_alpha_num = true;
    } else if (opt == CHR(ARGS__LIMIT_WLEN)) {
      printf("Want to limit w len to %s\n", optarg);/////////////////
    } else if (opt == CHR(ARGS__SORT_REVERSE)) {
      a->sort_reversed = true;
    }

    if (ARGS__SORT_COND(LEXICAL)) {
      a->sort_type = ARGS__SORT_VAL_LEXICAL;
    } else if (ARGS__SORT_COND(NUMERIC)) {
      a->sort_type = ARGS__SORT_VAL_NUMERIC;
    } else if (ARGS__SORT_COND(NONE)) {
      a->sort_type =ARGS__SORT_VAL_NONE;
    }
    else if (opt == CHR(ARGS__SORT_TYPE)) {
      printf("Tri inconnu au bataillon\n");/////////////////
    }
  }

  printf("Sort type: %d Reversed: %d\n", a->sort_type, a->sort_reversed ? 1 : 0);/////////////////

  // Gestion des fichiers
  a->filecount = argc - optind;
  bool no_file = a->filecount == 0;

  if (no_file) {
    a->filecount = 1;
  }

  a->file = malloc((size_t) (a->filecount) * sizeof *a->file);
  if (a->file == NULL) {
    wordstream_pdispose(&a->filter);
    free(a);
    return NULL;
  }

  int tf = 0;
  bool ec = false;
  if (no_file) {
    a->file[0] = wordstream_stdin("#1");
    ++tf;
    ec = a->file[0] == NULL;
  } else {
    for (; tf < a->filecount; ++tf) {
      a->file[tf] = wordstream_new(argv[optind + tf], "DEFAULT");
      if (a->file[tf] == NULL) {
        ec = true;
        break;
      }
      printf(" - %d : %s\n", tf + 1, argv[optind + tf]);/////////////////
    }
  }

  if (ec) {
    a->filecount = tf;
    args_dispose(a);
    return NULL;
  }
  return a;
}

void args_dispose(args *a) {
  for (int i = 0; i < a->filecount; ++i) {
    wordstream_pdispose(&a->file[i]);
  }
  free(a->file);
  wordstream_pdispose(&a->filter);
  free(a);
}

// -------------------------------------------------------------------------------------------------------------------------


//typedef struct wordstream wordstream;
//struct wordstream {
//  char *filename;
//  bool is_stdin;
//  bool is_open;
//  FILE *stream;
//};
//
//typedef struct textfile textfile;
//struct textfile {
//  char *filename;
//  FILE *curr_file;
//  bool is_stdin;
//  bool is_open;
//};

wordstream *wordstream_new(const char *filename, const char *default_fn) {
  wordstream *w = malloc(sizeof *w);
  if (w == NULL) {
    return NULL;
  }
  // Défini s'il s'agit de l'entrée standard
  w->is_stdin = *filename == '-';
  const char *fn = w->is_stdin ? default_fn : filename;
  // Copie le nom de fichier choisi
  char *s = malloc(strlen(fn) + 1);
  if (s == NULL) {
    free(w);
    return NULL;
  }
  strcpy(s, fn);
  w->filename = s;
  w->is_open = false;
  w->stream = NULL;
  return w;
}

wordstream *wordstream_stdin(const char *default_fn) {
  return wordstream_new("-", default_fn);
}

void wordstream_pdispose(wordstream **w) {
  if (*w == NULL) {
    return;
  }
  if ((*w)->is_open) {
    wordstream_pclose(*w);
  }
  free((*w)->filename);
  free(*w);
  *w = NULL;
}

int wordstream_popen(wordstream *w) {
  if (w->is_open) {
    return 1;
  }
  FILE *f;
  if (w->is_stdin) {
    f = stdin;
    printf(
      FORMAT_INPUT_START "--- starts reading for %s FILE"
      FORMAT_INPUT_STOP "\n", w->filename
    );
  } else {
    f = fopen(w->filename, "r");
    if (!f) {
      fprintf(stderr, "*** Could not open file: %s\n", w->filename);
      return -1;
    }
  }
  w->stream = f;
  w->is_open = true;
  return 0;
}

int wordstream_pclose(wordstream *w) {
  w->is_open = false;
  if (!w->is_stdin && fclose(w->stream) != 0) {
      fprintf(stderr, "*** Erreur lors de la fermeture du fichier: %s\n", w->filename);
  }
  if (w->is_stdin) {
    printf(
        FORMAT_INPUT_START "--- ends reading for %s FILE"
        FORMAT_INPUT_STOP "\n",
        w->filename);
    clearerr(stdin);
  }
  return 0;
}

void wordstream_pfn(wordstream *w, FILE *stream) {
  if (w->is_stdin) {
    fprintf(stream, "\"\"");
    return;
  }
  fprintf(stream, "%s", w->filename);
}






// void textfile_dispose(textfile **t) {
//   if ((*t)->is_open) {
//     textfile_pclose(*t);
//   }
//   free((*t)->filename);
//   free(*t);
//   *t = NULL;
//}

// textfile *textfile_new(const char *filename, const char *default_fn) {
//   textfile *t = malloc(sizeof *t);
//   if (t == NULL) {
//     return NULL;
//   }
//   t->is_stdin = strcmp(filename, "-") == 0;
//   const char *fn = t->is_stdin ? default_fn : filename;
//   char *s = malloc(strlen(fn) + 1);
//   if (s == NULL) {
//     free(t);
//     return NULL;
//   }
//   strcpy(s, fn);
//   t->filename = s;
//   t->is_open = false;
//   return t;
// }

// int textfile_popen(textfile *t) {
//     FILE *f;
//     if (t->is_stdin) {
//       // Read on stdin
//       f = stdin;
//       printf(
//           FORMAT_INPUT_START "--- starts reading for %s FILE"
//           FORMAT_INPUT_STOP "\n",
//           t->filename);
//     } else {
//       // Read a file
//       f = fopen(t->filename, "r");
//       if (!f) {
//         fprintf(stderr, "*** Could not open file: %s\n", t->filename);
//         return -1;
//       }
//     }
//     t->curr_file = f;
//     t->is_open = true;
//     return 0;
// }

// int textfile_pclose(textfile *t) {
//     t->is_open = false;
//     if (!feof(t->curr_file)) {
//       fprintf(stderr, "*** ERR: %s\n", t->filename);
//     }
//     if (!t->is_stdin && fclose(t->curr_file) != 0) {
//       return -1;
//     }
//     if (t->is_stdin) {
//       printf(
//           FORMAT_INPUT_START "--- ends reading for %s FILE"
//           FORMAT_INPUT_STOP "\n",
//           t->filename);
//       clearerr(stdin);
//     }
//     return 0;
// }
