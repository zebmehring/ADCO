#include "cartographer.h"

int expr_count = 1;
int stmt_count = 0;
int chan_count = 0;

void print_vars (Chp *c)
{
  hash_bucket_t *b;
  int i;
  symbol *sym;

  if (!c || !c->sym) return;
  if (c->sym->n == 0) return;

  printf ("  /* --- declaring all variables and channels --- */\n");

  for (i = 0; i < c->sym->size; i++)
  {
    for (b = c->sym->head[i]; b; b = b->next)
    {
      sym = (symbol *) b->v;
      if (sym->ischan)
      {
      	if (sym->bitwidth > 1)
        {
      	  printf ("  aN1of2<%d> chan_%s;\n", sym->bitwidth, sym->name);
      	}
      	else
        {
      	  printf ("  a1of2 chan_%s;\n", sym->name);
      	}
      }
      else
      {
      	if (sym->bitwidth > 1)
        {
      	  printf ("  syn_var_init_false var_%s[%d];\n", sym->name, sym->bitwidth);
      	}
      	else
        {
      	  printf ("  syn_var_init_false var_%s;\n", sym->name);
      	}
      }
    }
  }
  printf ("  /* --- end of declarations --- */\n\n");
}

void emit_const_1 (void)
{
  static int emitted = 0;
  if (!emitted)
  {
    printf ("  syn_var_init_true const_1;\n");
  }
  emitted = 1;
}

void emit_const_0 (void)
{
  static int emitted = 0;
  if (!emitted)
  {
    printf ("  syn_var_init_false const_0;\n");
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

int get_max_bits (char *s, int lbits, int rbits)
{
  if (!strcmp (s, "bin_add") || !strcmp (s, "bin_sub"))
  {
    return (lbits > rbits) ? lbits + 1 : rbits + 1;
  }
  else if (!strcmp (s, "bin_mul"))
  {
    return lbits + rbits;
  }
  else if (!strcmp (s, "bin_div"))
  {
    return lbits > rbits ? lbits : rbits;
  }
  else
  {
    fprintf (stderr, "Error: unsupported binary operation\n");
    return -1;
  }
}

int base_var = -1;

int unop (char *s, Expr *e, int *bitwidth)
{
  int l;

  l = _print_expr (e->u.e.l, bitwidth);

  printf ("  %s e_%d(e_%d.out);\n", s, expr_count, l);
  return expr_count++;
}

int binop (char *s, Expr *e, int *bitwidth)
{
  int l, r;

  l = _print_expr (e->u.e.l, bitwidth);
  r = _print_expr (e->u.e.r, bitwidth);

  printf ("  %s e_%d(e_%d.out, e_%d.out);\n", s, expr_count, l, r);
  return expr_count++;
}

int arithmetic_binop (char *s, Expr *e, int *bitwidth)
{
  int l, r, lbits, rbits, abits, maxbits;

  abits = *bitwidth;
  l = _print_expr (e->u.e.l, bitwidth);
  lbits = *bitwidth;
  r = _print_expr (e->u.e.r, bitwidth);
  rbits = *bitwidth;
  maxbits = get_max_bits (s, lbits, rbits);

  printf ("  bundled_%s_%d e_%d;\n", s, abits, expr_count);

  if (lbits == 1)
  {
    printf ("  e_%d.lhs = e_%d.v;\n", expr_count, l);
  }
  else
  {
    printf ("  (i:%d: e_%d.lhs[i] = e_%d[i].v;)\n", lbits, expr_count, l);
  }
  if (rbits == 1)
  {
    printf ("  e_%d.rhs = e_%d.v;\n", expr_count, r);
  }
  else
  {
    printf ("  (i:%d: e_%d.rhs[i] = e_%d[i].v;)\n", rbits, expr_count, r);
  }

  if (base_var == -1)
  {
     base_var = expr_count;
  }
  else
  {
     printf ("  e_%d.go_r = e_%d.go.r;\n", expr_count, base_var);
  }
  return expr_count++;
}

Chp *__chp;

int func_bitwidth;

int _print_expr (Expr *e, int *bitwidth)
{
  int ret;
  switch (e->type)
  {
    case E_AND:
      ret = binop ("syn_expr_and", e, bitwidth);
      break;
    case E_OR:
      ret = binop ("syn_expr_or", e, bitwidth);
      break;
    case E_PLUS:
      ret = arithmetic_binop ("bin_add", e, bitwidth);
      break;
    case E_MINUS:
      ret = arithmetic_binop ("bin_sub", e, bitwidth);
      break;
    case E_MULT:
      ret = arithmetic_binop ("bin_mul", e, bitwidth);
      break;
    case E_DIV:
      ret = arithmetic_binop ("bin_div", e, bitwidth);
      break;
    case E_NOT:
    case E_COMPLEMENT:
      ret = unop ("syn_expr_not", e, bitwidth);
      break;
    case E_UMINUS:
      // TODO: need to discriminate single/multi-bit case
      ret = unop ("syn_expr_uminus", e, bitwidth);
      break;
    case E_PROBE:
      ret = 0;
      break;
    case E_VAR:
      {
        symbol *v = find_symbol (__chp, (char *)e->u.e.l);
        if (!v)
        {
        	fprintf (stderr, "Error: symbol '%s' not found\n", (char *)e->u.e.l);
        	exit (1);
        }
        if (v->bitwidth == 1)
        {
  	       printf ("  syn_expr_var e_%d(,var_%s.v);\n", expr_count, (char *)e->u.e.l);
        }
        else
        {
        	printf ("  syn_expr_vararray<%d> e_%d;\n", v->bitwidth, expr_count);
        	printf ("  (i:%d: e_%d[i].v = var_%s[i].v;)\n", v->bitwidth, expr_count, v->name);
        }
        if (base_var == -1)
        {
  	       base_var = expr_count;
        }
        else
        {
  	       printf ("  e_%d.go_r = e_%d.go_r;\n", expr_count, base_var);
        }
  	    *bitwidth = v->bitwidth;
        ret = expr_count++;
      }
      break;
    case E_INT:
      *bitwidth = get_bitwidth (e->u.v);
      if (e->u.v == 1)
      {
        emit_const_1 ();
        printf ("  syn_expr_var e_%d(,const_1.v);\n", expr_count);
      }
      else if (e->u.v == 0)
      {
        emit_const_0 ();
        printf ("  syn_expr_var e_%d(,const_0.v);\n", expr_count);
      }
      // TODO: figure out how to initialize arbitrary integers
      else
      {
        printf ("  /* multi-bit integers still in progress\n");
        printf ("   * syn_expr_vararray<%d> e_%d;\n", *bitwidth, expr_count);
        printf ("   * (i:%d: e_%d[i].v = );\n", *bitwidth, expr_count);
        printf ("   */\n");
      }
      if (base_var == -1)
      {
        base_var = expr_count;
      }
      else
      {
        printf ("  e_%d.go_r = e_%d.go_r;\n", expr_count, base_var);
      }
      ret = expr_count++;
      break;
    case E_TRUE:
      emit_const_1 ();
      printf ("  syn_expr_var e_%d(,const_1.v);\n", expr_count);
      if (base_var == -1)
      {
        base_var = expr_count;
      }
      else
      {
        printf ("  e_%d.go_r = e_%d.go_r;\n", expr_count, base_var);
      }
      ret = expr_count++;
      break;
    case E_FALSE:
      emit_const_0 ();
      printf ("  syn_expr_var e_%d(,const_0.v);\n", expr_count);
      if (base_var == -1)
      {
        base_var = expr_count;
      }
      else
      {
        printf ("  e_%d.go_r = e_%d.go_r;\n", expr_count, base_var);
      }
      ret = expr_count++;
      break;

    case E_FUNCTION:
      {
        Expr *tmp;
        int bits;
        char *t;

        if (func_bitwidth != -1)
        {
        	fprintf (stderr, "Error: only simple functions are supported!\n");
        	exit (1);
        }

        t = e->u.fn.s + strlen (e->u.fn.s) - 1;
        while (t > e->u.fn.s)
        {
        	if (!isdigit (*t))
          {
        	  if (*t != '_')
            {
        	    fprintf (stderr, "Error: functions must be of the form name_<bitwidth>\n");
        	    exit (1);
        	  }
        	  bits = atoi (t+1);
        	  break;
        	}
        	t--;
        }
        if (t == e->u.fn.s)
        {
        	fprintf (stderr, "Error: functions must be of the form name_<bitwidth>\n");
        	exit (1);
        }
        func_bitwidth = bits;
        *bitwidth = bits;
        tmp = e->u.fn.r;
        while (tmp)
        {
        	if (tmp->u.e.l->type != E_VAR)
          {
        	  printf ("-- can't handle this case --\n");
        	  break;
        	}
        	tmp = tmp->u.e.r;
        }
        if (tmp)
        {
  	       return -1;
        }
        /* we have a bundled-data function */
        printf ("  bundled_%s e_%d;\n", e->u.fn.s, expr_count);
        tmp = e->u.fn.r;
        while (tmp)
        {
        	symbol *v = find_symbol (__chp, (char *)tmp->u.e.l->u.e.l);
        	if (!v)
          {
        	  fprintf (stderr, "Error: no such variable '%s'\n", (char *)tmp->u.e.l->u.e.l);
        	  exit (1);
        	}
        	if (v->bitwidth == 1)
          {
        	  printf ("  e_%d.%s = var_%s.v;\n", expr_count, v->name, v->name);
        	}
        	else
          {
        	  printf ("  (i:%d: e_%d.%s[i] = var_%s[i].v;)\n", v->bitwidth, expr_count, v->name, v->name);
        	}
        	tmp = tmp->u.e.r;
        }
        if (base_var == -1)
        {
  	       base_var = expr_count;
        }
        else
        {
  	       printf ("  e_%d.go_r = e_%d.go.r;\n", expr_count, base_var);
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
                          output (func_bitwidth will be set to a value > 1)

  base_var is a global (ick) for the expression # for e_<num>.go_r
*/

int print_expr (Expr *e, int *bitwidth)
{
  base_var = -1;
  func_bitwidth = -1;
  return _print_expr (e, bitwidth);
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

  printf ("  syn_fullseq s_%d;\n", seq);
  printf ("  %s = s_%d.go.r;\n", req, seq);

  if (bits == 1)
  {
    printf ("  syn_recv rtv_%d;\n", seq);
    printf ("  syn_expr_var e_%d;\n", evar);
    printf ("  syn_var_init_false tv_%d(rtv_%d.v);\n", seq, seq);
    printf ("  e_%d.v = tv_%d.v;\n", evar, seq);
    printf ("  s_%d.r.r = e_%d.go_r;\n", seq, ego);
    printf ("  s_%d.r = rtv_%d.go;\n", seq, seq);
    printf ("  e_%d.out.t = rtv_%d.in.t;\n", eout, seq);
    printf ("  e_%d.out.f = rtv_%d.in.f;\n", eout, seq);
  }
  else
  {
    printf ("  syn_recv rtv_%d[%d];\n", seq, bits);
    printf ("  syn_expr_vararray<%d> e_%d;\n", bits, evar);
    printf ("  syn_var_init_false tv_%d[%d];\n", seq, bits);
    printf ("  (i:%d: e_%d[i].v = tv_%d[i].v; e_%d[i].v = rtv_%d[i].v;)\n", bits, evar, seq, evar, seq);
    printf ("  s_%d.r.r = e_%d.go_r;\n", seq, ego);
    printf ("  (i:%d: s_%d.r.r = rtv_%d[i].go.r;)\n", bits, seq, seq);
    printf ("  syn_ctree<%d> ct_%d;\n", bits, seq);
    printf ("  (i:%d: ct_%d.in[i] = rtv_%d[i].go.a;)\n", bits, seq, seq);
    printf ("  s_%d.r.a = ct_%d.out;\n", seq, seq);
    printf ("  (i:%d: e_%d.out[i].t = rtv_%d[i].in.t; e_%d.out[i].f = rtv_%d[i].in.f;)\n", bits, eout, seq, eout, seq);
  }
  printf ("  s_%d.go.a = e_%d.go_r;\n", seq, evar);

  return evar;
}

int gc_chan_count = 0;

/*
  Returns integer that corresponds to r1of2 channel that is used to
  evaluate the expression and return the Boolean result (t/f).
      t = guard evaluated to true and the statement was executed
      f = guard evaluated to false
*/
int print_one_gc (chp_gc_t *gc, int *bitwidth)
{
  int a, b;
  int ret = gc_chan_count++;
  if (!gc) return -1;

  printf ("  r1of2 gc_%d;\n", ret);
  if (gc->g)
  {
    int go_r;
    char buf[1024];
    a = print_expr (gc->g, bitwidth);
    go_r = base_var;

    sprintf (buf, "gc_%d.r", ret);

    /* replace "a"  with latched value */
    a = print_expr_tmpvar (buf, go_r, a, 1);

    b = print_chp_stmt (gc->s, bitwidth);

    if (b == -1)
    {
      /* empty */
      printf ("  gc_%d.t = e_%d.out.t;\n", ret, a);
    }
    else
    {
      printf ("  e_%d.out.t = c_%d.r;\n", a, b);
      printf ("  gc_%d.t = c_%d.a;\n", ret, b);
    }
    printf ("  gc_%d.f = e_%d.out.f;\n", ret, a);
  }
  else
  {
    b = print_chp_stmt (gc->s, bitwidth);
    printf ("  gc_%d.r = c_%d.r;\n", ret, b);
    printf ("  gc_%d.t = c_%d.a;\n", ret, b);
    //printf ("  prs { Reset|~Reset -> gc_%d.f- }\n", ret);
    printf ("  gc_%d.f = GND;\n", ret);
  }
  return ret;
}

/*
  prints loop/selection staetment, returns channel # for the "go"  command.
*/
int print_gc (int loop, chp_gc_t *gc, int *bitwidth)
{
  int start_gc_chan = gc_chan_count;
  int end_gc_chan;
  int ret, i;
  int na;
  static int gc_num = 0;
  int mygc;

  mygc = gc_num++;
  printf ("\n /*--- emit individual gc (#%d) [%s] ---*/\n", mygc, loop ? "loop"  : "selection");
  start_gc_chan = print_one_gc (gc, bitwidth);
  end_gc_chan = start_gc_chan;
  gc = gc->next;
  while (gc)
  {
    end_gc_chan = print_one_gc (gc, bitwidth);
    gc = gc->next;
  }

  ret = chan_count++;
  printf ("  a1of1 c_%d;\n", ret);

  printf ("  /* gc cascade, start = %d, end = %d */\n", start_gc_chan, end_gc_chan);

  for (i = start_gc_chan; i < end_gc_chan; i++)
  {
    /* gc cascade */
    printf ("  gc_%d.f = gc_%d.r;\n", i, i+1);
  }
  if (!loop)
  {
    printf ("  gc_%d.r = c_%d.r;\n", start_gc_chan, ret);
  }
  else
  {
    na = stmt_count++;
    printf ("  syn_notand na_%d;\n", na);
    printf ("  na_%d.x = c_%d.r;\n", na, ret);
    printf ("  na_%d.out = gc_%d.r;\n", na, start_gc_chan);
  }

  if (start_gc_chan == end_gc_chan)
  {
    /* only one guard */
    if (!loop)
    {
      printf ("  gc_%d.t = c_%d.a;\n", start_gc_chan, ret);
    }
    else
    {
      printf ("  gc_%d.t = na_%d.y;\n", start_gc_chan, na);
      printf ("  gc_%d.f = c_%d.a;\n", start_gc_chan, ret);
    }
  }
  else
  {
    int a, b;
    /* multi-stage or gate */
    a = stmt_count++;
    printf ("  syn_or2 or_%d(gc_%d.t,gc_%d.t);\n", a, start_gc_chan, start_gc_chan+1);
    for (i = start_gc_chan+2; i < end_gc_chan; i++)
    {
      b = stmt_count++;
      printf ("  syn_or2 or_%d(or_%d.out,gc_%d.t);\n", b, a, i);
      a = b;
    }
    if (!loop)
    {
      printf ("  or_%d.out = c_%d.a;\n", a, ret);
    }
    else
    {
      printf ("  or_%d.out = na_%d.y;\n", a, na);
      printf ("  gc_%d.f = c_%d.a;\n", end_gc_chan, ret);
    }
  }
  printf ("  /* ----- end of gc (#%d) ----- */\n\n", mygc);
  return ret;
}

/*
  Prints chp statement, returns channel # for "go"  command
*/
int print_chp_stmt (chp_lang_t *c, int *bitwidth)
{
  int ret;
  int a, b, go_r;
  symbol *v, *u;
  char buf[100];
  if (!c) return -1;
  switch (c->type)
  {
    case CHP_SKIP:
      printf ("  /* skip */");
      printf ("  syn_skip s_%d(c_%d);\n", stmt_count, chan_count);
      stmt_count++;
      ret = chan_count++;
      break;

    case CHP_ASSIGN:
      printf ("  /* assign */\n");
      v = find_symbol (__chp, c->u.assign.id);
      if (!v)
      {
        fprintf (stderr, "Error: variable %s not found\n", c->u.assign.id);
        exit (1);
      }
      *bitwidth = v->bitwidth;
      a = print_expr (c->u.assign.e, bitwidth);
      go_r = base_var;
      ret = chan_count++;
      b = stmt_count++;
      printf ("  a1of1 c_%d;\n", ret);
      sprintf (buf, "c_%d.r", ret);
      //printf ("  e_%d.go_r = c_%d.r;\n", go_r, ret);
      if (func_bitwidth == -1)
      {
        a = print_expr_tmpvar (buf, go_r, a, *bitwidth);
      }
      else
      {
        a = print_expr_tmpvar (buf, go_r, a, func_bitwidth);
      }
      if (func_bitwidth != v->bitwidth && func_bitwidth != -1)
      {
        fprintf (stderr, "Error: function bitwidth (%d) doesn't match variable bitwidth (%d)\n", func_bitwidth, v->bitwidth);
        exit (1);
      }
      if (v->bitwidth == 1)
      {
        printf ("  syn_recv s_%d(c_%d);\n", b, ret);
        printf ("  s_%d.in.t = e_%d.out.t;\n", b, a);
        printf ("  s_%d.in.f = e_%d.out.f;\n", b, a);
        printf ("  s_%d.v = var_%s.v;\n", b, c->u.assign.id);
      }
      else
      {
        printf ("  syn_recv s_%d[%d];\n", b, v->bitwidth);
        printf ("  (i:%d: s_%d[i].go.r = c_%d.r;)\n", v->bitwidth, b, ret);
        printf ("  syn_ctree<%d> ct_%d;\n", v->bitwidth, b);
        printf ("  (i:%d: ct_%d.in[i] = s_%d[i].go.a;)\n", v->bitwidth, b, b);
        printf ("  ct_%d.out = c_%d.a;\n", b, ret);
        printf ("  (i:%d: s_%d[i].in.t = e_%d.out[i].t;\n          s_%d[i].in.f = e_%d.out[i].f;\n          s_%d[i].v = var_%s[i].v; )\n", v->bitwidth, b, a, b, a, b, c->u.assign.id);
      }
      printf ("\n");
      break;

    case CHP_SEND:
      printf ("  /* send */\n");
      if (list_length (c->u.comm.rhs) == 1)
      {
        a = print_expr ((Expr *)list_value (list_first (c->u.comm.rhs)), bitwidth);
        go_r = base_var;
        ret = chan_count++;

        printf ("  a1of1 c_%d;\n", ret);

        sprintf (buf, "c_%d.r", ret);
        if (func_bitwidth == -1)
        {
  	       a = print_expr_tmpvar (buf, go_r, a, *bitwidth);
        }
        else
        {
          a = print_expr_tmpvar (buf, go_r, a, func_bitwidth);
        }

        //printf ("  c_%d.r = e_%d.go_r;\n", ret, base_var);
        printf ("  c_%d.a = chan_%s.a;\n", ret, c->u.comm.chan);
        v = find_symbol (__chp, c->u.comm.chan);
        if (!v)
        {
  	       fprintf (stderr, "Error: channel '%s' not found\n", c->u.comm.chan);
  	       exit (1);
        }
        if (v->bitwidth != func_bitwidth && func_bitwidth != -1)
        {
  	       fprintf (stderr, "Error: channel '%s' bitwidth (%d) doesn't match expression (%d)\n", c->u.comm.chan, v->bitwidth, func_bitwidth);
  	       exit (1);
        }
        if (func_bitwidth == 1 || *bitwidth == 1)
        {
  	       printf ("  chan_%s.t = e_%d.out.t;\n", c->u.comm.chan, a);
  	       printf ("  chan_%s.f = e_%d.out.f;\n", c->u.comm.chan, a);
        }
        else if (func_bitwidth != -1)
        {
          printf ("  (i:%d: chan_%s.d[i] = e_%d.out[i];)\n", func_bitwidth, v->name, a);
        }
        else
        {
  	       printf ("  (i:%d: chan_%s.d[i] = e_%d.out[i];)\n", *bitwidth, v->name, a);
        }
      }
      printf ("\n");
      break;

    case CHP_RECV:
      printf ("  /* recv */\n");
      if (list_length (c->u.comm.rhs) == 1)
      {
        ret = chan_count++;
        a = stmt_count++;
        printf ("  a1of1 c_%d;\n", ret);
        v = find_symbol (__chp, c->u.comm.chan);
        if (!v)
        {
        	fprintf (stderr, "Error: channel '%s' not found\n", c->u.comm.chan);
        	exit (1);
        }
        u = find_symbol (__chp, (char *)list_value (list_first (c->u.comm.rhs)));
        if (!u)
        {
        	fprintf (stderr, "Error: variable %s not found\n", (char *)list_value (list_first (c->u.comm.rhs)));
        	exit (1);
        }
        if (v->bitwidth != u->bitwidth)
        {
        	fprintf (stderr, "Error: channel '%s' bitwidth (%d) doesn't match expression (%d)\n", c->u.comm.chan, v->bitwidth, u->bitwidth);
        	exit (1);
        }
        if (v->bitwidth == 1)
        {
        	printf ("  syn_recv s_%d(c_%d);\n", a, ret);
        	printf ("  s_%d.in = chan_%s;\n", a, c->u.comm.chan);
        	printf ("  s_%d.v = var_%s.v;\n", a, u->name);
        }
        else
        {
        	printf ("  syn_recv s_%d[%d];\n", a, v->bitwidth);
        	printf ("  (i:%d: s_%d[i].go.r = c_%d.r;)\n", v->bitwidth, a, ret);
        	printf ("  syn_ctree<%d> ct_%d;\n", v->bitwidth, a);
        	printf ("  (i:%d: ct_%d.in[i] = s_%d[i].go.a;)\n", v->bitwidth, a, a);
        	printf ("  ct_%d.out = c_%d.a; c_%d.a = chan_%s.a;\n", a, ret, ret, v->name);
        	printf ("  (i:%d: s_%d[i].in.t = chan_%s.d[i].t;\n          s_%d[i].in.f = chan_%s.d[i].f;\n          s_%d[i].v = var_%s[i].v; )\n", v->bitwidth, a, v->name, a, v->name, a, u->name);
        }
      }
      printf ("\n");
      break;

    case CHP_COMMA:
    case CHP_SEMI:
      {
        listitem_t *li;
        if (list_length (c->u.semi_comma.cmd) == 1)
        {
          return print_chp_stmt ((chp_lang_t *)list_value (list_first (c->u.semi_comma.cmd)), bitwidth);
        }

        printf ("  /* %s */\n", c->type == CHP_COMMA ? "comma"  : "semicolon");
        a = chan_count++;
        ret = a;
        printf ("  a1of1 c_%d;\n", ret);
        for (li = list_first (c->u.semi_comma.cmd); list_next (li); li = list_next (li))
        {
          int s;
  	      b = print_chp_stmt ((chp_lang_t *)list_value (li), bitwidth);
          s = stmt_count++;
          if (c->type == CHP_SEMI)
          {
             printf ("  syn_seq s_%d(c_%d);\n", s, a);
          }
          else
          {
             printf ("  syn_par s_%d(c_%d);\n", s, a);
          }
          printf ("  s_%d.s1 = c_%d;\n", s, b);
          if (!list_next (list_next (li)))
          {
             /* if this is the last loop iteration */
             b = print_chp_stmt ((chp_lang_t *)list_value (list_next (li)), bitwidth);
             printf ("  s_%d.s2 = c_%d;\n", s, b);
          }
          else
          {
             printf ("  a1of1 c_%d;\n", chan_count);
             printf ("  s_%d.s2 = c_%d;\n", s, chan_count);
             a = chan_count++;
          }
        }
        printf ("\n");
      }
      break;

    case CHP_LOOP:
    case CHP_SELECT:
      ret = print_gc ((c->type == CHP_LOOP) ? 1 : 0, c->u.gc, bitwidth);
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
  printf ("import \"lab_syn.act\";\n\n");
  printf ("defproc toplevel (a1of1 go)\n{\n");
  print_vars (c);
  int *bitwidth = calloc (1, sizeof(int));
  i = print_chp_stmt (c->c, bitwidth);
  free (bitwidth);
  printf ("  go = c_%d;\n", i);
  printf ("}\n");
}