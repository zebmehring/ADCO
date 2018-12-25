#ifndef CHECK_TYPES_H
#define CHECK_TYPES_H

extern Chp *__chp;

int get_bitwidth_expr (Expr *e);

void check_types_cmd (chp_lang_t *c);

void check_types (Chp *c);

#endif
