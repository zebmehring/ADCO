#ifndef __LINKED_LIST_H__
#define __LINKED_LIST_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

typedef struct linked_list_node {
  char *v;
  struct linked_list_node *next;
} linked_list_node_t;

struct Linkedlist {
  int size;
  linked_list_node_t *head;
};

struct Linkedlist *linked_list_new ();

void linked_list_add (struct Linkedlist *ll, const char *v);

char *linked_list_peek (struct Linkedlist *ll);

char *linked_list_poll (struct Linkedlist *ll);

void linked_list_free (struct Linkedlist *ll);

void _linked_list_free (linked_list_node_t *n);

#ifdef __cplusplus
}
#endif

#endif
