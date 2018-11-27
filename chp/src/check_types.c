#include "check_types.h"

int get_bitwidth_expr (Expr *e)
{
  int left_bitwidth, right_bitwidth, ret;
  symbol *s;
  Expr *tmp;
  char *t;

  switch (e->type)
  {
    case E_AND:
    case E_OR:
    case E_XOR:
    case E_PLUS:
    case E_MINUS:
    case E_MULT:
    case E_DIV:
      left_bitwidth = get_bitwidth_expr (e->u.e.l);
      right_bitwidth = get_bitwidth_expr (e->u.e.r);
      if (left_bitwidth == 0)
      {
        return right_bitwidth;
      }
      else if (right_bitwidth == 0)
      {
        return left_bitwidth;
      }
      else if ((e->type == E_DIV) && (e->u.e.r->type == E_INT) && (e->u.e.r->u.v == 0))
      {
        fprintf (stderr, "Error: Attempted division by 0\n");
        exit (-1);
      }
      else
      {
        return left_bitwidth == right_bitwidth ? left_bitwidth : -1;
      }

    case E_NOT:
    case E_UMINUS:
      return get_bitwidth_expr (e->u.e.l);

    case E_EQ:
    case E_GT:
    case E_LT:
    case E_NE:
    case E_LE:
    case E_GE:
      left_bitwidth = get_bitwidth_expr (e->u.e.l);
      right_bitwidth = get_bitwidth_expr (e->u.e.r);
      return (left_bitwidth == 0) || (right_bitwidth == 0) || (left_bitwidth == right_bitwidth) ? 1 : -1;

    case E_INT:
      return 0;

    case E_VAR:
      s = find_symbol (__chp, (char *) e->u.e.l);
      if (!s)
      {
        fprintf (stderr, "Error: Symbol not found: %s\n", (char *) e->u.e.l);
        exit (-1);
      }
      else if (s->bitwidth < 1)
      {
        fprintf (stderr, "Error: Invalid variable bitwidth (%d): %s\n", s->bitwidth, (char *)s->name);
        exit (-1);
      }
      return s->bitwidth;

    case E_TRUE:
    case E_FALSE:
      left_bitwidth = get_bitwidth_expr (e->u.e.l);
      return left_bitwidth == 1 ? 1 : -1;

    case E_PROBE:
      s = find_symbol (__chp, (char *) e->u.e.l);
      if (!s)
      {
        fprintf (stderr, "Error: Symbol not found: %s\n", (char *) e->u.e.l);
        exit (-1);
      }
      return s->ischan ? 1 : -1;

    case E_FUNCTION:
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
          ret = atoi (t+1);
          break;
        }
        t--;
      }
      if (t == e->u.fn.s)
      {
        fprintf (stderr, "Error: functions must be of the form name_<bitwidth>\n");
        exit (1);
      }
      tmp = e->u.fn.r;
      while (tmp)
      {
        if (tmp->u.e.l->type != E_VAR)
        {
          fprintf (stderr, "Error: functions must take variable parameters\n");
          exit (-1);
        }
        symbol *v = find_symbol (__chp, (char *)tmp->u.e.l->u.e.l);
        if (!v)
        {
          fprintf (stderr, "Error: no such variable '%s'\n", (char *)tmp->u.e.l->u.e.l);
          exit (1);
        }
        tmp = tmp->u.e.r;
      }
      return ret;

    default:
      fprintf (stderr, "Error: Unknown token: %d\n", e->type);
      exit (-1);
  }
  return -1;
}

void check_types_cmd (chp_lang_t *c)
{
  int expr_width;
  symbol *s;

  switch (c->type)
  {
    case CHP_ASSIGN:
      s = find_symbol (__chp, c->u.assign.id);
      if (s == NULL)
      {
        fprintf (stderr, "Error: Symbol not found: %s\n", c->u.assign.id);
        exit (-1);
      }
      else if (s->ischan)
      {
        fprintf (stderr, "Error: Attempted assignment to channel: %s\n", c->u.assign.id);
        exit (-1);
      }
      expr_width = get_bitwidth_expr (c->u.assign.e);
      if (expr_width == -1)
      {
        fprintf (stderr, "Error: Expression operands have incompatible bit widths\n");
        exit (-1);
      }
      else if (expr_width != s->bitwidth && expr_width != 0)
      {
        fprintf (stderr, "Error: Assignment variable (%s) and expression have incompatible bit widths\n", c->u.assign.id);
        exit (-1);
      }
      break;

    case CHP_SEND:
    case CHP_RECV:
      s = find_symbol (__chp, c->u.comm.chan);
      if (s == NULL)
      {
        fprintf (stderr, "Error: Channel not found: %s\n", c->u.comm.chan);
        exit (-1);
      }
      else if (!s->ischan)
      {
        fprintf (stderr, "Error: Channel expression expected: %s\n", c->u.comm.chan);
        exit (-1);
      }
      else
      {
        listitem_t *li;
        for (li = list_first (c->u.comm.rhs); li; li = list_next (li))
        {
          if (c->type == CHP_SEND)
          {
            Expr *e = list_value (li);
            expr_width = get_bitwidth_expr (e);
            if (expr_width == -1)
            {
              fprintf (stderr, "Error: Expression operands have incompatible bit widths\n");
              exit (-1);
            }
            else if (expr_width != s->bitwidth && expr_width != 0)
            {
              fprintf (stderr, "Error: Channel and expression have incompatible bit widths\n");
              exit (-1);
            }
          }
          else
          {
            char *name = list_value (li);
            symbol *ls = find_symbol (__chp, name);
            if (!ls)
            {
            	fprintf (stderr, "Error: variable %s not found\n", name);
            	exit (1);
            }
            if (ls->bitwidth != s->bitwidth)
            {
              fprintf (stderr, "Error: Receiving variable has insufficient width: %s\n", name);
              exit (-1);
            }
          }
        }
      }
      break;

    case CHP_SEMI:
    case CHP_COMMA:
      {
        listitem_t *li;
        for (li = list_first (c->u.semi_comma.cmd); li; li = list_next (li))
        {
          chp_lang_t *cmd = list_value (li);
          check_types_cmd (cmd);
        }
      }
      break;

    case CHP_SELECT:
    case CHP_LOOP:
      {
        chp_gc_t *gc = c->u.gc;
        while (gc)
        {
          if (gc->g && (get_bitwidth_expr (gc->g) != 1))
          {
            fprintf (stderr, "Error: Boolean guard expected\n");
            exit (-1);
          }
          if (gc->s)
          {
            check_types_cmd (gc->s);
          }
          gc = gc->next;
        }
      }
      break;
    }
}

void check_types (Chp *c)
{
  check_types_cmd (c->c);
}
