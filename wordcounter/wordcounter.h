//  Partie interface du module wordcounter
//
//  Le module permet de compter le nombre d'occurences de mots qui apparaissent
//    dans un canal (channel) particulier ou plusieurs canaux. L'accès aux
//    compteurs via wc_apply n'est par défaut pas ordonné, il peut cependant
//    l'être si une fonction de tri (wc_sort_*) est appliqué avant wc_appply.

#ifndef WORDCOUNTER__H
#define WORDCOUNTER__H

//  Fonctionnement général :
//  - aux mots (word) sont associés un compteur et un canal, le compteur est
//    égal au nombre de fois que le mot a été compté via wc_addcount. Chaque
//    fois qu'un mot est rencontré dans un canal particulié, son canal courant
//    peut être modifier. Un canal est un entier.
//  - trois 'types' de canaux sont possible:
//    > canal indéfini: associé a un mot qui n'a jamais été rencontré, il est
//      représenté par la macro-constante UNDEFINED_CHANNEL
//    > canal simple: le mot n'a été rencontré que dans un seul canal. Sa valeur
//      est alors supérieur ou égal à START_CHANNEL
//    > canal multiple: le mot a été rencontré dans plusieurs canaux, représenté
//      par la macro-constante MULTI_CHANNEL
//  - il est assuré que UNDEFINED_CHANNEL < MULTI_CHANNEL < START_CHANNEL
//  - le comportement des fonctions du module est indéterminé si :
//    > lorqu'un paramètre de type « wordcounter * » ou « wordcounter ** »
//      est demandé et que la valeur du wordcounter associé n'est pas un
//      contrôleur préalablement renvoyé par wc_empty ou qu'il a été révoqué
//      via wc_dispose
//    > lorsqu'un canal est demandé la valeur donnée ne correspond pas aux
//      valeurs possibles données plus haut
//  - si un wordcounter est "filtré" alors seul les mots qui ont été ajoutés
//    au filtre peuvent être comptés.

// -----------------------------------------------------------------------------

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

//  Les macro-constantes ci-dessous représentant les valeurs que peuvent
//    prendre un canal, sachant que la valeur d'un canal peut être supérieur
//    à START_CHANNEL.
//  Il est assuré que UNDEFINED_CHANNEL < MULTI_CHANNEL < START_CHANNEL

#define UNDEFINED_CHANNEL -1
#define MULTI_CHANNEL (UNDEFINED_CHANNEL + 1)
#define START_CHANNEL (MULTI_CHANNEL + 1)

//  Structures -----------------------------------------------------------------

//  struct word, word : compteur, auquel est associé un mot, une valeur
//    de compteur, et un canal
typedef struct word word;

//  struct wordcounter wordcounter : compteur de mots, stock des compteurs
//    (word) qui peuvent être modifiés via l'appel à ses fonction associées,
//    comme par exemple wc_addcount
typedef struct wordcounter wordcounter;

//  Fonctions pour word --------------------------------------------------------

//  word_str, word_count, word_channel : renvoie respectivement la chaine de
//    caractère, la valeur du compteur, le canal associé au mot pointé par w.
//    Si le mot est présent dans un contrôleur c de type wordcounter, et que la
//    chaine qui lui est associé est modifié en dehors du module après sa
//    récupération via word_str, alors le comportement de c devient indéterminé
extern char *word_str(const word *w);
extern long unsigned int word_count(const word *w);
extern int word_channel(const word *w);

//  Fonctions pour wordcounter -------------------------------------------------

//  wc_empty : tente d'allouer les ressources nécessaire pour gérer un nouveau
//    compteur de mots. Renvoie NULL en cas de dépassement de capacité. Renvoie
//    sinon un pointeur vers le controleur associé au nouveau compteur de mots.
extern wordcounter *wc_empty(bool filtered);

//  wc_dispose : sans effet si *w vaut NULL. Libère sinon les ressources
//    allouées pour la gestion du compteur de mots, puis affecte NULL à *w.
extern void wc_dispose(wordcounter **w);

//  wc_addcount : incrémente le compteur associé au mot égal à la chaine pointé
//    par s. Le canal associé au mot est mis à jour si nécessaire : s'il est
//    indéfini, il prend la valeur de channel; s'il est égal à channel ou qu'il
//    est multiple, rien ne se passe; enfin s'il est différent de channel, il
//    devient multiple. Si le compteur n'existe pas, il est créé et initialisé
//    à 1. Renvoie 1 en cas de dépassement de capacité, sinon renvoie 0.
extern int wc_addcount(wordcounter *w, const char *s, int channel);

//  pour wc_filecount, wc_file_add_filtered: les lus mots sont coupés à l'indice
//    max_w_len s'il ne vaut pas 0. Si only_alpha_num vaut true, les caractères
//    de ponctuations sont considérés comme des espaces. Renvoie 0 en cas de
//    succès, 1 ou 3 en cas de dépassement de capacité et 2 en cas d'erreur de
//    lecture sur le flux stream.

//  wc_filecount : applique wc_addcount(w, S, channel) à tous les mots S lus
//    depuis le flux pointé par stream. 
extern int wc_filecount(wordcounter *w, FILE *stream, size_t max_w_len, bool only_alpha_num, int channel);

//  wc_file_add_filtered : sans effet si w n'est pas filtré. Sinon ajoute les
//    mots lus dans le flux stream au filtre de w.
extern int wc_file_add_filtered(wordcounter *w, FILE *stream, size_t max_w_len, bool only_alpha_num);

//  wc_sort_lexical, wc_sort_count : tri l'ordre des mots en fonction de leur
//    ordre alphabétique (lexcical) ou de la valeur de leur compteur (count).
extern void wc_sort_lexical(wordcounter *w);
extern void wc_sort_count(wordcounter *w);

//  wc_sort_lexical_reverse, wc_sort_count_reverse : similaires à
//    wc_sort_lexical et wc_sort_count, mais tri dans l'ordre inverse.
extern void wc_sort_lexical_reverse(wordcounter *w);
extern void wc_sort_count_reverse(wordcounter *w);

//  wc_apply : applique fun a tous les mots présents dans le compteur de mots
//    associé à *w. L'ordre d'appel est par défaut indéfini, mais peut l'être
//    si un tri a été effectué avant l'appel à wc_apply. Les appels sont
//    interrompus avant la fin si un appel à fun renvoie une valeur différente
//    de 0.
extern int wc_apply(wordcounter *w, int (*fun)(word *));

// -----------------------------------------------------------------------------

#endif
