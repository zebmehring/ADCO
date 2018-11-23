#include "check_types.h"

extern int expr_count;
extern int stmt_count;
extern int chan_count;
extern Chp *__chp;
extern int gc_chan_count;
extern bool bundle_data;

void print_vars (Chp *c);

void emit_const_1 (void);

void emit_const_0 (void);

int get_bitwidth (int n);

int get_func_bitwidth (char *s);

int get_max_bits (char *s, int lbits, int rbits);

int unop (char *s, Expr *e, int *bitwidth, int *base_var);

int binop (char *s, Expr *e, int *bitwidth, int *base_var, bool comp_op);

int _print_expr (Expr *e, int *bitwidth, int *base_var);

int print_expr (Expr *e, int *bitwidth, int *base_var);

int print_expr_tmpvar (char *req, int ego, int eout, int bits);

int print_one_gc (chp_gc_t *gc, int *bitwidth, int *base_var);

int print_gc (int loop, chp_gc_t *gc, int *bitwidth, int *base_var);

int print_chp_stmt (chp_lang_t *c, int *bitwidth, int *base_var);

void print_chp_structure (Chp *c);
