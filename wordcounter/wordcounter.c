#include "wordcounter.h"

#include <ctype.h>
#include <stdint.h>
#include <string.h>
#include "hashtable.h"
#include "holdall.h"


//  Les directives ci-dessous assurent l'inégalité :
//    UNDEFINED_CHANNEL < MULTI_CHANNEL < START_CHANNEL
#if UNDEFINED_CHANNEL >= MULTI_CHANNEL || MULTI_CHANNEL >= START_CHANNEL
#error Invalid choice for channel constants.
#endif

//  Structures -----------------------------------------------------------------

struct wordcounter {
    hashtable *counter;
    holdall *ha_word;
    bool filtered;
};

struct word {
  char *wordstr;
  long unsigned int count;
  int channel;
};

//  Fonction de hashage pour la hashmap ----------------------------------------

size_t str_hashfun(const char *s) {
  size_t h = 0;
  for (const unsigned char *p = (const unsigned char *) s; *p != '\0'; ++p) {
    h = 37 * h + *p;
  }
  return h;
}

// Fonctions auxiliaires pour word ---------------------------------------------

//  word__from : tente d'allouer les ressources nécessaire à un nouveau compteur
//    dont le mot est s, le canal channel, et la valeur du compteur est 1.
//    Renvoie NULL en cas de dépassement de capacité, renvoie sinon le compteur
//    nouvellement créé.
static word *word__from(const char *s, int channel) {
    word *w = malloc(sizeof *w);
    if (w == NULL) {
        return NULL;
    }
    char *t = malloc(strlen(s) + 1);
    if (t == NULL) {
        free(w);
        return NULL;
    }
    strcpy(t, s);
    w->wordstr = t;
    w->count = 1;
    w->channel = channel;
    return w;
}

//  word__dispose_content : Libère les ressources associées au compteur w,
//    sans affecté w à NULL, qui pointe donc désormais sur une zone non
//    allouée.
static void word__dispose_content(word *w) {
    free(w->wordstr);
    free(w);
}

//  rword__dispose_content : similaire à word__dispose_content mais renvoie 0.
static int rword__dispose_content(word *w) {
    word__dispose_content(w);
    return 0;
}

//  Fonctions pour word --------------------------------------------------------

char *word_str(const word *w) {
  return w->wordstr;
}

long unsigned int word_count(const word *w) {
  return w->count;
}

int word_channel(const word *w) {
  return w->channel;
}

//  Fonctions auxiliaires pour wordcounter -------------------------------------

//  wc__create_counter : tente d'allouer les ressources nécessaire pour un
//    nouveau compteur, initialisé à 1 occurence du mot, qui sera ajouté dans w.
//    Il est supposé que w ne contient pas de compteur pour le mot s. Renvoie
//    NULL en cas de dépassement de capacité, sinon renvoie un pointeur vers
//    le nouveau compteur.
static word *wc__create_counter(wordcounter *w, const char *s, int channel) {
    word *p = word__from(s, channel);
    if (p == NULL) {
        return NULL;
    }
    if (holdall_put(w->ha_word, p) != 0
      || hashtable_add(w->counter, p->wordstr, p) == NULL) { // peut etre probleme
        word__dispose_content(p);
        return NULL;
    }
    return p;
}

//  wc_create_empty_counter : similaire à wc__create_counter, mais change la
//    valeur du nouveau compteur pour être 0. Renvoie une valeur non nulle en
//    cas de dépassement de capacité, sinon 0.
static int wc__create_empty_counter(wordcounter *w, const char *s, int channel) {
  word *p = wc__create_counter(w, s, channel);
  if (p == NULL) {
    return 1;
  }
  p->count = 0;
  return 0;
}

//  wc_sort : Tri le compteur de mot w, ce qui modifiera l'ordre d'appel des
//    fonctions avec wc_apply par exemple.
static void wc__sort(wordcounter *w, int (*compare)(const word **, const word **)) {
  holdall_sort(w->ha_word, (int (*)(const void *, const void *))compare);
}

//  Fonctions auxiliaires pour word --------------------------------------------

//  word__compare_count, word__compare_count_reverse : compare la valeur des
//    compteurs **w1ptr et **w2ptr.
static int word__compare_count(const word **w1ptr, const word **w2ptr) {
  return ((*w1ptr)->count > (*w2ptr)->count) - ((*w1ptr)->count < (*w2ptr)->count);
}

static int word__compare_count_reverse(const word **w1ptr, const word **w2ptr) {
  return ((*w1ptr)->count < (*w2ptr)->count) - ((*w1ptr)->count > (*w2ptr)->count);
}

//  word__compare_lexical, word__compare_lexical_reverse : compare les mots des
//    compteurs **w1ptr et **w2ptr à l'aide de strcoll.
static int word__compare_lexical(const word **w1ptr, const word **w2ptr) {
  return strcoll((*w1ptr)->wordstr, (*w2ptr)->wordstr);
}

static int word__compare_lexical_reverse(const word **w1ptr, const word **w2ptr) {
  return strcoll((*w2ptr)->wordstr, (*w1ptr)->wordstr);
}

//  WC__BUFSIZE_MIN : taille minimale du buffer de lecture dans un fichier s'il
//    n'a pas de taille maximale prédéfinie
#define WC__BUFSIZE_MIN 16

//  WC__BUFSIZE_MUL : multiplicateur de la taille du buffer de lecture dans
//    un fichier lorsque ce buffer est plein
#define WC__BUFSIZE_MUL 2

//  wc__file_word_apply : parcours le flux pointé par stream et appel
//    fun(w, WORD, c_int) pour tout les mots WORD lus dans le flux, tant que
//    l'appel à fun renvoie une valeur nulle. Si only_alpha_num est à true alors
//    les caractères de ponctuations sont considérés comme des espaces. Enfin
//    les mots sont coupés au caractère à l'indice max_w_len si max_w_len
//    n'est pas égal à 0.
//  Renvoie 0 en cas de succès, 1 en cas de dépassement de capacité, 2 en cas
//    d'erreur de lecture sur le flux stream, et 3 si l'appel à fun a renvoyé
//    une valeur différente de 0.
static int wc__file_word_apply(FILE *stream, wordcounter *w, size_t max_w_len, bool only_alpha_num, int c_int, int (*fun)(wordcounter *, const char *, int)) {
  size_t cur_buff_size = max_w_len == 0 ? WC__BUFSIZE_MIN : max_w_len;
  char *buff = malloc(cur_buff_size + 1);
  size_t cur_w_len = 0;
  int c;
  while ((c = fgetc(stream)) != EOF) {
    if (cur_w_len == cur_buff_size) {
      if (max_w_len == 0) {
        if (cur_buff_size > SIZE_MAX / WC__BUFSIZE_MUL - 1) {
          free(buff);
          return 1;
        }
        cur_buff_size *= WC__BUFSIZE_MUL;
        char *nbuff = realloc(buff, cur_buff_size + 1);
        if (nbuff == NULL) {
          free(buff);
          return 1;
        }
        buff = nbuff;
      } else {
        if (!isspace(c)) {
          continue;
        }
      }
    }
    if (isspace(c) || (only_alpha_num && ispunct(c))) {
      if (cur_w_len > 0) {
        buff[cur_w_len] = '\0';
        if (fun(w, buff, c_int) != 0) {
          return 3;
        }
        cur_w_len = 0;
      }
      continue;
    }
    buff[cur_w_len] = (char) c;
    ++cur_w_len;
  }
  int r = ferror(stream) ? 2 : 0;
  if (!ferror(stream) && cur_w_len > 0) {
    buff[cur_w_len] = '\0';
    if (fun(w, buff, c_int) != 0) {
      return 3;
    }
  }
  free(buff);
  return r;
}

// Fonctions pour wordcounter --------------------------------------------------

wordcounter *wc_empty(bool filtered) {
    wordcounter *w = malloc(sizeof *w);
    if (w == NULL) {
        return NULL;
    }
    w->counter = hashtable_empty((int (*)(const void *, const void *))strcmp,
      (size_t (*)(const void *))str_hashfun);
    if (w->counter == NULL) {
        free (w);
        return NULL;
    }
    w->ha_word = holdall_empty();
    if (w->ha_word == NULL) {
        free(w->counter);
        free(w);
        return NULL;
    }
    w->filtered = filtered;
    return w;
}

void wc_dispose(wordcounter **w) {
  if (*w == NULL) {
    return;
  }
    wc_apply(*w, rword__dispose_content);
    hashtable_dispose(&(*w)->counter);
    holdall_dispose(&(*w)->ha_word);
    free(*w);
    *w = NULL;
}

int wc_addcount(wordcounter *w, const char *s, int channel) {
    word *p = hashtable_search(w->counter, s);
    if (p != NULL) {
        ++p->count;
        if (p->channel != channel) {
            p->channel = p->channel == UNDEFINED_CHANNEL ? channel : MULTI_CHANNEL;
        }
        return 0;
    }
    if (w->filtered) {
      return 0;
    }
    return wc__create_counter(w, s, channel) == NULL ? 1 : 0;
}

int wc_filecount(wordcounter *w, FILE *stream, size_t max_w_len, bool only_alpha_num, int channel) {
    return wc__file_word_apply(stream, w, max_w_len, only_alpha_num, channel, wc_addcount);
}

int wc_file_add_filtered(wordcounter *w, FILE *stream, size_t max_w_len, bool only_alpha_num) {
  if (!w->filtered) {
    return 0;
  }
  return wc__file_word_apply(stream, w, max_w_len, only_alpha_num, UNDEFINED_CHANNEL, wc__create_empty_counter);
}

void wc_sort_lexical(wordcounter *w) {
  wc__sort(w, word__compare_lexical);
}

void wc_sort_count(wordcounter *w) {
  wc__sort(w, word__compare_count);
}

void wc_sort_lexical_reverse(wordcounter *w) {
  wc__sort(w, word__compare_lexical_reverse);
}

void wc_sort_count_reverse(wordcounter *w) {
  wc__sort(w, word__compare_count_reverse);
}

int wc_apply(wordcounter *w, int (*fun)(word *)) {
  return holdall_apply((void *) w->ha_word, (int (*)(void *))fun);
}
