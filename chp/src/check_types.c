#include "check_types.h"

/*
 * int get_bitwidth_expr (Expr *e)
 *
 * Summary:
 *
 *    Returns the bitwidth of the expression represented by e. If e is a
 *    primitive variable, the bitwidth of the variable is returned. If e is a
 *    primitive integer, 0 is returned as a special value and the integer is
 *    eventually truncated by the compiler to the width of the expression to
 *    which it is a part. If e is a complex expression, the bitwidth of the
 *    result is returned, or -1 is returned if the complex expression contains
 *    a sub-expression with incompatible bit widths or other errors.
 *
 *    In recursively exploring e, get_bitwidth_expr() also checks for the
 *    following errors: undeclared variables, undelcared channels, improper
 *    use of operators (i.e. E_TRUE/E_FALSE), improper use of functions, and
 *    incompatible bit widths of expressions. Any of the following result in
 *    the function immediately returning -1.
 *
 * Parameters: e - an Expr object containing the expression
 *
 * Return Value: the bitwidth of the expression; 0 if the expression can be
 *               arbitrarily truncated; -1 if any sub-expression has
 *               incompatible bitwidths
 */
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
      left_bitwidth = get_bitwidth_expr (e->u.e.l);
      right_bitwidth = get_bitwidth_expr (e->u.e.r);
      // if the right expression is an integer, the bitwidth is determined by the left expression
      if (left_bitwidth == 0)
      {
        return right_bitwidth;
      }
      // if the left expression is an integer, the bitwidth is determined by the right expression
      else if (right_bitwidth == 0)
      {
        return left_bitwidth;
      }
      // do a basic check for division by zero
      else if ((e->type == E_DIV) && (e->u.e.r->type == E_INT) && (e->u.e.r->u.v == 0))
      {
        fprintf (stderr, "Error: Attempted division by 0\n");
        exit (-1);
      }
      // if the bitwidths of both expressions match, return that value
      else
      {
        return left_bitwidth == right_bitwidth ? left_bitwidth : -1;
      }

    case E_NOT:
    case E_COMPLEMENT:
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
      // comparison operators produce a boolean result
      return (left_bitwidth == 0) || (right_bitwidth == 0) || (left_bitwidth == right_bitwidth) ? 1 : -1;

    case E_INT:
      // base case: integers can be arbitrarily expanded/truncated (as in C)
      return 0;

    case E_VAR:
      s = find_symbol (__chp, (char *) e->u.e.l);
      // ensure variable declaration
      if (!s)
      {
        fprintf (stderr, "Error: Symbol not found: %s\n", (char *) e->u.e.l);
        exit (-1);
      }
      // ensure _valid_ variable declaration
      else if (s->bitwidth < 1)
      {
        fprintf (stderr, "Error: Invalid variable bitwidth (%d): %s\n", s->bitwidth, (char *)s->name);
        exit (-1);
      }
      // base case: bitwidth for an expression is determined by its constituent variables
      return s->bitwidth;

    case E_TRUE:
    case E_FALSE:
      left_bitwidth = get_bitwidth_expr (e->u.e.l);
      // E_TRUE/E_FALSE are only valid operations on boolean variables
      return left_bitwidth == 1 ? 1 : -1;

    case E_PROBE:
      s = find_symbol (__chp, (char *) e->u.e.l);
      // ensure the probed channel exists
      if (!s)
      {
        fprintf (stderr, "Error: Symbol not found: %s\n", (char *) e->u.e.l);
        exit (-1);
      }
      // ensure the probed channel is a channel
      return s->ischan ? 1 : -1;

    case E_FUNCTION:
      // ensure the function is of the form <name>_<bitwidth>
      t = e->u.fn.s + strlen (e->u.fn.s) - 1;
      while (t > e->u.fn.s)
      {
        if (!isdigit (*t))
        {
          if (*t != '_')
          {
            fprintf (stderr, "Error: functions must be of the form <name>_<bitwidth>\n");
            exit (1);
          }
          ret = atoi (t+1);
          break;
        }
        t--;
      }
      if (t == e->u.fn.s)
      {
        fprintf (stderr, "Error: functions must be of the form <name>_<bitwidth>\n");
        exit (1);
      }
      // ensure the funciton only takes variable parameters
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
      fprintf (stderr, "Error: Unsupported token: %d\n", e->type);
      exit (-1);
  }
  return -1;
}

/*
 * void check_types_cmd (chp_lang_t *c)
 *
 * Summary:
 *
 *    Does some basic error checks on the CHP statement represented by c. The
 *    included checks are: assignment to valid variables, communication over
 *    valid channels, boolean-valued guards, and matching bitwidths for composite
 *    expressions.
 *
 * Parameters: c - a structure representing the CHP program
 */
void check_types_cmd (chp_lang_t *c)
{
  int expr_width;
  symbol *s;

  switch (c->type)
  {
    case CHP_ASSIGN:
      s = find_symbol (__chp, c->u.assign.id);
      // ensure assigned expression exists
      if (s == NULL)
      {
        fprintf (stderr, "check_types error: Symbol not found: %s\n", c->u.assign.id);
        exit (-1);
      }
      // ensure assigned expression is a variable
      else if (s->ischan)
      {
        fprintf (stderr, "check_types error: Attempted assignment to channel: %s\n", c->u.assign.id);
        exit (-1);
      }
      expr_width = get_bitwidth_expr (c->u.assign.e);
      // ensure correct sub-expressions on the RHS
      if (expr_width == -1)
      {
        fprintf (stderr, "check_types error: Expression operands have incompatible bit widths\n");
        exit (-1);
      }
      // ensure assigned variable and assignment expression have identical bitwidths
      else if (expr_width != s->bitwidth && expr_width != 0)
      {
        fprintf (stderr, "check_types error: Assignment variable (%s) and expression have incompatible bit widths\n", c->u.assign.id);
        exit (-1);
      }
      break;

    case CHP_SEND:
    case CHP_RECV:
      s = find_symbol (__chp, c->u.comm.chan);
      // ensure channel exists
      if (s == NULL)
      {
        fprintf (stderr, "check_types error: Channel not found: %s\n", c->u.comm.chan);
        exit (-1);
      }
      // ensure channel is a channel
      else if (!s->ischan)
      {
        fprintf (stderr, "check_types error: Channel expression expected: %s\n", c->u.comm.chan);
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
            // ensure correct sub-expressions in the send buffer
            if (expr_width == -1)
            {
              fprintf (stderr, "check_types error: Expression operands have incompatible bit widths\n");
              exit (-1);
            }
            // ensure the send buffer and channel have the same width
            else if (expr_width != s->bitwidth && expr_width != 0)
            {
              fprintf (stderr, "check_types error: Channel and expression have incompatible bit widths\n");
              exit (-1);
            }
          }
          else
          {
            char *name = list_value (li);
            symbol *ls = find_symbol (__chp, name);
            // ensure valid receiving variable
            if (!ls)
            {
            	fprintf (stderr, "check_types error: variable %s not found\n", name);
            	exit (1);
            }
            // ensure channel bitwidth is equal to the variable bitwidth
            if (ls->bitwidth != s->bitwidth)
            {
              fprintf (stderr, "check_types error: Receiving variable has insufficient width: %s\n", name);
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
        if (!gc)
        {
          fprintf (stderr, "check_types error: empty loop/selection statment\n");
          exit (-1);
        }
        while (gc)
        {
          // ensure guards are boolean-valued
          if (gc->g && (get_bitwidth_expr (gc->g) != 1))
          {
            fprintf (stderr, "check_types error: Boolean guard expected\n");
            exit (-1);
          }
          // recursively check guarded statements
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

/*
 * void check_types (Chp *c)
 *
 * Summary:
 *
 *    Does some basic error checks on the CHP program represented by c. Assumes
 *    that all "interacting" variables are of identical bitwidth. Composite
 *    multi-bit expressions adopt the bitwidth of their constituent variables.
 *    The following elements of the CHP program are verified:
 *        - all utilized variables and channels are declared
 *        - all binary expression operands have equal bitwidths
 *        - probes are only taken of channel expressions
 *        - functions conform to <name>_<bitwidth>
 *        - assignment is only done to variable expressions
 *        - sends and receives happen over channels
 *        - sends and receives only happen over channels with bitwidth equal
 *          to the data sent/received
 *        - the send buffer is a non-channel expression
 *        - the receive buffer is a variable
 *        - guards are boolean-valued
 *
 * Parameters: c - a structure representing the CHP program
 */
void check_types (Chp *c)
{
  if (!c || !c->c)
  {
    fprintf (stderr, "check_types error: No CHP parsed\n");
    exit (-1);
  }
  check_types_cmd (c->c);
}
