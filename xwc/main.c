


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

//  Macros ---------------------------------------------------------------------

//  STR, XSTR : permet de transformé une macro-constante en une chaine de
//    caractère qui contient son nom, sa valeur.
#define STR(s)  #s
#define XSTR(s) STR(s)

//  CHR : Donne le premier caractère de la chaine dont le contenu est celui
//    de la macro-constante qui lui est passée en paramètre ; du type int.
#define CHR_AUX(s) ((int) ((#s)[0]))
#define CHR(s) CHR_AUX(s)

//  Les directives suivantes sont des "raccourcis" pour les format de couleurs
//    ANSI
#define BGCOLOR_WHITE "\x1b[107m"
#define BGCOLOR_RESET "\x1b[49m"

#define FGCOLOR_BLACK "\x1b[30m"
#define FGCOLOR_RESET "\x1b[39m"

#define FORMAT_INPUT_START BGCOLOR_WHITE FGCOLOR_BLACK
#define FORMAT_INPUT_STOP BGCOLOR_RESET FGCOLOR_RESET

//  ARGS__* : utilisées pour les paramètres de l'executable
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


//  Structures -----------------------------------------------------------------

//  struct wordstream, wordstream : utilisé pour représenté un flux de texte,
//    accessible par stream s'il a été ouvert (is_open). Il est soit égal à
//    stdin, soit il s'agit du fichier de chemin filename, ouvert en mode
//    lecture
//  Les valeurs sont accessibles, mais le comportement devient indéterminé
//    si elles sont modifiés en dehors des fonctions wordstream_*
typedef struct wordstream wordstream;
struct wordstream {
  FILE *stream;
  bool is_open;
  bool is_stdin;
  char *filename;
};

//  struct args, args : représente les paramètres de l'executable.
//  - file, filecount : les flux de textes à traiter et leur nombre.
//  - filtered, filter : filtered est à true si le comptage des mots est filtré,
//      si c'est le cas alors le filtre a utilisé est pointé par filter
//  - only_alpha_num : défini si les caractère de ponctuation des mots lus
//      doivent être considérés comme des espaces
//  - max_w_len : Longueur maximale des mots lus, si un mot est plus long il
//      coupé. Par défaut 0, qui représente l'absence de limite
//  - sort_type : Tri utilisé pour l'affichage des compteurs, qui est égal à une
//      des maccro-constantes de nom ARGS__SORT_VAL_*
//  - sort_reversed : Défini si le tri se fait dans l'ordre inverse
typedef struct args args;
struct args {
  wordstream **file;
  int filecount;
  bool filtered;
  wordstream *filter;
  bool only_alpha_num;
  size_t max_w_len;
  int sort_type;
  bool sort_reversed;
};

//  Fonctions ------------------------------------------------------------------

//  Fonctions pour wordstream --------------------------------------------------

//  wordstream_new : Initialise un flux de texte non ouvert. Si filename est la
//    chaine "-" alors le flux représente l'entrée standard, is_stdin est à
//    true, et son nom de fichier prends la valeur default_fn. Renvoie NULL en
//    cas de dépassement de capacité, le nouveau flux sinon.
static wordstream *wordstream_new(const char *filename, const char *default_fn);

//  wordstream_stdin : Initialise un flux de texte qui est l'entrée standard, et
//    dont le nom de de fichier défaut est default_fn. Renvoie NULL en cas de
//    dépassement de capacité, le nouveau flux sinon.
static wordstream *wordstream_stdin(const char *default_fn);

//  wordstream_pdispose : sans effet si w vaut NULL, sinon libère les ressources
//    nécessaire à la gestion du flux pointé par *w, puis affecte NULL à *w
static void wordstream_pdispose(wordstream **w);

//  wordsteam_popen : tente d'ouvrir le flux associé à w. Renvoie 0 en cas de
//    succès, 1 si le flux est déjà ouvert. En cas d'erreur, affiche un message
//    sur la sortie erreur, et renvoie -1.
static int wordstream_popen(wordstream *w);

//  wordstream_pclose : Tente de fermer le flux w. Affiche un message sur la
//    sortie erreur et renvoie -1 en cas d'erreur de fermeture. Sinon le flux
//    est correctement fermé et 0 est renvoyé.
static int wordstream_pclose(wordstream *w);

//  wordstream_pfn : Affiche le nom de fichier associé au flux w sur la sortie
//    stream. Ce nom est le nom du fichier lu, ou "" s'il s'agit de l'entrée
//    standard.
static void wordstream_pfn(wordstream *w, FILE *stream);

//  Fonctions pour args --------------------------------------------------------

//  args_init : tente d'allouer les ressources pour stocké les paramètre données
//    par argc, argv. Renvoie NULL en cas d'erreur, renvoie sinon la structure
//    nouvellement allouée.
//  En cas d'erreur un code d'erreur est associé à error, qui ne doit pas être
//    NULL. 0 en cas de dépassement de capacité. //////////////////////////////////////////////////////////////////////////////
static args *args_init(int argc, char *argv[], int *error);

//  args_dispose : Libère les ressources nécessaire à la gestion de *a. Le
//    pointeur a réfèrence après appel une zone non allouée.
static void args_dispose(args *a);

//  Fonctions auxiliaires ------------------------------------------------------

//  rword_put : Sans effet si le canal de w vaut MULTI_CHANNEL. Sinon affiche le
//    mot w sur la sortie standard, précédé d'un nombre de tabulation cohérent
//    par rapport à son canal. Affiche ensuite le nombre d'occurences de ce mot
//    puis saute à la ligne. Renvoie 0 dans tous les cas.
static int rword_put(const word *w);

//  rword_put_filter : similaire à rword_put, mais n'affiche que les mots dont
//    le canal est différent de MULTI_CHANNEL et UNDEFINED_CHANNEL
static int rword_put_filter(const word *w);

//  Main -----------------------------------------------------------------------

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

//  Définitions des fonctions --------------------------------------------------

//  DISPLAY_WORD : affiche le mot w, son compteur d'occurences et un saut de
//    lignes, le tout précédé de tabulations en fonction du canal de w.
#define DISPLAY_WORD(w)                                                        \
  printf("%s\t", word_str(w));                                                 \
  for (int k = word_channel(w); k > START_CHANNEL; --k) {                      \
    fputc('\t', stdout);                                                       \
  }                                                                            \
  printf("%zu\n", word_count(w));

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

//  ----------------------------------------------------------------------------

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
      return -1;
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

//  ----------------------------------------------------------------------------

//  Représente la chaine de caractère des options de l'executable, à passer
//    à getopt.
#define ARGS__OPT_STRING \
  XSTR(ARGS__RESTRICT) ":" \
  XSTR(ARGS__ONLY_ALPHA_NUM) \
  XSTR(ARGS__LIMIT_WLEN) ":" \
  XSTR(ARGS__SORT_REVERSE) \
  XSTR(ARGS__SORT_LEXICAL) \
  XSTR(ARGS__SORT_NUMERIC) \
  XSTR(ARGS__SORT_NONE) \
  XSTR(ARGS__SORT_TYPE) ":"

//  ARGS__SORT_COND : on suppose qu'il existe un entier opt qui est l'option en
//    cours de traitement. L'expression vaut true si cette option (et sa valeur
//    si besoin) correspond au tri SORT_TYPE.
#define ARGS__SORT_COND(SORT_TYPE)                                             \
  (                                                                            \
  opt == CHR(ARGS__SORT_ ## SORT_TYPE)                                         \
  || (                                                                         \
     opt == CHR(ARGS__SORT_TYPE)                                               \
     && strcmp(optarg, ARGS__SORT_TYPE_ ## SORT_TYPE) == 0                     \
     )                                                                         \
  )

//  args__set_filtered : défini l'option "filtré" de a à true, avec comme filtre
//    le fichier de nom filter_fn.
static int args__set_filtered(args *a, const char *filter_fn) {
  a->filter = wordstream_new(filter_fn, "restrict");
  if (a->filter == NULL) {
    return -1;
  }
  a->filtered = true;
  return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void args_dispose(args *a) {
  for (int i = 0; i < a->filecount; ++i) {
    wordstream_pdispose(&a->file[i]);
  }
  free(a->file);
  wordstream_pdispose(&a->filter);
  free(a);
}