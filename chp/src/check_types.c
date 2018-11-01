#include "check_types.h"

int get_bitwidth_expr (Expr *e)
{
  int left_bitwidth, right_bitwidth;
  symbol *s;

  switch (e->type)
  {
    case E_AND:
    case E_OR:
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
        fprintf (stderr, "Attempted division by 0\n");
        exit (-1);
      }
      else
      {
        return left_bitwidth == right_bitwidth ? left_bitwidth : -1;
      }

    case E_NOT:
    case E_UMINUS:
      return get_bitwidth_expr (e->u.e.l);

    case E_INT:
      return 0;

    case E_VAR:
      s = find_symbol (__chp, (char *) e->u.e.l);
      if (!s)
      {
        fprintf (stderr, "Symbol not found: %s\n", (char *) e->u.e.l);
        exit (-1);
      }
      else if (s->bitwidth < 1)
      {
        fprintf (stderr, "Invalid variable bitwidth (%d): %s\n", s->bitwidth, (char *)s->name);
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
        fprintf (stderr, "Symbol not found: %s\n", (char *) e->u.e.l);
        exit (-1);
      }
      return s->ischan ? 1 : -1;

    default:
      fprintf (stderr, "Unknown token: %d\n", e->type);
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
        fprintf (stderr, "Symbol not found: %s\n", c->u.assign.id);
        exit (-1);
      }
      else if (s->ischan)
      {
        fprintf (stderr, "Attempted assignment to channel: %s\n", c->u.assign.id);
        exit (-1);
      }
      expr_width = get_bitwidth_expr (c->u.assign.e);
      if (expr_width == -1)
      {
        fprintf (stderr, "Expression operands have incompatible bit widths\n");
        exit (-1);
      }
      else if (expr_width != s->bitwidth && expr_width != 0)
      {
        fprintf (stderr, "Assignment variable (%s) and expression have incompatible bit widths\n", c->u.assign.id);
        exit (-1);
      }
      break;

    case CHP_SEND:
    case CHP_RECV:
      s = find_symbol (__chp, c->u.comm.chan);
      if (s == NULL)
      {
        fprintf (stderr, "Symbol not found: %s\n", c->u.comm.chan);
        exit (-1);
      }
      else if (!s->ischan)
      {
        fprintf (stderr, "Channel expression expected: %s\n", c->u.comm.chan);
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
              fprintf (stderr, "Expression operands have incompatible bit widths\n");
              exit (-1);
            }
            else if (expr_width != s->bitwidth && expr_width != 0)
            {
              fprintf (stderr, "Channel and expression have incompatible bit widths\n");
              exit (-1);
            }
          }
          else
          {
            char *name = list_value (li);
            symbol *ls = find_symbol (__chp, name);
            if (ls->bitwidth != s->bitwidth)
            {
              fprintf (stderr, "Receiving variable has insufficient width: %s\n", name);
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
            fprintf (stderr, "Boolean guard expected\n");
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
