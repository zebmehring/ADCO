#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include "chp.h"
#include "list.h"
#include "hash.h"

extern Chp *__chp;

int get_bitwidth_expr (Expr *e);

void check_types_cmd (chp_lang_t *c);

void check_types (Chp *c);
