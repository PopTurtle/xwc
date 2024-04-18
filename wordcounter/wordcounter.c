#include "wordcounter.h"

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

size_t str_hashfun(const char *s) {
  size_t h = 0;
  for (const unsigned char *p = (const unsigned char *) s; *p != '\0'; ++p) {
    h = 37 * h + *p;
  }
  return h;
}

// Fonctions auxiliaires pour word ---------------------------------------------

//
static word *word__from(const char *s, int channel) {
    char *t = malloc(strlen(s) + 1);
    if (t == NULL) {
        return NULL;
    }
    strcpy(t, s);
    word *w = malloc(sizeof *w);
    if (w == NULL) {
        free(t);
        return NULL;
    }
    w->wordstr = t;
    w->count = 1;
    w->channel = channel;
    return w;
}

//
static void word__dispose_content(word *w) {
    free(w->wordstr);
    free(w);
}

//
static int rword__dispose_content(word *w) {
    word__dispose_content(w);
    return 0;
}

//  Fonctions pour word --------------------------------------------------------

char *word_str(const word *w) {return w->wordstr;}
long unsigned int word_count(const word *w){return w->count;}
int word_channel(const word *w){return w->channel;}

//  Fonctions auxiliaires pour wordcounter -------------------------------------

//
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

// 
static void wc__sort(wordcounter *w, int (*compare)(const word **, const word **)) {
  holdall_sort(w->ha_word, (int (*)(const void *, const void *))compare);
}

//
static int word__compare_count(const word **w1ptr, const word **w2ptr) {
  return ((*w1ptr)->count > (*w2ptr)->count) - ((*w1ptr)->count < (*w2ptr)->count);
}

//
static int word__compare_count_reverse(const word **w1ptr, const word **w2ptr) {
  return ((*w1ptr)->count < (*w2ptr)->count) - ((*w1ptr)->count > (*w2ptr)->count);
}

//
static int word__compare_lexical(const word **w1ptr, const word **w2ptr) {
  return strcoll((*w1ptr)->wordstr, (*w2ptr)->wordstr);
}

//
static int word__compare_lexical_reverse(const word **w1ptr, const word **w2ptr) {
  return strcoll((*w2ptr)->wordstr, (*w1ptr)->wordstr);
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

#define WC__BUFSIZE_MIN 32
#define WC__BUFSIZE_MUL 2

// pour le moment : affiche chaque mot sur la sortie standard
// max_word_len = 0 : infini

// Renvoie :
// 0 = Tout est ok
// 1 = error capacity
// 2 = error stream = error read

// static int wc__file_word_apply_int(FILE *stream, int max_word_len) {
//   bool is_limit_len = max_word_len != 0;
//   char *s = malloc((is_limit_len ? 256 : 256) + 1);
//   size_t cur_w_len = 0;
//   int c;
//   char *t;
//   while ((c = fgetc(stream)) != EOF) {
//     ++cur_w_len;
//     if (isspace(c)) {
//       
//     }
//   }
// }

#include <ctype.h>
#include <stdint.h>

// Renvoie :
// 0 = Tout est ok
// 1 = error capacity
// 2 = error stream = error read

//static int print_every_file_word(FILE *stream) {
//  size_t cur_buff_size = WC__BUFSIZE_MIN;
//  char *buff = malloc(cur_buff_size + 1);
//  int c;
//  size_t cur_w_len = 0;
//  while ((c = fgetc(stream)) != EOF) {
//    if (isspace(c)) {
//      buff[cur_w_len] = '\0';
//      printf("Word: %s\n", buff);
//      cur_w_len = 0;
//      continue;
//    }
//    if (cur_w_len == cur_buff_size) {
//      if (cur_buff_size > SIZE_MAX / WC__BUFSIZE_MUL - 1) {
//        free(buff);
//        return 1;
//      }
//      char *nbuff = realloc(buff, cur_buff_size * WC__BUFSIZE_MUL + 1);
//      if (nbuff == NULL) {
//        free(buff);
//        return 1;
//      }
//      buff = nbuff;
//    }
//    buff[cur_w_len] = (char) c;
//    ++cur_w_len;
//  }
//  int r = ferror(stream) ? 2 : 0;
//  if (!ferror(stream)) {
//    buff[cur_w_len] = '\0';
//    printf("Word: %s\n", buff);
//  }
//  free(buff);
//  return r;
//}

#define WC__LINE_BUFSIZE 5 // min LINE_MAX value (_POSIX2_LINE_MAX) : 2024

/*
static int wc__file_word_apply(FILE *stream, wordcounter *w, int c_int, int (*fun)(wordcounter *, const char *, int)) {
  int r = 0;
  char *buff = malloc(WC__LINE_BUFSIZE + 1);
  if (buff == NULL) {
    return 1;
  }
  size_t cur_wbuff_size = WC__BUFSIZE_MIN;
  char *word = malloc(cur_wbuff_size + 1);
  if (word == NULL) {
    goto fwa_error_capacity;
  }
  size_t cur_w_len = 0;
  while (fgets(buff, WC__LINE_BUFSIZE + 1, stream) != NULL) {
    char *cw = buff;
    while (*cw != '\0') {
      if (cur_w_len == cur_wbuff_size) {
        if (cur_wbuff_size > SIZE_MAX / WC__BUFSIZE_MUL - 1) {
          goto fwa_error_capacity;
        }
        cur_wbuff_size *= WC__BUFSIZE_MUL;
        char *nword = realloc(word, cur_wbuff_size + 1);
        if (nword == NULL) {
          goto fwa_error_capacity;
        }
        word = nword;
      }
      if (isspace((int) *cw)) {
        word[cur_w_len] = '\0';
        fun(w, word, c_int);
        cur_w_len = 0;
        while (isspace((int) *(++cw))) {}
        continue;
      }
      word[cur_w_len] = *cw;
      ++cur_w_len;
      ++cw;
    }
  }
  if (cur_w_len > 0) {
    word[cur_w_len] = '\0';
    fun(w, word, c_int);
    cur_w_len = 0;
  }
  if (ferror(stream)) {
    free(buff);
    return 2;
  }

  goto fwa_dispose;

fwa_error_capacity:
  r = 1;
fwa_dispose:
  free(buff);
  free(word);
  return r;
}
// */


//*
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
    if (isspace(c) || (only_alpha_num && !isalnum(c))) {
      if (cur_w_len > 0) {
        buff[cur_w_len] = '\0';
        fun(w, buff, c_int);
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
    fun(w, buff, c_int);
  }
  free(buff);
  return r;
}
// */


int wc_filecount(wordcounter *w, FILE *stream, size_t max_w_len, bool only_alpha_num, int channel) {

    // --------------------
    //printf("PRINT FILE\n");
    //print_every_file_word(stream);
    //printf("END PRINT FILE\n");
    //// --------------------
//
    //return NULL;

    return wc__file_word_apply(stream, w, max_w_len, only_alpha_num, channel, wc_addcount);

    // char s[32];
    // while (fscanf(stream, "%" "31" "s", s) == 1) { // !!!!!!!!!!!!!!!!!!!!!!!!!
    //   if (wc_addcount(w, s, channel) != 0) {
    //     return 1;
    //   }
    // }
    // return 0;
}

int wc_file_add_filtered(wordcounter *w, FILE *stream) {
  if (!w->filtered) {
    return 0;
  }
  char s[32]; // !!!!!!!!!!!!!!!!!!!!!!!!!// !!!!!!!!!!!!!!!!!!!!!!!!!
  while (fscanf(stream, "%" "31" "s", s) == 1) {
    word *p = hashtable_search(w->counter, s);
    if (p == NULL) {
      p = wc__create_counter(w, s, UNDEFINED_CHANNEL);
      if (p == NULL) {
        return 1;
      }
      p->count = 0;
    }
  }
  return 0;
}

void wc_sort_count(wordcounter *w) {
  wc__sort(w, word__compare_count);
}

void wc_sort_lexical(wordcounter *w) {
  wc__sort(w, word__compare_lexical);
}

void wc_sort_count_reverse(wordcounter *w) {
  wc__sort(w, word__compare_count_reverse);
}

void wc_sort_lexical_reverse(wordcounter *w) {
  wc__sort(w, word__compare_lexical_reverse);
}

int wc_apply(wordcounter *w, int (*fun)(word *)) {
  return holdall_apply((void *) w->ha_word, (int (*)(void *))fun);
}
