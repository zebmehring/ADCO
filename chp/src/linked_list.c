struct Linkedlist *linked_list_new ()
{
  struct Linkedlist *ll = calloc (1, sizeof(struct Linkedlist));
  ll->size = 0;
  ll->head = NULL;
  ll->tail = NULL;
  return ll;
}

void linked_list_add (struct Linkedlist *ll, const char *v)
{
  if (ll->size < 1)
  {
    ll->head = calloc (1, sizeof(linked_list_node_t));
    ll->head->v = calloc (strlen (v) + 1, sizeof(char));
    strcpy (ll->v, v);
    ll->head->next = NULL;
    ll->tail = ll->head;
  }
  else
  {
    linked_list_node_t *probe;
    for (probe = ll->head; probe->next; probe = probe->next);
    probe->next = calloc (1, sizeof(linked_list_t));
    probe->next->v = calloc (strlen (v) + 1, sizeof(char));
    strcpy (probe->next->v, v);
    probe->next->next = NULL;
    ll->tail = probe->next;
  }
  ll->size++;
}

char *linked_list_peek (struct Linkedlist *ll)
{
  return ll->size > 0 ? ll->head->v : NULL;
}

char *linked_list_poll (struct Linkedlist *ll)
{
  if (ll->size < 1) return NULL;
  char *ret = calloc (strlen (ll->head->v) + 1, sizeof(char));
  strcpy (ret, ll->head->v);
  free (ll->head->v);
  if (ll->head == ll->tail) ll->tail = ll->head->next;
  linked_list_t *temp = ll->head;
  ll->head = ll->head->next;
  free (temp);
  ll->size--;
  return ret;
}

void linked_list_free (struct Linkedlist *ll)
{
  _linked_list_free (ll->head);
  free (ll);
}

void _linked_list_free (linked_list_node_t *n)
{
  if (!n) return;
  free (n->next);
  free (n->v);
  free (n);
}
