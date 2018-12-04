#include "cartographer.h"

int expr_count = 1;
int stmt_count = 0;
int chan_count = 0;
bool bundle_data = false;
int optimization = 0;
char *output_file = NULL;
FILE *output_stream;
struct Hashtable *evaluated_exprs;

#define INITIAL_HASH_SIZE 10
#define MAX_EXPR_SIZE 1024
#define MAX_KEY_SIZE 2048
#define MAX_PATH_SIZE 4096

void print_vars (Chp *c)
{
  hash_bucket_t *b;
  int i;
  symbol *sym;

  if (!c || !c->sym) return;
  if (c->sym->n == 0) return;

  fprintf (output_stream, "  /* --- declaring all variables and channels --- */\n");

  for (i = 0; i < c->sym->size; i++)
  {
    for (b = c->sym->head[i]; b; b = b->next)
    {
      sym = (symbol *) b->v;
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

void emit_const_1 (void)
{
  static int emitted = 0;
  if (!emitted)
  {
    fprintf (output_stream, "  syn_var_init_true const_1;\n");
  }
  emitted = 1;
}

void emit_const_0 (void)
{
  static int emitted = 0;
  if (!emitted)
  {
    fprintf (output_stream, "  syn_var_init_false const_0;\n");
  }
  emitted = 1;
}

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

int get_func_bitwidth (char *s)
{
  while (isdigit (*s)) s--;
  return atoi (s+1);
}

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

void hash_add_expr (struct Hashtable *h, const char *expr)
{
  hash_bucket_t *b = hash_add (h, expr);
  b->v = calloc (MAX_EXPR_SIZE, sizeof(char));
  snprintf (b->v, MAX_EXPR_SIZE, "e_%d", expr_count);
}

void hash_remove_expr (struct Hashtable *h, const char *expr)
{
  for (int i = 0; i < h->size; i++) hash_remove_expr_rec (h, expr, h->head[i]);
}

void _hash_remove_expr (struct Hashtable *h, const char *expr, hash_bucket_t *b)
{
  if (!b) return;
  _hash_remove_expr (h, expr, b->next);
  if (strstr (b->key, expr))
  {
    hash_remove_expr (h, (char *) b->v);
    free (b->v);
    hash_delete (h, b->key);
  }
}

int hash_get_or_add (struct Hashtable *h, const char* s, Expr *l, Expr *r, int nl, int nr, bool commutative)
{
  hash_bucket_t *b;
  char *left_expr, *right_expr, *k, *_k;

  left_expr = calloc (MAX_EXPR_SIZE, sizeof(char));
  if (r) right_expr = calloc (MAX_EXPR_SIZE, sizeof(char));
  get_expr (l, nl, left_expr);
  if (r) get_expr (r, nr, right_expr);
  k = calloc (MAX_KEY_SIZE, sizeof(char));
  if (commutative) _k = calloc (MAX_KEY_SIZE, sizeof(char));
  snprintf (k, MAX_KEY_SIZE, "%s(%s,%s)", s, left_expr, right_expr);
  if (commutative) snprintf (_k, MAX_KEY_SIZE, "%s(%s,%s)", s, right_expr, left_expr);
  free (left_expr);
  if (r) free (right_expr);

  if ((b = hash_lookup (h, k)))
  {
    free (k);
    if (commutative) free (_k);
    char *t = (char *) b->v + strlen ((char *) b->v) - 1;
    while (isdigit (*t)) t--;
    return atoi (++t);
  }
  else
  {
    hash_add_expr (h, k);
    if (commutative) hash_add_expr (h, _k);
    free (k);
    if (commutative) free (_k);
    return -1;
  }
}

int unop (const char *s, Expr *e, int *bitwidth, int *base_var)
{
  int l = _print_expr (e->u.e.l, bitwidth, base_var);

  if (*bitwidth == 1)
  {
    if (optimization > 0)
    {
      int ret = hash_get_or_add (evaluated_exprs, s, e->u.e.l, NULL, l, -1, false);
      if (ret > 0) return ret;
    }
    fprintf (output_stream, "  syn_expr_%s e_%d(e_%d.out);\n", s, expr_count, l);
  }
  else if (bundle_data)
  {
    fprintf (output_stream, "  bundled_%s<%d> e_%d;\n", s, *bitwidth, expr_count);
    fprintf (output_stream, "  (i:%d: e_%d.in[i] = e_%d.out[i];)\n", *bitwidth, expr_count, l);
    if (*base_var == -1)
    {
       *base_var = expr_count;
    }
    else
    {
       fprintf (output_stream, "  e_%d.go_r = e_%d.go_r;\n", expr_count, *base_var);
    }
  }
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

int binop (const char *s, Expr *e, int *bitwidth, int *base_var, bool commutative)
{
  int l = _print_expr (e->u.e.l, bitwidth, base_var);
  int r = _print_expr (e->u.e.r, bitwidth, base_var);

  if (*bitwidth == 1)
  {
    if (optimization > 0)
    {
      int ret = hash_get_or_add (evaluated_exprs, s, e->u.e.l, e->u.e.r, l, r, commutative);
      if (ret > 0) return ret;
    }
    fprintf (output_stream, "  syn_expr_%s e_%d(e_%d.out, e_%d.out);\n", s, expr_count, l, r);
  }
  else if (bundle_data)
  {
    fprintf (output_stream, "  bundled_%s<%d> e_%d;\n", s, *bitwidth, expr_count);
    fprintf (output_stream, "  (i:%d: e_%d.in1[i] = e_%d.out[i];)\n", *bitwidth, expr_count, l);
    fprintf (output_stream, "  (i:%d: e_%d.in2[i] = e_%d.out[i];)\n", *bitwidth, expr_count, r);
    if (*base_var == -1)
    {
       *base_var = expr_count;
    }
    else
    {
       fprintf (output_stream, "  e_%d.go_r = e_%d.go_r;\n", expr_count, *base_var);
    }
  }
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

Chp *__chp;

int _print_expr (Expr *e, int *bitwidth, int *base_var)
{
  int ret;
  switch (e->type)
  {
    case E_AND:
      ret = binop ("and", e, bitwidth, base_var, true);
      break;
    case E_OR:
      ret = binop ("or", e, bitwidth, base_var, true);
      break;
    case E_XOR:
      ret = binop ("xor", e, bitwidth, base_var, true);
      break;
    case E_PLUS:
      ret = binop ("add", e, bitwidth, base_var, true);
      break;
    case E_MINUS:
      ret = binop ("sub", e, bitwidth, base_var, false);
      break;
    case E_MULT:
      ret = binop ("mul", e, bitwidth, base_var, true);
      break;
    case E_NOT:
    case E_COMPLEMENT:
      ret = unop ("not", e, bitwidth, base_var);
      break;
    case E_UMINUS:
      ret = unop ("uminus", e, bitwidth, base_var);
      break;
    case E_EQ:
      ret = binop ("eq", e, bitwidth, base_var, true);
      break;
    case E_NE:
      ret = binop ("ne", e, bitwidth, base_var, true);
      break;
    case E_LT:
      ret = binop ("lt", e, bitwidth, base_var, false);
      break;
    case E_GT:
      ret = binop ("gt", e, bitwidth, base_var, false);
      break;
    case E_LE:
      ret = binop ("le", e, bitwidth, base_var, false);
      break;
    case E_GE:
      ret = binop ("ge", e, bitwidth, base_var, false);
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
        }
        else
        {
        	fprintf (output_stream, "  syn_expr_vararray<%d> e_%d;\n", v->bitwidth, expr_count);
        	fprintf (output_stream, "  (i:%d: e_%d.v[i] = var_%s[i].v;)\n", v->bitwidth, expr_count, v->name);
        }
        if (*base_var == -1)
        {
  	       *base_var = expr_count;
        }
        else
        {
  	       fprintf (output_stream, "  e_%d.go_r = e_%d.go_r;\n", expr_count, *base_var);
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
            fprintf(output_stream, "  e_%d.v[%d] = const_0.v;\n", expr_count, *bitwidth - i);
          }
        }
      }
      if (*base_var == -1)
      {
        *base_var = expr_count;
      }
      else
      {
        fprintf (output_stream, "  e_%d.go_r = e_%d.go_r;\n", expr_count, *base_var);
      }
      ret = expr_count++;
      break;
    case E_TRUE:
      emit_const_1 ();
      fprintf (output_stream, "  syn_expr_var e_%d(,const_1.v);\n", expr_count);
      if (*base_var == -1)
      {
        *base_var = expr_count;
      }
      else
      {
        fprintf (output_stream, "  e_%d.go_r = e_%d.go_r;\n", expr_count, *base_var);
      }
      ret = expr_count++;
      break;
    case E_FALSE:
      emit_const_0 ();
      fprintf (output_stream, "  syn_expr_var e_%d(,const_0.v);\n", expr_count);
      if (*base_var == -1)
      {
        *base_var = expr_count;
      }
      else
      {
        fprintf (output_stream, "  e_%d.go_r = e_%d.go_r;\n", expr_count, *base_var);
      }
      ret = expr_count++;
      break;
    case E_PROBE:
      {
        symbol *s = find_symbol (__chp, (char *) e->u.e.l);
        *bitwidth = s->bitwidth;
        if (s->bitwidth > 1)
        {
      	  fprintf (output_stream, "  syn_aN1of2_probe<%d> e_%d(chan_%s);\n", s->bitwidth, expr_count, s->name);
      	}
      	else
        {
      	  fprintf (output_stream, "  syn_a1of2_probe e_%d(chan_%s.t, chan_%s.f, chan_%s.a);\n", expr_count, s->name, s->name, s->name);
      	}
        ret = expr_count++;
      }
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
  returns the expression # for e_<num>.out.{t,f}
                          or   e_<num>.out[i].{t,f} for multi-bit

  base_var is for the expression # for e_<num>.go_r
*/
int print_expr (Expr *e, int *bitwidth, int *base_var)
{
  *base_var = -1;
  return _print_expr (e, bitwidth, base_var);
}

/*
  go for expression is e_<ego>.go.r
  out is e_<eout>.out.{t,f}
  req is request for evaluation

  returns new number for e_<num>.out.{t,f}
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

int gc_chan_count = 0;

/*
  Returns integer that corresponds to r1of2 channel that is used to
  evaluate the expression and return the Boolean result (t/f).
      t = guard evaluated to true and the statement was executed
      f = guard evaluated to false
*/
int print_one_gc (chp_gc_t *gc, int *bitwidth, int *base_var)
{
  int a, b;
  int ret = gc_chan_count++;
  if (!gc) return -1;

  fprintf (output_stream, "  r1of2 gc_%d;\n", ret);
  if (gc->g)
  {
    int go_r;
    char buf[MAX_EXPR_SIZE];
    a = print_expr (gc->g, bitwidth, base_var);
    go_r = *base_var;

    snprintf (buf, MAX_EXPR_SIZE, "gc_%d.r", ret);

    /* replace "a"  with latched value */
    a = print_expr_tmpvar (buf, go_r, a, 1);

    b = print_chp_stmt (gc->s, bitwidth, base_var);

    if (b == -1)
    {
      /* empty */
      fprintf (output_stream, "  gc_%d.t = e_%d.out.t;\n", ret, a);
    }
    else
    {
      fprintf (output_stream, "  e_%d.out.t = c_%d.r;\n", a, b);
      fprintf (output_stream, "  gc_%d.t = c_%d.a;\n", ret, b);
    }
    fprintf (output_stream, "  gc_%d.f = e_%d.out.f;\n", ret, a);
  }
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
  prints loop/selection staetment, returns channel # for the "go"  command.
*/
int print_gc (int loop, chp_gc_t *gc, int *bitwidth, int *base_var)
{
  int start_gc_chan = gc_chan_count;
  int end_gc_chan;
  int ret, i;
  int na;
  static int gc_num = 0;
  int mygc;

  mygc = gc_num++;
  fprintf (output_stream, "\n  /*--- emit individual gc (#%d) [%s] ---*/\n", mygc, loop ? "loop"  : "selection");
  start_gc_chan = print_one_gc (gc, bitwidth, base_var);
  end_gc_chan = start_gc_chan;
  gc = gc->next;
  while (gc)
  {
    end_gc_chan = print_one_gc (gc, bitwidth, base_var);
    gc = gc->next;
  }

  ret = chan_count++;
  fprintf (output_stream, "  a1of1 c_%d;\n", ret);

  fprintf (output_stream, "  /* gc cascade, start = %d, end = %d */\n", start_gc_chan, end_gc_chan);

  for (i = start_gc_chan; i < end_gc_chan; i++)
  {
    /* gc cascade */
    fprintf (output_stream, "  gc_%d.f = gc_%d.r;\n", i, i+1);
  }
  if (!loop)
  {
    fprintf (output_stream, "  gc_%d.r = c_%d.r;\n", start_gc_chan, ret);
  }
  else
  {
    na = stmt_count++;
    fprintf (output_stream, "  syn_notand na_%d;\n", na);
    fprintf (output_stream, "  na_%d.x = c_%d.r;\n", na, ret);
    fprintf (output_stream, "  na_%d.out = gc_%d.r;\n", na, start_gc_chan);
  }

  if (start_gc_chan == end_gc_chan)
  {
    /* only one guard */
    if (!loop)
    {
      fprintf (output_stream, "  gc_%d.t = c_%d.a;\n", start_gc_chan, ret);
    }
    else
    {
      fprintf (output_stream, "  gc_%d.t = na_%d.y;\n", start_gc_chan, na);
      fprintf (output_stream, "  gc_%d.f = c_%d.a;\n", start_gc_chan, ret);
    }
  }
  else
  {
    int a, b;
    /* multi-stage or gate */
    a = stmt_count++;
    fprintf (output_stream, "  syn_or2 or_%d(gc_%d.t,gc_%d.t);\n", a, start_gc_chan, start_gc_chan+1);
    for (i = start_gc_chan+2; i < end_gc_chan; i++)
    {
      b = stmt_count++;
      fprintf (output_stream, "  syn_or2 or_%d(or_%d.out,gc_%d.t);\n", b, a, i);
      a = b;
    }
    if (!loop)
    {
      fprintf (output_stream, "  or_%d.out = c_%d.a;\n", a, ret);
    }
    else
    {
      fprintf (output_stream, "  or_%d.out = na_%d.y;\n", a, na);
      fprintf (output_stream, "  gc_%d.f = c_%d.a;\n", end_gc_chan, ret);
    }
  }
  fprintf (output_stream, "  /* ----- end of gc (#%d) ----- */\n\n", mygc);
  return ret;
}

/*
  Prints chp statement, returns channel # for "go"  command
*/
int print_chp_stmt (chp_lang_t *c, int *bitwidth, int *base_var)
{
  int ret;
  int a, b;
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
      a = print_expr (c->u.assign.e, bitwidth, base_var);
      ret = chan_count++;
      b = stmt_count++;
      fprintf (output_stream, "  a1of1 c_%d;\n", ret);
      snprintf (buf, MAX_EXPR_SIZE, "c_%d.r", ret);
      a = print_expr_tmpvar (buf, *base_var, a, *bitwidth);
      // fprintf (output_stream, "  e_%d.go_r = c_%d.r;\n", go_r, ret);
      if (v->bitwidth == 1)
      {
        fprintf (output_stream, "  syn_recv s_%d(c_%d);\n", b, ret);
        fprintf (output_stream, "  s_%d.in.t = e_%d.out.t;\n", b, a);
        fprintf (output_stream, "  s_%d.in.f = e_%d.out.f;\n", b, a);
        fprintf (output_stream, "  s_%d.v = var_%s.v;\n", b, c->u.assign.id);
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
        a = print_expr ((Expr *)list_value (list_first (c->u.comm.rhs)), bitwidth, base_var);
        ret = chan_count++;
        fprintf (output_stream, "  a1of1 c_%d;\n", ret);
        snprintf (buf, MAX_EXPR_SIZE, "c_%d.r", ret);
        a = print_expr_tmpvar (buf, *base_var, a, *bitwidth);
        // fprintf (output_stream, "  e_%d.go_r = c_%d.r;\n", go_r, ret);
        fprintf (output_stream, "  c_%d.a = chan_%s.a;\n", ret, c->u.comm.chan);
        if (*bitwidth == 1)
        {
  	       fprintf (output_stream, "  chan_%s.t = e_%d.out.t;\n", c->u.comm.chan, a);
  	       fprintf (output_stream, "  chan_%s.f = e_%d.out.f;\n", c->u.comm.chan, a);
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
        if (list_length (c->u.semi_comma.cmd) == 1)
        {
          return print_chp_stmt ((chp_lang_t *)list_value (list_first (c->u.semi_comma.cmd)), bitwidth, base_var);
        }
        fprintf (output_stream, "  /* %s */\n", c->type == CHP_COMMA ? "comma"  : "semicolon");
        a = chan_count++;
        ret = a;
        fprintf (output_stream, "  a1of1 c_%d;\n", ret);
        fprintf (output_stream, "\n");
        for (li = list_first (c->u.semi_comma.cmd); list_next (li); li = list_next (li))
        {
          int s;
  	      b = print_chp_stmt ((chp_lang_t *)list_value (li), bitwidth, base_var);
          s = stmt_count++;
          if (c->type == CHP_SEMI)
          {
             fprintf (output_stream, "  syn_seq s_%d(c_%d);\n", s, a);
          }
          else
          {
             fprintf (output_stream, "  syn_par s_%d(c_%d);\n", s, a);
          }
          fprintf (output_stream, "  s_%d.s1 = c_%d;\n", s, b);
          if (!list_next (list_next (li)))
          {
             /* if this is the last loop iteration */
             fprintf (output_stream, "\n");
             b = print_chp_stmt ((chp_lang_t *)list_value (list_next (li)), bitwidth, base_var);
             fprintf (output_stream, "  s_%d.s2 = c_%d;\n", s, b);
          }
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
      ret = print_gc ((c->type == CHP_LOOP) ? 1 : 0, c->u.gc, bitwidth, base_var);
      break;

    default:
      fprintf (stderr, "Error: unsupported token: %d\n", c->type);
      exit (1);
      break;
  }
  return ret;
}

void print_chp_structure (Chp *c)
{
  int i;
  if (output_file)
  {
    char *output_path = calloc (MAX_PATH_SIZE, sizeof(char));
    snprintf (output_path, MAX_PATH_SIZE, "/home/user/Documents/ADCO/act/tst/%s", output_file);
    output_stream = fopen (output_path, "w");
    free (output_path);
  }
  else
  {
    output_stream = stdout;
  }
  fprintf (output_stream, "import \"~/Documents/ADCO/act/syn.act\";\n");
  if (bundle_data) fprintf (output_stream, "import \"~/Documents/ADCO/act/bundled.act\";\n");
  fprintf (output_stream, "\n");
  fprintf (output_stream, "defproc toplevel (a1of1 go)\n{\n");
  print_vars (c);
  int *bitwidth = calloc (1, sizeof (int));
  int *base_var = calloc (1, sizeof (int));
  if (optimization > 0) evaluated_exprs = hash_new (INITIAL_HASH_SIZE);
  i = print_chp_stmt (c->c, bitwidth, base_var);
  free (bitwidth);
  free(base_var);
  if (optimization > 0)
  {
    hash_free (evaluated_exprs);
  }
  fprintf (output_stream, "  go = c_%d;\n", i);
  fprintf (output_stream, "}\n\n");
  fprintf (output_stream, "toplevel t;\n");
  if (output_file) fclose (output_stream);
}
