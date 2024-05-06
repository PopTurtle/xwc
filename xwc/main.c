#include "hashtable.h"
#include "holdall.h"
#include "wordcounter.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <locale.h>
#include <getopt.h>

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
#define ESC_COLOR_INVERSE "\x1b[7m"
#define ESC_COLOR_INVERSE_RESET "\x1b[27m"

#define FORMAT_INPUT_START ESC_COLOR_INVERSE
#define FORMAT_INPUT_STOP ESC_COLOR_INVERSE_RESET

//  ARGS__* : utilisées pour les paramètres de l'executable ; ils ont des
//    valeurs différentes de '?' et ':', à l'exception de ARGS__HELP, qui peut
//    prendre la valeur '?'.
#define ARGS__HELP ?

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
//  - max_w_len : longueur maximale des mots lus, si un mot est plus long il
//      coupé. par défaut 0, qui représente l'absence de limite
//  - sort_type : tri utilisé pour l'affichage des compteurs, qui est égal à une
//      des maccro-constantes de nom ARGS__SORT_VAL_*
//  - sort_reversed : défini si le tri se fait dans l'ordre inverse
//  - help : faut-il afficher l'aide ?
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
  bool help;
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
//    nouvellement allouée. Si l'option d'aide (ARGS__HELP) est lue, alors les
//    options suivantes ne sont pas évalués, et a->help est à true.
//  En cas d'erreur un code d'erreur est associé à *error (error ne doit pas
//    être NULL). 0 en cas de dépassement de capacité. -1 s'il y a eu une erreur
//    lors de la lecture d'un argument, dans ce cas il est possible qu'un
//    message d'erreur soit affiché sur stderr.
static args *args_init(int argc, char *argv[], int *error);

//  args_dispose : sans effet si a est *a vaut NULL. Sinon libère les ressources
//    nécessaires à la gestion de a, puis affecte NULL à *a;
static void args_dispose(args **a);

//  Fonctions auxiliaires ------------------------------------------------------

//  rword_put : Sans effet si le canal de w vaut MULTI_CHANNEL. Sinon affiche le
//    mot w sur la sortie standard, précédé d'un nombre de tabulation cohérent
//    par rapport à son canal. Affiche ensuite le nombre d'occurences de ce mot
//    puis saute à la ligne. Renvoie 0 dans tous les cas.
static int rword_put(const word *w);

//  rword_put_filter : similaire à rword_put, mais n'affiche que les mots dont
//    le canal est différent de MULTI_CHANNEL et UNDEFINED_CHANNEL
static int rword_put_filter(const word *w);

//  print_help : affiche l'aide sur la sortie standard.
static void print_help();

//  Main -----------------------------------------------------------------------

int main(int argc, char *argv[]) {
  int r = EXIT_SUCCESS;
  // Récupèration des arguments
  int arg_err;
  args *a = args_init(argc, argv, &arg_err);
  if (arg_err != 0) {
    return EXIT_FAILURE;
  }
  if (a == NULL) {
    goto error_capacity;
  }
  // Affiche la page d'aide ?
  if (a->help) {
    print_help();
    args_dispose(&a);
    return r;
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
    wordstream *ws = a->filter;
    if (ws == NULL) {
      goto error_capacity;
    }
    if (wordstream_popen(ws) != 0) {
      goto error_read;
    }
    int rf = wc_file_add_filtered(wc, ws->stream, a->max_w_len,
        a->only_alpha_num);
    if (rf != 0) {
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
    wordstream *ws = a->file[i];
    if (ws == NULL) {
      goto error_capacity;
    }
    if (wordstream_popen(ws) != 0) {
      goto error_read;
    }
    int rc = wc_filecount(wc, ws->stream, a->max_w_len, a->only_alpha_num,
        channel);
    if (rc != 0) {
      if (rc == 2) {
        goto error_read;
      }
      goto error_capacity;
    }
    int r = wordstream_pclose(ws);
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
  // Affichage des entetes
  if (a->filtered) {
    wordstream_pfn(a->filter, stdout);
  }
  for (int i = 0; i < a->filecount; ++i) {
    fputc('\t', stdout);
    wordstream_pfn(a->file[i], stdout);
  }
  fputc('\n', stdout);
  // Affichage des compteurs
  int (*wd)(const word *) = a->filtered ? rword_put_filter : rword_put;
  wc_apply(wc, (int (*)(word *))wd);
  goto dispose;
  // Gestion des erreurs et sortie du programme
  error_capacity
  : r = EXIT_FAILURE;
  fprintf(stderr, "*** Error capacity\n");
  goto dispose;
error_read:
  r = EXIT_FAILURE;
  fprintf(stderr, "*** Error while reading a file\n");
  goto dispose;
dispose:
  wc_dispose(&wc);
  args_dispose(&a);
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

int rword_put(const word *w) {
  if (word_channel(w) == MULTI_CHANNEL) {
    return 0;
  }
  DISPLAY_WORD(w);
  return 0;
}

int rword_put_filter(const word *w) {
  if (word_channel(w) == MULTI_CHANNEL
      || word_channel(w) == UNDEFINED_CHANNEL) {
    return 0;
  }
  DISPLAY_WORD(w);
  return 0;
}

//  ----------------------------------------------------------------------------

//  help__print_category : sert pour l'affichage de l'aide ; affiche une
//    catégorie d'aide.
static void help__print_category(const char *category) {
  printf("%s\n", category);
}

//  help_print_opt : sert pour l'affichage de l'aide ; affiche une option (opt)
//    suivie de sa description (describe).
static void help__print_opt(char opt, const char *describe) {
  printf("  -%c\t\t%s\n\n", opt, describe);
}

void print_help() {
  //  Usage
  printf("Usage: xwc [OPTION]... [FILE]...\n\n");
  //  Description
  printf(
      "Exclusive word counting. Print the number of occurrences of each word " \
      "that appears in one and only one of given text FILES.\n\n"
      );
  printf(
      "A word is, by default, a maximum length sequence of characters that "   \
      "do not belong to the white-space characters set.\n\n"
      );
  printf(
      "Results are displayed in columns on the standard output. Columns are "  \
      "separated by the tab character. Lines are terminated by the "           \
      "end-of-line character. A header line shows the FILE names: the name "   \
      "of the first FILE appears in the second column, that of the second in " \
      "the third, and so on. For the following lines, a word appears in the "  \
      "first column, its number of occurrences in the FILE in which it "       \
      "appears to the exclusion of all others in the column associated with "  \
      "the FILE. No tab characters are written on a line after the number of " \
      "occurrences.\n\n"
      );
  //  Options
  help__print_category("Program Information");
  help__print_opt(
      CHR(ARGS__HELP),
      "Print this help message and exit."
      );
  help__print_category("Input Control");
  help__print_opt(
      CHR(ARGS__LIMIT_WLEN),
      "Set the maximal number of significant initial letters for words to "    \
      "VALUE. 0 means without limitation. Default is 0."
      );
  help__print_opt(
      CHR(ARGS__ONLY_ALPHA_NUM),
      "Make the punctuation characters play the same role as white-space "     \
      "characters in the meaning of words."
      );
  help__print_opt(
      CHR(ARGS__RESTRICT),
      "Limit the counting to the set of words that appear in FILE. FILE is "   \
      "displayed in the first column the standard input; in this case, \"\" "  \
      "is displayed in first column of the header line."
      );
  help__print_category("Output Control");
  help__print_opt(
      CHR(ARGS__SORT_LEXICAL),
      "Same as -s lexicographical"
      );
  help__print_opt(
      CHR(ARGS__SORT_NUMERIC),
      "Same as -s numeric"
      );
  help__print_opt(
      CHR(ARGS__SORT_NONE),
      "Same as -s none"
      );
  help__print_opt(
      CHR(ARGS__SORT_TYPE),
      "Sort the results in ascending order, by default, according to TYPE. "   \
      "The available values for TYPE are: 'lexicographical', sort on words, "  \
      "'numeric', sort on number of occurrences, first key, and words, "       \
      "second key, and 'none', don't try to sort, take it as it comes. "       \
      "Default is 'none'."
      );
  help__print_opt(
      CHR(ARGS__SORT_REVERSE),
      "Sort in descending order on the single or first key instead of "        \
      "ascending order. This option has no effect if the -S option is enable."
      );
}

//  ----------------------------------------------------------------------------

wordstream *wordstream_new(const char *filename, const char *default_fn) {
  wordstream *w = malloc(sizeof *w);
  if (w == NULL) {
    return NULL;
  }
  // Défini s'il s'agit de l'entrée standard
  w->is_stdin = strcmp(filename, "-") == 0;
  // Vérifie que le nom de fichier ne commence pas par "-- "
  if (!w->is_stdin && filename[0] == '-' && filename[1] == '-'
      && filename[2] == ' ') {
    filename = filename + 3;
  }
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
    fprintf(stderr, "*** Erreur lors de la fermeture du fichier: %s\n",
        w->filename);
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
#define ARGS__OPT_STRING ":"                                                   \
  XSTR(ARGS__HELP)                                                             \
  XSTR(ARGS__RESTRICT) ":"                                                     \
  XSTR(ARGS__ONLY_ALPHA_NUM)                                                   \
  XSTR(ARGS__LIMIT_WLEN) ":"                                                   \
  XSTR(ARGS__SORT_REVERSE)                                                     \
  XSTR(ARGS__SORT_LEXICAL)                                                     \
  XSTR(ARGS__SORT_NUMERIC)                                                     \
  XSTR(ARGS__SORT_NONE)                                                        \
  XSTR(ARGS__SORT_TYPE) ":"

//  ARGS__SORT_COND : on suppose qu'il existe un entier opt qui est l'option en
//    cours de traitement. L'expression vaut true si cette option (et sa valeur
//    si besoin) correspond au tri SORT_TYPE.
#define ARGS__SORT_COND(SORT_TYPE)                                             \
  (                                                                            \
    opt == CHR(ARGS__SORT_ ## SORT_TYPE)                                       \
    || (                                                                       \
      opt == CHR(ARGS__SORT_TYPE)                                              \
      && strcmp(optarg, ARGS__SORT_TYPE_ ## SORT_TYPE) == 0                    \
      )                                                                        \
  )

//  ARGS__FN_BUFSIZE : taille du buffer utilisé pour les noms de fichier par
//    défaut (dans sprintf) ; Actuellement : "#n" Donc (1 + (longueur de n) + 1)
#define ARGS__FN_BUFSIZE (1 + 30 + 1)

//  args__set_filtered : défini l'option "filtré" de a à true, avec comme filtre
//    le fichier de nom filter_fn. Renvoie -1 en cas de dépassement de capacité,
//    sinon renvoie 0.
static int args__set_filtered(args *a, const char *filter_fn) {
  a->filter = wordstream_new(filter_fn, "restrict");
  if (a->filter == NULL) {
    return -1;
  }
  a->filtered = true;
  return 0;
}

//  args__get_size_t : tente de parser la chaine s vers un size_t. En cas de
//    succès, le nombre trouvé est affecté à *k et 0 est renvoyé. Sinon une
//    valeur non nulle est renvoyée et *k n'est pas modifié.
static int args__get_size_t(size_t *k, const char *s) {
  if (*s == '-') {
    return 1;
  }
  errno = 0;
  char *end = NULL;
  unsigned long long m = strtoull(s, &end, 10);
  if (errno != 0 || end == s || *end != '\0') {
    return -1;
  }
  *k = (size_t) m;
  return 0;
}

args *args_init(int argc, char *argv[], int *error) {
  if (error == NULL) {
    return NULL;
  }
  int alloc_ws = 0;
  *error = 0;
  // Création avec valeurs par défaut
  args *a = malloc(sizeof *a);
  if (a == NULL) {
    return NULL;
  }
  a->filecount = 0;
  a->file = NULL;
  a->filtered = false;
  a->filter = NULL;
  a->only_alpha_num = false;
  a->max_w_len = 0;
  a->sort_type = ARGS__SORT_VAL_NONE;
  a->sort_reversed = false;
  a->help = false;
  // Récupération des valeurs des arguments
  int opt;
  opterr = 0;
  optopt = 0;
  while ((opt = getopt(argc, argv, ARGS__OPT_STRING)) != -1) {
    if (opt == CHR(ARGS__HELP) && (CHR(ARGS__HELP) != '?' || optopt == 0)) {
      a->help = true;
      return a;
    } else if (opt == CHR(ARGS__RESTRICT)) {
      if (args__set_filtered(a, optarg) != 0) {
        goto ai__error_capacity;
      }
    } else if (opt == CHR(ARGS__ONLY_ALPHA_NUM)) {
      a->only_alpha_num = true;
    } else if (opt == CHR(ARGS__LIMIT_WLEN)) {
      if (args__get_size_t(&a->max_w_len, optarg) != 0) {
        fprintf(stderr, "*** Invalid argument: -%c %s\n", (char) opt, optarg);
        goto ai__error_arg;
      }
    } else if (opt == CHR(ARGS__SORT_REVERSE)) {
      a->sort_reversed = true;
    } else if (ARGS__SORT_COND(LEXICAL)) {
      a->sort_type = ARGS__SORT_VAL_LEXICAL;
    } else if (ARGS__SORT_COND(NUMERIC)) {
      a->sort_type = ARGS__SORT_VAL_NUMERIC;
    } else if (ARGS__SORT_COND(NONE)) {
      a->sort_type = ARGS__SORT_VAL_NONE;
    } else if (opt == CHR(ARGS__SORT_TYPE)) {
      // Le tri n'est pas reconnu
      fprintf(stderr, "*** Unrecognised expression: -%c %s\n", (char) opt,
          optarg);
      goto ai__error_arg;
    } else if (opt == ':') {
      fprintf(stderr, "*** Missing argument: %s\n", argv[optind - 1]);
      goto ai__error_arg;
    } else {
      fprintf(stderr, "*** Unknown argument: %s\n", argv[optind - 1]);
      goto ai__error_arg;
    }
    optopt = 0;
  }
  // Gestion des fichiers
  a->filecount = argc - optind;
  bool no_file = a->filecount == 0;
  if (no_file) {
    a->filecount = 1;
  }
  a->file = malloc((size_t) (a->filecount) * sizeof *a->file);
  if (a->file == NULL) {
    goto ai__error_capacity;
  }
  if (no_file) {
    a->file[0] = wordstream_stdin("#1");
    if (a->file[0] == NULL) {
      goto ai__error_capacity;
    }
  } else {
    char buff[ARGS__FN_BUFSIZE];
    for (; alloc_ws < a->filecount; ++alloc_ws) {
      sprintf(buff, "#%d", alloc_ws + 1);
      a->file[alloc_ws] = wordstream_new(argv[optind + alloc_ws], buff);
      if (a->file[alloc_ws] == NULL) {
        goto ai__error_capacity;
      }
    }
  }
  return a;
ai__error_arg:
  *error = -1;
ai__error_capacity:
  if (a->file != NULL) {
    for (int i = 0; i < alloc_ws; ++i) {
      wordstream_pdispose(&a->file[i]);
    }
    free(a->file);
  }
  wordstream_pdispose(&a->filter);
  free(a);
  return NULL;
}

void args_dispose(args **a) {
  if (*a == NULL) {
    return;
  }
  for (int i = 0; i < (*a)->filecount; ++i) {
    wordstream_pdispose(&(*a)->file[i]);
  }
  free((*a)->file);
  wordstream_pdispose(&(*a)->filter);
  free(*a);
  *a = NULL;
}
