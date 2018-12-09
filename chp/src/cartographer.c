#include "cartographer.h"

int expr_count = 1;
int stmt_count = 0;
int chan_count = 0;
int gc_chan_count = 0;

Chp *__chp;

bool bundle_data = false;
char *output_file = NULL;
FILE *output_stream;
int optimization = 0;
struct Hashtable *evaluated_exprs;

#define ACT_PATH "/home/user/Documents/ADCO/act"

#define max(a,b) (((a) > (b)) ? (a) : (b))

#define INITIAL_HASH_SIZE 10

#define MAX_EXPR_SIZE 1024
#define MAX_KEY_SIZE 2048
#define MAX_PATH_SIZE 4096

/*
 * void print_vars (Chp *c)
 *
 * Summary:
 *
 *    Declares and initializes all varialbes and channels in the CHP program
 *    described by c. All variables are initialized with syn_var_init_false.
 *    Returns immediately if c contains no symbols or is otherwise invalid.
 *
 *    All lines of ACT are printed to the location specified by output_stream.
 *
 * Parameters: c - a data structure containing the read-in CHP program
 */
void print_vars (Chp *c)
{
  hash_bucket_t *b;
  int i;
  symbol *sym;

  if (!c || !c->sym) return;
  if (c->sym->n == 0) return;

  fprintf (output_stream, "  /* --- declaring all variables and channels --- */\n");

  // iterate through the symbol hash table
  for (i = 0; i < c->sym->size; i++)
  {
    for (b = c->sym->head[i]; b; b = b->next)
    {
      sym = (symbol *) b->v;
      // declare channels
      if (sym->ischan)
      {
      	if (sym->bitwidth > 1)
        {
      	  fprintf (output_stream, "  aN1of2<%d> chan_%s;\n", sym->bitwidth, sym->name);
      	}
      	else
        {
      	  fprintf (output_stream, "  a1of2 chan_%s;\n", sym->name);
      	}
      }
      // declare variables, initialized to false
      else
      {
      	if (sym->bitwidth > 1)
        {
      	  fprintf (output_stream, "  syn_var_init_false var_%s[%d];\n", sym->name, sym->bitwidth);
      	}
      	else
        {
      	  fprintf (output_stream, "  syn_var_init_false var_%s;\n", sym->name);
      	}
      }
    }
  }
  fprintf (output_stream, "  /* --- end of declarations --- */\n\n");
}

/*
 * void emit_const_1 (void)
 *
 * Summary:
 *
 *    Declares a static node connected to Vdd. If such a node has already been
 *    created by a previous call, no action is taken.
 *
 *    All lines of ACT are printed to the location specified by output_stream.
 */
void emit_const_1 (void)
{
  static int emitted = 0;
  if (!emitted)
  {
    fprintf (output_stream, "  syn_var_init_true const_1;\n");
  }
  emitted = 1;
}

/*
 * void emit_const_0 (void)
 *
 * Summary:
 *
 *    Declares a static node connected to GND. If such a node has already been
 *    created by a previous call, no action is taken.
 *
 *    All lines of ACT are printed to the location specified by output_stream.
 */
void emit_const_0 (void)
{
  static int emitted = 0;
  if (!emitted)
  {
    fprintf (output_stream, "  syn_var_init_false const_0;\n");
  }
  emitted = 1;
}

/*
 * int get_bitwidth (int n)
 *
 * Summary:
 *
 *    Returns the bitwidth of the integer n.
 *
 */
int get_bitwidth (int n)
{
  int width = n == 0 ? 1 : 0;
  while (n > 0)
  {
    n >>= 1;
    width++;
  }
  return width;
}

/*
 * int get_func_bitwidth (char *s)
 *
 * Summary:
 *
 *    Returns the bitwidth of the function call represented by s.
 *
 * Parameters: s - a string containing the parsed CHP function call, of the form
 *                 <name>_<bitwidth>
 *
 * Return Value: the bitwidth of the function as specified in s
 */
int get_func_bitwidth (char *s)
{
  while (isdigit (*s)) s--;
  return atoi (s+1);
}

/*
 * int get_max_bits (const char *s, int lbits, int rbits)
 *
 * Summary:
 *
 *    Gets maximum the bitwidth of the result of the operation specified by s on
 *    operands of bitwidths lbits and rbits, respectively. Acceptable operations
 *    are: "add", "sub", "mul", and "div".
 *
 * Parameters: s - a string specifying the arithmetic operation
 *             lbits - the number of bits in the left operand
 *             rbits - the number of bits in the right operand
 *
 * Return Value: the maximum bitwidth result of n(<lbits>) <s> m(<rbits>)
 */
int get_max_bits (const char *s, int lbits, int rbits)
{
  if (!strcmp (s, "add") || !strcmp (s, "sub"))
  {
    return (lbits > rbits) ? lbits + 1 : rbits + 1;
  }
  else if (!strcmp (s, "mul"))
  {
    return lbits + rbits;
  }
  else if (!strcmp (s, "div"))
  {
    return lbits > rbits ? lbits : rbits;
  }
  else
  {
    fprintf (stderr, "Error: unsupported binary operation\n");
    return -1;
  }
}

int get_bundle_delay (int n, int type)
{
  switch (type)
  {
    case E_AND:
    case E_OR:
    case E_XOR:
    case E_NOT:
    case E_COMPLEMENT:
      return 2*n;
    case E_PLUS:
      return 2*n;
    case E_MINUS:
      return 2*(2*n);
    case E_MULT:
      return 2*(n*n);
    case E_UMINUS:
      return 2*(2*n);
    case E_LT:
    case E_GE:
      return 2*(2*n);
    case E_LE:
    case E_GT:
      return 2*(3*n);
    case E_NE:
    case E_EQ:
      return 2*(2*n);
    case E_VAR:
    case E_INT:
      return 0;
    default:
      fprintf (stderr, "Error: unsupported bundled operation: %d\n", type);
      return -1;
  }
}

/*
 * void get_expr (Expr *e, int v, char *buf)
 *
 * Summary:
 *
 *    Prints to buf a string representing the ACT module for e. If e is a
 *    primitive variable, the string is of the form "<var_name>", if it is
 *    a primitive integer, the string is of the form "<decimal_value>".
 *    Otherwise, the string is of the form "e_<expr_count>" where expr_count
 *    is the integer specified for the module in the ACT file.
 *
 * Parameters: e - an Expr object containing the expression
 *             v - integer value of expr_count for e
 *             buf - buffer in which to store the result
 */
void get_expr (Expr *e, int v, char *buf)
{
  symbol *s;
  switch (e->type)
  {
    case E_AND:
    case E_OR:
    case E_XOR:
    case E_NOT:
    case E_COMPLEMENT:
    case E_PLUS:
    case E_MINUS:
    case E_MULT:
    case E_EQ:
    case E_LT:
    case E_GT:
    case E_LE:
    case E_GE:
    case E_NE:
      snprintf (buf, MAX_EXPR_SIZE, "e_%d", v);
      break;
    case E_VAR:
      s = find_symbol (__chp, (char *)e->u.e.l);
      snprintf (buf, MAX_EXPR_SIZE, "%s", s->name);
      break;
    case E_INT:
      snprintf (buf, MAX_EXPR_SIZE, "%d", e->u.v);
      break;
    default:
      return;
  }
}

/*
 * void hash_add_expr (struct Hashtable *h, const char *expr)
 *
 * Summary:
 *
 *    Inserts the pair (expr, e_<expr>) into the table of evaluated expressions.
 *    The value associated with the key is e_<expr_count>, which corresponds to
 *    the module instantiated in the ACT file.
 *
 * Parameters: h - the hash table of evaluated expressions
 *             expr - a string representing the stored operation, of the form:
 *                    <op>(<operand1>[,<operand2>])
 */
void hash_add_expr (struct Hashtable *h, const char *expr)
{
  hash_bucket_t *b = hash_add (h, expr);
  b->v = calloc (MAX_EXPR_SIZE, sizeof(char));
  snprintf (b->v, MAX_EXPR_SIZE, "e_%d", expr_count);
}

/*
 * void hash_remove_expr (struct Hashtable *h, const char *expr)
 *
 * Summary:
 *
 *    Removes any instance of expr or its dependents from the hash table.
 *    Iterates through all entries in the table and recursively combs through
 *    each list of buckets.
 *
 * Parameters: h - the hash table of evaluated expressions
 *             expr - a string representing the stored operation, of the form:
 *                    <op>(<operand1>[,<operand2>])
 */
void hash_remove_expr (struct Hashtable *h, const char *expr)
{
  for (int i = 0; i < h->size; i++) _hash_remove_expr (h, expr, h->head[i]);
}

/*
 * void _hash_remove_expr (struct Hashtable *h, const char *expr, hash_bucket_t *b)
 *
 * Summary:
 *
 *    Removes any instance of expr or its dependents from the hash table.
 *    Recursively inspects an individual index in the table, removing a bucket
 *    from the chain if expr is a constituent. Also recursively calls
 *    hash_remove_expr() on the value of any removed bucket.
 *
 * Parameters: h - the hash table of evaluated expressions
 *             expr - a string representing the stored operation, of the form:
 *                    <op>(<operand1>[,<operand2>])
 */
void _hash_remove_expr (struct Hashtable *h, const char *expr, hash_bucket_t *b)
{
  if (!b) return;
  // recursively remove the rest of the entries in the chain
  _hash_remove_expr (h, expr, b->next);
  if (strstr (b->key, expr))
  {
    // recursively remove all dependent entries
    hash_remove_expr (h, (char *) b->v);
    free (b->v);
    hash_delete (h, b->key);
  }
}

/*
 * int hash_get_or_add (struct Hashtable *h, const char* s, Expr *l, Expr *r, int nl, int nr, bool commutative)
 *
 * Summary:
 *
 *    Looks up the entry <s>(<l>[,<r>]) in the hash table h. Returns the integer
 *    associated with the key's value, if found, or -1 if not. If the entry is
 *    not in the table, it (and its symmetric counterpart, if the operation is
 *    commutative) is added to the table.
 *
 * Parameters: h - the hash table of evaluated expressions
 *             s - a string representing the operation
 *             l - an Expr object containing the LHS of the expression
 *             r - an Expr object containing the RHS of the expression
 *             nl - the (previous) value of expr_count for the left expression
 *             nr - the (previous) value of expr_count for the right expression
 *             commutative - boolean representing the commutativity of s
 *
 * Return Value: the value of expr_count when the expression was evaluated, or
 *               -1 if the expression is not in the table
 */
int hash_get_or_add (struct Hashtable *h, const char* s, Expr *l, Expr *r, int nl, int nr, bool commutative)
{
  hash_bucket_t *b;
  char *left_expr, *right_expr, *k, *_k;
  int ret;

  // get the strings "e_<expr_count> | <var_name>" for the expression(s)
  left_expr = calloc (MAX_EXPR_SIZE, sizeof(char));
  if (r) right_expr = calloc (MAX_EXPR_SIZE, sizeof(char));
  get_expr (l, nl, left_expr);
  if (r) get_expr (r, nr, right_expr);

  // form the key string from the operation and operand strings
  k = calloc (MAX_KEY_SIZE, sizeof(char));
  if (commutative) _k = calloc (MAX_KEY_SIZE, sizeof(char));
  if (r)
  {
    snprintf (k, MAX_KEY_SIZE, "%s(%s,%s)", s, left_expr, right_expr);
    if (commutative) snprintf (_k, MAX_KEY_SIZE, "%s(%s,%s)", s, right_expr, left_expr);
  }
  else
  {
    snprintf (k, MAX_KEY_SIZE, "%s(%s)", s, left_expr);
  }

  free (left_expr);
  if (r) free (right_expr);

  // if an entry is in the table, return the integer associated with the value
  if ((b = hash_lookup (h, k)))
  {
    char *t = (char *) b->v + strlen ((char *) b->v) - 1;
    while (isdigit (*t)) t--;
    ret = atoi (++t);
  }
  // if the key isn't in the table, add it (and its commutative counterpart)
  else
  {
    hash_add_expr (h, k);
    if (commutative) hash_add_expr (h, _k);
    ret = -1;
  }

  free (k);
  if (commutative) free (_k);
  return ret;
}

/*
 * int unop (const char *s, Expr *e, int *bitwidth, int *base_var)
 *
 * Summary:
 *
 *    Prints the ACT for a unary operation represented by e.
 *
 *    All lines of ACT are printed to the location specified by output_stream.
 *
 * Parameters: s - a string representing the operation
 *             e - an Expr object containing the expression
 *             bitwidth - reference to an integer containing the bitwidth of the
 *                        current expression, as defined by its variables
 *             base_var - reference to the expr_count value to connect the go
 *                        signal to
 *
 * Return Value: the value of expr_count for the expression
 */
int unop (const char *s, Expr *e, int *bitwidth, int *base_var, int *delay)
{
  int l = _print_expr (e->u.e.l, bitwidth, base_var, delay);

  // print ACT module for boolean unary operations
  if (*bitwidth == 1)
  {
    if (optimization > 0)
    {
      int ret = hash_get_or_add (evaluated_exprs, s, e->u.e.l, NULL, l, -1, false);
      if (ret > 0) return ret;
    }
    fprintf (output_stream, "  syn_expr_%s e_%d(e_%d.out);\n", s, expr_count, l);
  }
  // print ACT module for multi-bit unary operations with the bundle data protocol
  else if (bundle_data)
  {
    *delay += get_bundle_delay (*bitwidth, e->u.e.l->type);
    fprintf (output_stream, "  bundled_%s<%d> be_%d;\n", s, *bitwidth, expr_count);
    fprintf (output_stream, "  (i:%d: be_%d.in[i] = be_%d.out[i];)\n", *bitwidth, expr_count, l);
  }
  // print delay-insensitive ACT module for multi-bit unary operations
  else
  {
    if (optimization > 0)
    {
      int ret = hash_get_or_add (evaluated_exprs, s, e->u.e.l, NULL, l, -1, false);
      if (ret > 0) return ret;
    }
    fprintf (output_stream, "  syn_%s<%d> e_%d;\n", s, *bitwidth, expr_count);
    fprintf (output_stream, "  (i:%d: e_%d.in[i] = e_%d.out[i];)\n", *bitwidth, expr_count, l);
  }

  return expr_count++;
}

/*
 * int binop (const char *s, Expr *e, int *bitwidth, int *base_var, bool commutative)
 *
 * Summary:
 *
 *    Prints the ACT for a binary operation represented by e.
 *
 *    All lines of ACT are printed to the location specified by output_stream.
 *
 * Parameters: s - a string representing the operation
 *             e - an Expr object containing the expression
 *             bitwidth - reference to an integer containing the bitwidth of the
 *                        current expression, as defined by its variables
 *             base_var - reference to the expr_count value to connect the go
 *                        signal to
 *             commutative - boolean representing the commutativity of e
 *
 * Return Value: the value of expr_count for the expression
 */
int binop (const char *s, Expr *e, int *bitwidth, int *base_var, int *delay, bool commutative)
{
  int l = _print_expr (e->u.e.l, bitwidth, base_var, delay);
  int r = _print_expr (e->u.e.r, bitwidth, base_var, delay);

  // print ACT module for boolean binary operations
  if (*bitwidth == 1)
  {
    if (optimization > 0)
    {
      int ret = hash_get_or_add (evaluated_exprs, s, e->u.e.l, e->u.e.r, l, r, commutative);
      if (ret > 0) return ret;
    }
    fprintf (output_stream, "  syn_expr_%s e_%d(e_%d.out, e_%d.out);\n", s, expr_count, l, r);
  }
  // print ACT module for multi-bit binary operations with the bundle data protocol
  else if (bundle_data)
  {
    int dl = get_bundle_delay (*bitwidth, e->u.e.l->type);
    int dr = get_bundle_delay (*bitwidth, e->u.e.r->type);
    *delay += max (dl, dr);
    fprintf (output_stream, "  bundled_%s<%d> be_%d;\n", s, *bitwidth, expr_count);
    fprintf (output_stream, "  (i:%d: be_%d.in1[i] = be_%d.out[i];)\n", *bitwidth, expr_count, l);
    fprintf (output_stream, "  (i:%d: be_%d.in2[i] = be_%d.out[i];)\n", *bitwidth, expr_count, r);
  }
  // print delay-insensitive ACT module for multi-bit binary operations
  else
  {
    if (optimization > 0)
    {
      int ret = hash_get_or_add (evaluated_exprs, s, e->u.e.l, e->u.e.r, l, r, commutative);
      if (ret > 0) return ret;
    }
    fprintf (output_stream, "  syn_%s<%d> e_%d;\n", s, *bitwidth, expr_count);
    fprintf (output_stream, "  (i:%d: e_%d.in1[i] = e_%d.out[i];)\n", *bitwidth, expr_count, l);
    fprintf (output_stream, "  (i:%d: e_%d.in2[i] = e_%d.out[i];)\n", *bitwidth, expr_count, r);
  }

  return expr_count++;
}

/*
 * int _print_expr (Expr *e, int *bitwidth, int *base_var)
 *
 * Summary:
 *
 *    Prints the ACT for an expression tree rooted at e.
 *
 *    All lines of ACT are printed to the location specified by output_stream.
 *
 * Parameters: e - an Expr object containing the expression
 *             bitwidth - reference to an integer containing the bitwidth of the
 *                        current expression, as defined by its variables
 *             base_var - reference to the expr_count value to connect the go
 *                        signal to
 *
 * Return Value: the value of expr_count for the root expression
 */
int _print_expr (Expr *e, int *bitwidth, int *base_var, int *delay)
{
  int ret;
  switch (e->type)
  {
    case E_AND:
      ret = binop ("and", e, bitwidth, base_var, delay, true);
      break;
    case E_OR:
      ret = binop ("or", e, bitwidth, base_var, delay, true);
      break;
    case E_XOR:
      ret = binop ("xor", e, bitwidth, base_var, delay, true);
      break;
    case E_PLUS:
      ret = binop ("add", e, bitwidth, base_var, delay, true);
      break;
    case E_MINUS:
      ret = binop ("sub", e, bitwidth, base_var, delay, false);
      break;
    case E_MULT:
      ret = binop ("mul", e, bitwidth, base_var, delay, true);
      break;
    case E_NOT:
    case E_COMPLEMENT:
      ret = unop ("not", e, bitwidth, base_var, delay);
      break;
    case E_UMINUS:
      ret = unop ("uminus", e, bitwidth, base_var, delay);
      break;
    case E_EQ:
      ret = binop ("eq", e, bitwidth, base_var, delay, true);
      break;
    case E_NE:
      ret = binop ("ne", e, bitwidth, base_var, delay, true);
      break;
    case E_LT:
      ret = binop ("lt", e, bitwidth, base_var, delay, false);
      break;
    case E_GT:
      ret = binop ("gt", e, bitwidth, base_var, delay, false);
      break;
    case E_LE:
      ret = binop ("le", e, bitwidth, base_var, delay, false);
      break;
    case E_GE:
      ret = binop ("ge", e, bitwidth, base_var, delay, false);
      break;
    case E_VAR:
      {
        symbol *v = find_symbol (__chp, (char *)e->u.e.l);
        // if (optimization > 0)
        // {
        //   char *k = calloc (100, sizeof(char));
        //   sprintf (k, "var(%s)", v->name);
        //   hash_bucket_t *h;
        //   if ((h = hash_lookup (evaluated_exprs, k)))
        //   {
        //     free (k);
        //     // TODO: fix this
        //     if (*base_var == -1)
        //     {
      	//        *base_var = *((int *) h->v);
        //     }
        //     return *((int *) h->v);
        //   }
        //   else
        //   {
        //     h = hash_add (evaluated_exprs, k);
        //     int *count = calloc (1, sizeof(int));
        //     *count = expr_count;
        //     h->v = count;
        //   }
        // }
        if (v->bitwidth == 1)
        {
          fprintf (output_stream, "  syn_expr_var e_%d(,var_%s.v);\n", expr_count, (char *)e->u.e.l);
          if (*base_var == -1)
          {
    	       *base_var = expr_count;
          }
          else
          {
    	       fprintf (output_stream, "  e_%d.go_r = e_%d.go.r;\n", expr_count, *base_var);
          }
        }
        else if (bundle_data)
        {
          fprintf (output_stream, "  bundled_expr_vararray<%d> be_%d;\n", v->bitwidth, expr_count);
        	fprintf (output_stream, "  (i:%d: be_%d.v[i] = var_%s[i].v.t;)\n", v->bitwidth, expr_count, v->name);
        }
        else
        {
        	fprintf (output_stream, "  syn_expr_vararray<%d> e_%d;\n", v->bitwidth, expr_count);
        	fprintf (output_stream, "  (i:%d: e_%d.v[i] = var_%s[i].v;)\n", v->bitwidth, expr_count, v->name);
          // the current expression becomes the base go signal, if none is set
          if (*base_var == -1)
          {
    	       *base_var = expr_count;
          }
          // otherwise, the current expression is connected to the base go signal
          else
          {
    	       fprintf (output_stream, "  e_%d.go_r = e_%d.go_r;\n", expr_count, *base_var);
          }
        }
  	    *bitwidth = v->bitwidth;
        ret = expr_count++;
      }
      break;
    case E_INT:
      // if (optimization > 0)
      // {
      //   char *k = calloc (100, sizeof(char));
      //   sprintf (k, "int(%d)", e->u.v);
      //   hash_bucket_t *h;
      //   if ((h = hash_lookup (evaluated_exprs, k)))
      //   {
      //     free (k);
      //     if (*base_var == -1)
      //     {
      //        *base_var = *((int *) h->v);
      //     }
      //     return *((int *) h->v);
      //   }
      //   else
      //   {
      //     h = hash_add (evaluated_exprs, k);
      //     int *count = calloc (1, sizeof(int));
      //     *count = expr_count;
      //     h->v = count;
      //   }
      // }
      if (*bitwidth == 1)
      {
        if (e->u.v == 0)
        {
          emit_const_0 ();
          fprintf (output_stream, "  syn_expr_var e_%d(,const_0.v);\n", expr_count);
        }
        else
        {
          emit_const_1 ();
          fprintf (output_stream, "  syn_expr_var e_%d(,const_1.v);\n", expr_count);
        }
        if (*base_var == -1)
        {
  	       *base_var = expr_count;
        }
        else
        {
  	       fprintf (output_stream, "  e_%d.go_r = e_%d.go.r;\n", expr_count, *base_var);
        }
      }
      else if (bundle_data)
      {
        emit_const_0 ();
        fprintf (output_stream, "  bundled_expr_vararray<%d> be_%d;\n", *bitwidth, expr_count);
        int t = e->u.v;
        for (int i = *bitwidth; i > 0; i--, t >>= 1)
        {
          if (t & 1)
          {
            fprintf (output_stream, "  be_%d.v[%d] = const_0.v.f;\n", expr_count, *bitwidth - i);
          }
          else
          {
            fprintf (output_stream, "  be_%d.v[%d] = const_0.v.t;\n", expr_count, *bitwidth - i);
          }
        }
      }
      else
      {
        emit_const_0 ();
        emit_const_1 ();
        fprintf (output_stream, "  syn_expr_vararray<%d> e_%d;\n", *bitwidth, expr_count);
        int t = e->u.v;
        for (int i = *bitwidth; i > 0; i--, t >>= 1)
        {
          if (t & 1)
          {
            fprintf (output_stream, "  e_%d.v[%d] = const_1.v;\n", expr_count, *bitwidth - i);
          }
          else
          {
            fprintf (output_stream, "  e_%d.v[%d] = const_0.v;\n", expr_count, *bitwidth - i);
          }
        }
        // the current expression becomes the base go signal, if none is set
        if (*base_var == -1)
        {
          *base_var = expr_count;
        }
        // otherwise, the current expression is connected to the base go signal
        else
        {
          fprintf (output_stream, "  e_%d.go_r = e_%d.go_r;\n", expr_count, *base_var);
        }
      }
      ret = expr_count++;
      break;
    case E_TRUE:
      emit_const_1 ();
      fprintf (output_stream, "  syn_expr_var e_%d(,const_1.v);\n", expr_count);
      // the current expression becomes the base go signal, if none is set
      if (*base_var == -1)
      {
        *base_var = expr_count;
      }
      // the current expression becomes the base go signal, if none is set
      else
      {
        fprintf (output_stream, "  e_%d.go_r = e_%d.go_r;\n", expr_count, *base_var);
      }
      ret = expr_count++;
      break;
    case E_FALSE:
      emit_const_0 ();
      fprintf (output_stream, "  syn_expr_var e_%d(,const_0.v);\n", expr_count);
      // the current expression becomes the base go signal, if none is set
      if (*base_var == -1)
      {
        *base_var = expr_count;
      }
      // the current expression becomes the base go signal, if none is set
      else
      {
        fprintf (output_stream, "  e_%d.go_r = e_%d.go_r;\n", expr_count, *base_var);
      }
      ret = expr_count++;
      break;
    case E_PROBE:
      {
        symbol *s = find_symbol (__chp, (char *) e->u.e.l);
      	if (s->bitwidth == 1)
        {
      	  fprintf (output_stream, "  syn_a1of2_probe e_%d(,chan_%s.t, chan_%s.f, chan_%s.a);\n", expr_count, s->name, s->name, s->name);
      	}
        else
        {
      	  fprintf (output_stream, "  syn_aN1of2_probe<%d> e_%d(,chan_%s);\n", s->bitwidth, expr_count, s->name);
      	}
        *bitwidth = s->bitwidth;
        // the current expression becomes the base go signal, if none is set
        if (*base_var == -1)
        {
          *base_var = expr_count;
        }
        // the current expression becomes the base go signal, if none is set
        else
        {
          fprintf (output_stream, "  e_%d.go_r = e_%d.go_r;\n", expr_count, *base_var);
        }
        ret = expr_count++;
      }
      break;
    case E_FUNCTION:
      {
        Expr *tmp;
        *bitwidth = get_func_bitwidth (e->u.fn.s + strlen (e->u.fn.s) - 1);
        fprintf (output_stream, "  bundled_%s e_%d;\n", e->u.fn.s, expr_count);
        tmp = e->u.fn.r;
        while (tmp)
        {
        	symbol *v = find_symbol (__chp, (char *)tmp->u.e.l->u.e.l);
        	if (v->bitwidth == 1)
          {
        	  fprintf (output_stream, "  e_%d.%s = var_%s.v;\n", expr_count, v->name, v->name);
        	}
        	else
          {
        	  fprintf (output_stream, "  (i:%d: e_%d.%s[i] = var_%s[i].v;)\n", v->bitwidth, expr_count, v->name, v->name);
        	}
        	tmp = tmp->u.e.r;
        }
        if (*base_var == -1)
        {
  	       *base_var = expr_count;
        }
        else
        {
  	       fprintf (output_stream, "  e_%d.go_r = e_%d.go.r;\n", expr_count, *base_var);
        }
        ret = expr_count++;
      }
      break;
    default:
      fprintf (stderr, "Error: unknown type %d\n", e->type);
      break;
  }
  return ret;
}

/*
 * int print_expr (Expr *e, int *bitwidth, int *base_var)
 *
 * Summary:
 *
 *    Prints the ACT for an expression tree rooted at e. Resets base_var, which
 *    stores the go signal for the expression.
 *
 *    All lines of ACT are printed to the location specified by output_stream.
 *
 * Parameters: e - an Expr object containing the expression
 *             bitwidth - reference to an integer containing the bitwidth of the
 *                        current expression, as defined by its variables
 *             base_var - reference to the expr_count value to connect the go
 *                        signal to
 *
 * Return Value: the value of expr_count for the root expression
 */
int print_expr (Expr *e, int *bitwidth, int *base_var, int *delay)
{
  *base_var = -1;
  *delay = 0;
  return _print_expr (e, bitwidth, base_var, delay);
}

/*
 * int print_expr_tmpvar (char *req, int ego, int eout, int bits)
 *
 * Summary:
 *
 *    Prints a fully-sequenced receive of the node e_<eout> into a temporary
 *    variable e_<evar>. The base variable evaluation and the sequencing "go"
 *    signals are connected, so a master "go" signal triggers the following:
 *        - inputs to e_<eout> are evaluated when the request to the sequencer
 *          goes high
 *        - e_<eout> is received into a receiving buffer
 *        - the receiving buffer is closed by a completion tree
 *        - the receiving buffer is copied into a temporary variable e_<evar>
 *
 *    All lines of ACT are printed to the location specified by output_stream.
 *
 * Parameters: req - a string containing the request for the sequencing
 *             ego - expr_count for the base variable for the expression
 *             eout - expr_count for the signal to be stored
 *             bits - the bitwidth of e_<eout>
 *
 * Return Value: the value of expr_count for the temporary variable
 */
int print_expr_tmpvar (char *req, int ego, int eout, int bits)
{
  int seq = stmt_count++;
  int evar = expr_count++;

  fprintf (output_stream, "  syn_fullseq s_%d;\n", seq);
  fprintf (output_stream, "  %s = s_%d.go.r;\n", req, seq);

  if (bits == 1)
  {
    fprintf (output_stream, "  syn_recv rtv_%d;\n", seq);
    fprintf (output_stream, "  syn_expr_var e_%d;\n", evar);
    fprintf (output_stream, "  syn_var_init_false tv_%d(rtv_%d.v);\n", seq, seq);
    fprintf (output_stream, "  e_%d.v = tv_%d.v;\n", evar, seq);
    fprintf (output_stream, "  s_%d.r.r = e_%d.go_r;\n", seq, ego);
    fprintf (output_stream, "  s_%d.r = rtv_%d.go;\n", seq, seq);
    fprintf (output_stream, "  e_%d.out.t = rtv_%d.in.t;\n", eout, seq);
    fprintf (output_stream, "  e_%d.out.f = rtv_%d.in.f;\n", eout, seq);
  }
  else if (bundle_data)
  {
    fprintf (output_stream, "  bundled_recv<%d> brtv_%d;\n", bits, seq);
    fprintf (output_stream, "  syn_expr_vararray<%d> e_%d;\n", bits, evar);
    fprintf (output_stream, "  syn_var_init_false tv_%d[%d];\n", seq, bits);
    fprintf (output_stream, "  (i:%d: e_%d.v[i] = tv_%d[i].v; e_%d.v[i] = brtv_%d.v[i];)\n", bits, evar, seq, evar, seq);
    fprintf (output_stream, "  s_%d.r.r = brtv_%d.go.r;\n", seq, seq);
    fprintf (output_stream, "  s_%d.r.a = brtv_%d.go.a;\n", seq, seq);
    fprintf (output_stream, "  (i:%d: e_%d.out[i].t = brtv_%d.in.d[i].t; e_%d.out[i].f = brtv_%d.in.d[i].f;)\n", bits, eout, seq, eout, seq);
  }
  else
  {
    fprintf (output_stream, "  syn_recv rtv_%d[%d];\n", seq, bits);
    fprintf (output_stream, "  syn_expr_vararray<%d> e_%d;\n", bits, evar);
    fprintf (output_stream, "  syn_var_init_false tv_%d[%d];\n", seq, bits);
    fprintf (output_stream, "  (i:%d: e_%d.v[i] = tv_%d[i].v; e_%d.v[i] = rtv_%d[i].v;)\n", bits, evar, seq, evar, seq);
    fprintf (output_stream, "  s_%d.r.r = e_%d.go_r;\n", seq, ego);
    fprintf (output_stream, "  (i:%d: s_%d.r.r = rtv_%d[i].go.r;)\n", bits, seq, seq);
    fprintf (output_stream, "  syn_ctree<%d> ct_%d;\n", bits, seq);
    fprintf (output_stream, "  (i:%d: ct_%d.in[i] = rtv_%d[i].go.a;)\n", bits, seq, seq);
    fprintf (output_stream, "  s_%d.r.a = ct_%d.out;\n", seq, seq);
    fprintf (output_stream, "  (i:%d: e_%d.out[i].t = rtv_%d[i].in.t; e_%d.out[i].f = rtv_%d[i].in.f;)\n", bits, eout, seq, eout, seq);
  }
  fprintf (output_stream, "  s_%d.go.a = e_%d.go_r;\n", seq, evar);

  return evar;
}

/*
 * int print_one_gc (chp_gc_t *gc, int *bitwidth, int *base_var)
 *
 * Summary:
 *
 *    Prints a single guarded command. Connects the appropriate outputs to the
 *    evaluation request for the guarded command. Returns an integer that
 *    corresponds to the r1of2 channel that is used to evaluate the guard.
 *
 *    All lines of ACT are printed to the location specified by output_stream.
 *
 * Parameters: gc - object containing the CHP guard and statement
 *             bitwidth - reference to the width of the statement
 *             base_var - reference to the go signal for the statement
 *
 * Return Value: the integer corresponding to the guard's request channel
 */
int print_one_gc (chp_gc_t *gc, int *bitwidth, int *base_var)
{
  int a, b;
  int ret = gc_chan_count++;

  fprintf (output_stream, "  r1of2 gc_%d;\n", ret);
  // guarded statement case
  if (gc->g)
  {
    int delay;
    char buf[MAX_EXPR_SIZE];
    a = print_expr (gc->g, bitwidth, base_var, &delay);
    snprintf (buf, MAX_EXPR_SIZE, "gc_%d.r", ret);

    if (bundle_data && *bitwidth > 1)
    {
      delay += get_bundle_delay (*bitwidth, gc->g->type);
      if (delay > 0)
      {
        fprintf (output_stream, "  delay<%d> de_%d(%s);\n", delay, a, buf);
        snprintf (buf, MAX_EXPR_SIZE, "de_%d.out", a);
      }
      delay = get_bundle_delay (1, E_NOT);
      fprintf (output_stream, "  delay<%d> dn_%d(%s);\n", delay, a, buf);
      fprintf (output_stream, "  bundled_var_to_dualrail be_%d(dn_%d.out, be_%d.out);\n", expr_count, a, a);
      fprintf (output_stream, "  syn_expr_var e_%d;\n", expr_count);
      fprintf (output_stream, "  e_%d.v = be_%d.out;\n", expr_count, expr_count);
      snprintf (buf, MAX_EXPR_SIZE, "dn_%d.out", a);
      a = expr_count++;
      *base_var = a;
    }

    // replace guard output with latched value
    a = print_expr_tmpvar (buf, *base_var, a, 1);

    // print guarded statement
    b = print_chp_stmt (gc->s, bitwidth, base_var);

    // empty guard
    if (b == -1)
    {
      fprintf (output_stream, "  gc_%d.t = e_%d.out.t;\n", ret, a);
    }
    // non-empty guard
    else
    {
      fprintf (output_stream, "  e_%d.out.t = c_%d.r;\n", a, b);
      fprintf (output_stream, "  gc_%d.t = c_%d.a;\n", ret, b);
    }
    fprintf (output_stream, "  gc_%d.f = e_%d.out.f;\n", ret, a);
  }
  // unguarded statement case (implicit true guard)
  else
  {
    b = print_chp_stmt (gc->s, bitwidth, base_var);
    fprintf (output_stream, "  gc_%d.r = c_%d.r;\n", ret, b);
    fprintf (output_stream, "  gc_%d.t = c_%d.a;\n", ret, b);
    //fprintf (output_stream, "  prs { Reset|~Reset -> gc_%d.f- }\n", ret);
    fprintf (output_stream, "  gc_%d.f = GND;\n", ret);
  }
  return ret;
}

/*
 * int print_gc (int loop, chp_gc_t *gc, int *bitwidth, int *base_var)
 *
 * Summary:
 *
 *    Prints a collection of guarded commands by calling print_one_gc().
 *
 *    All lines of ACT are printed to the location specified by output_stream.
 *
 * Parameters: loop - discriminator between a loop and a selection statement
 *             gc - object containing the CHP guard and statement
 *             bitwidth - reference to the width of the statement
 *             base_var - reference to the go signal for the statement
 *
 * Return Value: the integer corresponding to the first guard's request channel
 */
int print_gc (bool loop, chp_gc_t *gc, int *bitwidth, int *base_var)
{
  static int gc_num = 0;
  int na;
  int this_gc = gc_num++;

  fprintf (output_stream, "\n  /* emit individual gc (#%d) [%s] */\n", this_gc, loop ? "loop" : "selection");
  int start_gc_chan = print_one_gc (gc, bitwidth, base_var);
  int end_gc_chan = start_gc_chan;
  gc = gc->next;
  while (gc)
  {
    end_gc_chan = print_one_gc (gc, bitwidth, base_var);
    gc = gc->next;
  }
  int ret = chan_count++;
  fprintf (output_stream, "  a1of1 c_%d;\n", ret);
  fprintf (output_stream, "  /* gc cascade, start = %d, end = %d */\n", start_gc_chan, end_gc_chan);

  // cascade guards for deterministic selection
  for (int i = start_gc_chan; i < end_gc_chan; i++)
  {
    fprintf (output_stream, "  gc_%d.f = gc_%d.r;\n", i, i+1);
  }

  // connect statment request to first guard
  if (!loop)
  {
    fprintf (output_stream, "  gc_%d.r = c_%d.r;\n", start_gc_chan, ret);
  }
  else
  {
    na = stmt_count++;
    fprintf (output_stream, "  syn_bool_notand na_%d;\n", na);
    fprintf (output_stream, "  na_%d.in1 = c_%d.r;\n", na, ret);
    fprintf (output_stream, "  na_%d.out = gc_%d.r;\n", na, start_gc_chan);
  }

  // single guard case
  if (start_gc_chan == end_gc_chan)
  {
    if (!loop)
    {
      fprintf (output_stream, "  gc_%d.t = c_%d.a;\n", start_gc_chan, ret);
    }
    else
    {
      fprintf (output_stream, "  gc_%d.t = na_%d.in2;\n", start_gc_chan, na);
      fprintf (output_stream, "  gc_%d.f = c_%d.a;\n", start_gc_chan, ret);
    }
  }
  // multi-guard case
  else
  {
    // construct a multi-stage or gate for guard outputs
    int a, b;
    a = stmt_count++;
    fprintf (output_stream, "  syn_bool_or or_%d(gc_%d.t,gc_%d.t);\n", a, start_gc_chan, start_gc_chan+1);
    for (int i = start_gc_chan+2; i < end_gc_chan; i++)
    {
      b = stmt_count++;
      fprintf (output_stream, "  syn_bool_or or_%d(or_%d.out,gc_%d.t);\n", b, a, i);
      a = b;
    }

    // connect multi-stage or gate output to statement acknowledge
    if (!loop)
    {
      fprintf (output_stream, "  or_%d.out = c_%d.a;\n", a, ret);
    }
    else
    {
      fprintf (output_stream, "  or_%d.out = na_%d.in2;\n", a, na);
      fprintf (output_stream, "  gc_%d.f = c_%d.a;\n", end_gc_chan, ret);
    }
  }

  fprintf (output_stream, "  /* end of gc (#%d) */\n\n", this_gc);
  return ret;
}

/*
 * int print_chp_stmt (chp_lang_t *c, int *bitwidth, int *base_var)
 *
 * Summary:
 *
 *    Prints a CHP statement specified by c. Returns an integer corresponding to
 *    the master request channel for the statement. CHP operations are
 *    constructed in the following manner:
 *        - ASSIGNMENT:  LHS is evaluated and received into a temporary variable,
 *                       then received into the RHS variable
 *        - SEND:        data is evaluated and received into a temporary variable,
 *                       then connected to the send channel
 *        - RECEIVE:     data from channel is received into the specified variable
 *        - COMPOSITION: go signals for individual modules are connected, and
 *                       the function is called recursively
 *        - GUARDS:      guarded statement chains (loops or selection statements)
 *                       are evaluated with print_gc()
 *
 *    All lines of ACT are printed to the location specified by output_stream.
 *
 * Parameters: c - object containing the CHP statement
 *             bitwidth - reference to the width of the statement
 *             base_var - reference to the go signal for the statement
 *
 * Return Value: the integer corresponding to the base request channel for the
 *               statement
 */
int print_chp_stmt (chp_lang_t *c, int *bitwidth, int *base_var)
{
  int ret;
  int a, b;
  int delay;
  symbol *v, *u;
  char buf[MAX_EXPR_SIZE];
  if (!c) return -1;
  switch (c->type)
  {
    case CHP_SKIP:
      fprintf (output_stream, "  /* skip */");
      fprintf (output_stream, "  syn_skip s_%d(c_%d);\n", stmt_count, chan_count);
      stmt_count++;
      ret = chan_count++;
      break;

    case CHP_ASSIGN:
      fprintf (output_stream, "  /* assign */\n");
      v = find_symbol (__chp, c->u.assign.id);
      *bitwidth = v->bitwidth;
      a = print_expr (c->u.assign.e, bitwidth, base_var, &delay);
      ret = chan_count++;
      b = stmt_count++;
      fprintf (output_stream, "  a1of1 c_%d;\n", ret);
      snprintf (buf, MAX_EXPR_SIZE, "c_%d.r", ret);
      if (bundle_data && v->bitwidth > 1)
      {
        delay += get_bundle_delay (*bitwidth, c->u.assign.e->type);
        if (delay > 0)
        {
          fprintf (output_stream, "  delay<%d> de_%d(%s);\n", delay, a, buf);
          snprintf (buf, MAX_EXPR_SIZE, "de_%d.out", a);
        }
        delay = get_bundle_delay (*bitwidth, E_NOT);
        fprintf (output_stream, "  delay<%d> dn_%d(%s);\n", delay, a, buf);
        fprintf (output_stream, "  bundled_vararray_to_dualrail<%d> be_%d(dn_%d.out, be_%d.out);\n", v->bitwidth, expr_count, a, a);
        fprintf (output_stream, "  syn_expr_vararray<%d> e_%d;\n", v->bitwidth, expr_count);
        fprintf (output_stream, "  e_%d.go_r = dn_%d.out;\n", expr_count, a);
        fprintf (output_stream, "  (i:%d: e_%d.v[i] = be_%d.out[i];)\n", v->bitwidth, expr_count, expr_count);
        snprintf (buf, MAX_EXPR_SIZE, "e_%d.go_r", expr_count);
        a = expr_count++;
      }

      a = print_expr_tmpvar (buf, *base_var, a, *bitwidth);
      // fprintf (output_stream, "  e_%d.go_r = c_%d.r;\n", go_r, ret);
      if (v->bitwidth == 1)
      {
        fprintf (output_stream, "  syn_recv s_%d(c_%d);\n", b, ret);
        fprintf (output_stream, "  s_%d.in.t = e_%d.out.t;\n", b, a);
        fprintf (output_stream, "  s_%d.in.f = e_%d.out.f;\n", b, a);
        fprintf (output_stream, "  s_%d.v = var_%s.v;\n", b, c->u.assign.id);
      }
      else if (bundle_data)
      {
        fprintf (output_stream, "  bundled_recv<%d> s_%d;\n", v->bitwidth, b);
        fprintf (output_stream, "  s_%d.go.r = e_%d.go_r;\n", b, a);
        fprintf (output_stream, "  s_%d.go.a = c_%d.a;\n", b, ret);
        fprintf (output_stream, "  (i:%d: s_%d.in.d[i].t = e_%d.out[i].t;\n", v->bitwidth, b, a);
        fprintf (output_stream, "        s_%d.in.d[i].f = e_%d.out[i].f;\n", b, a);
        fprintf (output_stream, "        s_%d.v[i] = var_%s[i].v;)\n", b, c->u.assign.id);
      }
      else
      {
        fprintf (output_stream, "  syn_recv s_%d[%d];\n", b, v->bitwidth);
        fprintf (output_stream, "  (i:%d: s_%d[i].go.r = c_%d.r;)\n", v->bitwidth, b, ret);
        fprintf (output_stream, "  syn_ctree<%d> ct_%d;\n", v->bitwidth, b);
        fprintf (output_stream, "  (i:%d: ct_%d.in[i] = s_%d[i].go.a;)\n", v->bitwidth, b, b);
        fprintf (output_stream, "  ct_%d.out = c_%d.a;\n", b, ret);
        fprintf (output_stream, "  (i:%d: s_%d[i].in.t = e_%d.out[i].t;\n", v->bitwidth, b, a);
        fprintf (output_stream, "        s_%d[i].in.f = e_%d.out[i].f;\n", b, a);
        fprintf (output_stream, "        s_%d[i].v = var_%s[i].v;)\n", b, c->u.assign.id);
      }
      fprintf (output_stream, "\n");
      if (optimization > 0) hash_remove_expr (evaluated_exprs, c->u.assign.id);
      break;

    case CHP_SEND:
      fprintf (output_stream, "  /* send */\n");
      if (list_length (c->u.comm.rhs) == 1)
      {
        v = find_symbol (__chp, c->u.comm.chan);
        *bitwidth = v->bitwidth;
        a = print_expr ((Expr *)list_value (list_first (c->u.comm.rhs)), bitwidth, base_var, &delay);
        ret = chan_count++;
        fprintf (output_stream, "  a1of1 c_%d;\n", ret);
        snprintf (buf, MAX_EXPR_SIZE, "c_%d.r", ret);
        if (bundle_data && v->bitwidth > 1)
        {
          delay += get_bundle_delay (*bitwidth, ((Expr *)list_value (list_first (c->u.comm.rhs)))->type);
          if (delay > 0)
          {
            fprintf (output_stream, "  delay<%d> de_%d(%s);\n", delay, a, buf);
            snprintf (buf, MAX_EXPR_SIZE, "de_%d.out", a);
          }
          delay = get_bundle_delay (*bitwidth, E_NOT);
          fprintf (output_stream, "  delay<%d> dn_%d(%s);\n", delay, a, buf);
          fprintf (output_stream, "  bundled_vararray_to_dualrail<%d> be_%d(dn_%d.out, be_%d.out);\n", v->bitwidth, expr_count, a, a);
          fprintf (output_stream, "  syn_expr_vararray<%d> e_%d;\n", v->bitwidth, expr_count);
          fprintf (output_stream, "  e_%d.go_r = dn_%d.out;\n", expr_count, a);
          fprintf (output_stream, "  (i:%d: e_%d.v[i] = be_%d.out[i];)\n", v->bitwidth, expr_count, expr_count);
          snprintf (buf, MAX_EXPR_SIZE, "e_%d.go_r", expr_count);
          a = expr_count++;
        }

        a = print_expr_tmpvar (buf, *base_var, a, *bitwidth);
        // fprintf (output_stream, "  e_%d.go_r = c_%d.r;\n", go_r, ret);
        fprintf (output_stream, "  c_%d.a = e_%d.go_r;\n", ret, a);
        if (*bitwidth == 1)
        {
  	       fprintf (output_stream, "  chan_%s.t = e_%d.out.t;\n", c->u.comm.chan, a);
  	       fprintf (output_stream, "  chan_%s.f = e_%d.out.f;\n", c->u.comm.chan, a);
        }
        else if (bundle_data)
        {
          fprintf (output_stream, "  (i:%d: chan_%s.d[i] = e_%d.out[i];)\n", v->bitwidth, v->name, a);
        }
        else
        {
  	       fprintf (output_stream, "  (i:%d: chan_%s.d[i] = e_%d.out[i];)\n", v->bitwidth, v->name, a);
        }
      }
      fprintf (output_stream, "\n");
      break;

    case CHP_RECV:
      fprintf (output_stream, "  /* recv */\n");
      if (list_length (c->u.comm.rhs) == 1)
      {
        v = find_symbol (__chp, c->u.comm.chan);
        u = find_symbol (__chp, (char *)list_value (list_first (c->u.comm.rhs)));
        *bitwidth = v->bitwidth;
        ret = chan_count++;
        a = stmt_count++;
        fprintf (output_stream, "  a1of1 c_%d;\n", ret);
        if (v->bitwidth == 1)
        {
        	fprintf (output_stream, "  syn_recv s_%d(c_%d);\n", a, ret);
        	fprintf (output_stream, "  s_%d.in = chan_%s;\n", a, c->u.comm.chan);
        	fprintf (output_stream, "  s_%d.v = var_%s.v;\n", a, u->name);
        }
        else if (bundle_data)
        {
          fprintf (output_stream, "  bundled_recv<%d> s_%d;\n", v->bitwidth, a);
          fprintf (output_stream, "  s_%d.go.r = c_%d.r;\n", a, ret);
          fprintf (output_stream, "  s_%d.go.a = c_%d.a; c_%d.a = chan_%s.a;\n", a, ret, ret, v->name);
          fprintf (output_stream, "  (i:%d: s_%d.in.d[i].t = chan_%s.d[i].t;\n", v->bitwidth, a, v->name);
          fprintf (output_stream, "        s_%d.in.d[i].f = chan_%s.d[i].f;\n", a, v->name);
          fprintf (output_stream, "        s_%d.v[i] = var_%s[i].v;)\n", a, u->name);
        }
        else
        {
        	fprintf (output_stream, "  syn_recv s_%d[%d];\n", a, v->bitwidth);
        	fprintf (output_stream, "  (i:%d: s_%d[i].go.r = c_%d.r;)\n", v->bitwidth, a, ret);
        	fprintf (output_stream, "  syn_ctree<%d> ct_%d;\n", v->bitwidth, a);
        	fprintf (output_stream, "  (i:%d: ct_%d.in[i] = s_%d[i].go.a;)\n", v->bitwidth, a, a);
        	fprintf (output_stream, "  ct_%d.out = c_%d.a; c_%d.a = chan_%s.a;\n", a, ret, ret, v->name);
        	fprintf (output_stream, "  (i:%d: s_%d[i].in.t = chan_%s.d[i].t;\n", v->bitwidth, a, v->name);
          fprintf (output_stream, "        s_%d[i].in.f = chan_%s.d[i].f;\n", a, v->name);
          fprintf (output_stream, "        s_%d[i].v = var_%s[i].v;)\n", a, u->name);
        }
        if (optimization > 0) hash_remove_expr (evaluated_exprs, u->name);
      }
      fprintf (output_stream, "\n");
      break;

    case CHP_COMMA:
    case CHP_SEMI:
      {
        listitem_t *li;
        // special case for a single (non-composed) statement, generated by parser
        if (list_length (c->u.semi_comma.cmd) == 1)
        {
          return print_chp_stmt ((chp_lang_t *)list_value (list_first (c->u.semi_comma.cmd)), bitwidth, base_var);
        }
        fprintf (output_stream, "  /* %s */\n", c->type == CHP_COMMA ? "comma" : "semicolon");
        a = chan_count++;
        ret = a;
        fprintf (output_stream, "  a1of1 c_%d;\n", ret);
        fprintf (output_stream, "\n");
        // iterate through all composite statements
        for (li = list_first (c->u.semi_comma.cmd); list_next (li); li = list_next (li))
        {
          int s;
          // print the left statement
  	      b = print_chp_stmt ((chp_lang_t *)list_value (li), bitwidth, base_var);
          s = stmt_count++;
          // connect the go signal to the statement appropriately
          fprintf (output_stream, "  syn_%s s_%d(c_%d);\n", c->type == CHP_COMMA ? "par" : "seq", s, a);
          fprintf (output_stream, "  s_%d.s1 = c_%d;\n", s, b);
          // on the last loop iteration, print the final statement
          if (!list_next (list_next (li)))
          {
             fprintf (output_stream, "\n");
             b = print_chp_stmt ((chp_lang_t *)list_value (list_next (li)), bitwidth, base_var);
             fprintf (output_stream, "  s_%d.s2 = c_%d;\n", s, b);
          }
          // otherwise, connect the channels appropriately
          else
          {
             fprintf (output_stream, "  a1of1 c_%d;\n", chan_count);
             fprintf (output_stream, "  s_%d.s2 = c_%d;\n", s, chan_count);
             a = chan_count++;
             fprintf (output_stream, "\n");
          }
        }
        fprintf (output_stream, "\n");
      }
      break;

    case CHP_LOOP:
    case CHP_SELECT:
      ret = print_gc ((c->type == CHP_LOOP) ? true : false, c->u.gc, bitwidth, base_var);
      break;

    default:
      fprintf (stderr, "Error: unsupported token: %d\n", c->type);
      exit (1);
      break;
  }
  return ret;
}

/*
 * void print_chp_structure (Chp *c)
 *
 * Summary:
 *
 *    Prints a CHP program, compiled into ACT. Prints all the syntactical
 *    pieces that come at the start and end of the file, as specified by
 *    the parameters passed in on the command line.
 *
 *    All lines of ACT are printed to the location specified by output_stream.
 *
 * Parameters: c - object containing the CHP program
 */
void print_chp_structure (Chp *c)
{
  // initialize the output location
  if (output_file)
  {
    char *output_path = calloc (MAX_PATH_SIZE, sizeof(char));
    char *bundled = bundle_data ? "bundled_" : "";
    snprintf (output_path, MAX_PATH_SIZE, "%s/tst/%s%s", ACT_PATH, bundled, output_file);
    output_stream = fopen (output_path, "w");
    free (output_path);
  }
  else
  {
    output_stream = stdout;
  }

  // print import statements and wrapper process declaration
  fprintf (output_stream, "import \"%s/syn.act\";\n", ACT_PATH);
  if (bundle_data) fprintf (output_stream, "import \"%s/bundled.act\";\n", ACT_PATH);
  fprintf (output_stream, "\n");
  fprintf (output_stream, "defproc toplevel (a1of1 go)\n{\n");
  // initialize all variables and channels
  print_vars (c);

  int *bitwidth = calloc (1, sizeof (int));
  int *base_var = calloc (1, sizeof (int));
  if (optimization > 0) evaluated_exprs = hash_new (INITIAL_HASH_SIZE);
  // print the compiled CHP program
  int i = print_chp_stmt (c->c, bitwidth, base_var);
  free (bitwidth);
  free(base_var);
  if (optimization > 0)
  {
    // free allocated memory for buckets still remaining in the table
    hash_bucket_t *b;
    for (int j = 0; j < c->sym->size; j++)
    {
      for (b = c->sym->head[j]; b; b = b->next)
      {
        free (b->v);
      }
    }
    // free the table itself
    hash_free (evaluated_exprs);
  }

  // connect master "go" signal and print wrapper process instantiation
  fprintf (output_stream, "  go = c_%d;\n", i);
  fprintf (output_stream, "}\n\n");
  fprintf (output_stream, "toplevel t;\n");

  if (output_file) fclose (output_stream);
}
