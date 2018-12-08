#include "check_types.h"

extern int expr_count;
extern int stmt_count;
extern int chan_count;
extern Chp *__chp;
extern int gc_chan_count;
extern bool bundle_data;
extern int optimization;
extern char *output_file;
extern struct Hashtable *evaluated_exprs;

void print_vars (Chp *c);

void emit_const_1 (void);
void emit_const_0 (void);

int get_bitwidth (int n);
int get_func_bitwidth (char *s);
int get_max_bits (const char *s, int lbits, int rbits);
void get_expr (Expr *e, int v, char *buf);

void hash_add_expr (struct Hashtable *h, const char *expr);
void hash_remove_expr (struct Hashtable *h, const char *expr);
void _hash_remove_expr (struct Hashtable *h, const char *expr, hash_bucket_t *b);
int hash_get_or_add (struct Hashtable *h, const char *s, Expr *l, Expr *r, int nl, int nr, bool commutative);

int unop (const char *s, Expr *e, int *bitwidth, int *base_var, int *delay);
int binop (const char *s, Expr *e, int *bitwidth, int *base_var, int *delay, bool commutative);
int _print_expr (Expr *e, int *bitwidth, int *base_var, int *delay);
int print_expr (Expr *e, int *bitwidth, int *base_var, int *delay);

int print_expr_tmpvar (char *req, int ego, int eout, int bits);

int print_one_gc (chp_gc_t *gc, int *bitwidth, int *base_var);
int print_gc (bool loop, chp_gc_t *gc, int *bitwidth, int *base_var);

int print_chp_stmt (chp_lang_t *c, int *bitwidth, int *base_var);

void print_chp_structure (Chp *c);
