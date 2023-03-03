#include "lbuiltins.h"

extern mpc_parser_t* Lispy;

static lval_t *lval_eval_sexpr(lenv_t *e, lval_t *v);
static lval_t *lval_pop(lval_t *v, int i);
static lval_t *lval_take(lval_t *v, int i);

lval_t *builtin_eval(lenv_t *e, lval_t *v);
lval_t *builtin_list(lenv_t *e, lval_t *v);

/**
 * 运算处理入口。
 */
lval_t *lval_eval(lenv_t *e, lval_t *v)
{
    if (LVAL_SYM == v->type)        // 符号类型处理分支
    {
        lval_t *x = lenv_get(e, v); // 如果符号在 Lenv 中，则直接返回。
        lval_del(v);
        return x;
    }

    if (LVAL_SEXPR == v->type)  // S-Expr 类型处理分支
    {
        return lval_eval_sexpr(e, v);
    }

    return v;
}

/**
 * 函数调用分发入口，区分内置函数和自定义函数。
 *  1、如果是内置函数，直接调用即可。
 *  2、如果是自定义函数，还需要将传入的每个实际参数都绑定到 formals 字段，然后再计算 body 字段。
 *     此时，还需要使用到 env 字段来作为函数调用的局部运算环境。
 */
lval_t *lval_call(lenv_t *e, lval_t *f, lval_t *a) {

    /* 如果是内置函数，则直接调用。*/
    if (f->builtin) {
        return f->builtin(e, a);
    }
    /* 如果是自定义函数，则经过下列处理。*/

    /* Record Argument Counts */
    int given = a->count;
    int total = f->formals->count;

    /* While arguments still remain to be processed */
    while (a->count) {
        /* If we've ran out of formal arguments to bind */
        if (0 == f->formals->count) {
            lval_del(a);
            return lval_err("Function passed too many arguments. "
                    "Got %i, Expected %i.", given, total);
        }

        /* 弹出第一个函数参数。*/
        lval_t *sym = lval_pop(f->formals, 0);

        /* 检索符号字符串中是否存在 & 可变长形参标识符。*/
        if (0 == strcmp(sym->sym, "&")) {

             /* 检查 & 标识符后是否只跟着 1 个符号，如果不是，则抛出一个错误。*/
            if (f->formals->count != 1) {
                lval_del(a);
                return lval_err("Function format invalid. "
                                "Symbol '&' not followed by single symbol.");
            }

            /* 下一个 formal 函数参数存储 & 标识符后的若干个可变长的参数列表。*/
            lval_t *nsym = lval_pop(f->formals, 0);
            lenv_put(f->env, nsym, builtin_list(e, a));
            lval_del(sym); lval_del(nsym);
            break;
        }

        /* Pop the next argument from the list */
        lval_t *val = lval_pop(a, 0);

        /* Bind a copy into the function's environment */
        lenv_put(f->env, sym, val);

        /* Delete symbol and value */
        lval_del(sym); lval_del(val);
    }

    /* Argument list is now bound so can be cleaned up */
    lval_del(a);

    /* If '&' remains in formal list bind to empty list */
    if (f->formals->count > 0 && strcmp(f->formals->cell[0]->sym, "&") == 0) {
        /* Check to ensure that & is not passed invalidly. */
        if (f->formals->count != 2) {
            return lval_err("Function format invalid. "
                            "Symbol '&' not followed by single symbol.");
        }

        /* Pop and delete '&' symbol */
        lval_del(lval_pop(f->formals, 0));

        /* Pop next symbol and create empty list */
        lval_t *sym = lval_pop(f->formals, 0);
        lval_t *val = lval_qexpr();

        /* Bind to environment and delete */
        lenv_put(f->env, sym, val);
        lval_del(sym); lval_del(val);
    }

    /* If all formals have been bound evaluate */
    if (f->formals->count == 0) {

        /* Set environment parent to evaluation environment */
        f->env->par = e;

        /* Evaluate and return */
        return builtin_eval(f->env, lval_add(lval_sexpr(),
                            lval_copy(f->body)));
    } else {
        /* Otherwise return partially evaluated function */
        return lval_copy(f);
    }
}

/**
 * S-Expression 处理函数。
 *  只有 S-Expr 具有运算处理逻辑，而 Q-Expr 不具有。
 */
static lval_t *lval_eval_sexpr(lenv_t *e, lval_t *v)
{
    /* 遍历子节点，自地向上进行处理。*/
    for (int i=0; i < v->count; i++)
    {
        v->cell[i] = lval_eval(e, v->cell[i]);
    }

    /* 将所有 Err 类型节点取走。*/
    for (int i=0; i < v->count; i++)
    {
        if (LVAL_ERR == v->cell[i]->type)
        { 
            return lval_take(v, i);
        }
    }

    if (0 == v->count ) { return v; }               // 没有子节点的直接返回。
    if (1 == v->count ) { return lval_take(v, 0); } // 只有一个子节点的直接返回子节点并删除。

    /* 具有 2 个及以上子节点的函数处理流程。*/
    lval_t *f = lval_pop(v, 0);

    if (LVAL_FUN != f->type)    // 第一个节点应该是函数名类型，否者无法对后续的多个操作数或嵌套表达式节点进行处理。
    {
        lval_t *err = lval_err("S-Expression starts with incorrect type. "
                               "Got %s, Expected %s.",
                               ltype_name(f->type), ltype_name(LVAL_FUN));
        lval_del(f);
        lval_del(v);
        return err;
    }

    /* 根据相应的函数来处理后续的操作数或表达式。具体的操作函数在表达式读取阶段完成函数路由器的映射 */
    lval_t *result = lval_call(e, f, v);
    lval_del(f);
    return result;
}

/**
 * 根据指定的 idx，弹出一个子节点，然后将其后面的子节点向前移动填补空缺。
 * NOTE：取出子节点后，不会删除父节点。
 */
static lval_t *lval_pop(lval_t *v, int i)
{
    lval_t *x = v->cell[i];

    memmove(&v->cell[i], &v->cell[i+1], sizeof(lval_t *) * (v->count-i-1));  // 内存数据向前移动一个单元
    v->count--;
    v->cell = realloc(v->cell, sizeof(lval_t *) * v->count);
    return x;
}

/**
 * 根据指定的 idx，弹出一个 Lval 节点，然后删除父节点及其所有子节点。
 */
static lval_t *lval_take(lval_t *v, int i)
{
    lval_t *x = lval_pop(v, i);
    lval_del(v);
    return x;
}

/**
 * 算术运算函数集。
 *  支持 + - * / 运算。
 */
lval_t *builtin_op(lenv_t *e, lval_t *v, char *op)
{
    for (int i=0; i < v->count; i++)
    {
        LASSERT_TYPE(op, v, i, LVAL_NUM);
    }

    lval_t *x = lval_pop(v, 0);

    if (0 == (strcmp(op, "")) && (0 == v->count))
    {
        x->num = -x->num;
    }

    while (v->count > 0)
    {
        lval_t *y = lval_pop(v, 0);

        if (strcmp(op, "+") == 0) { x->num += y->num; }
        if (strcmp(op, "-") == 0) { x->num -= y->num; }
        if (strcmp(op, "*") == 0) { x->num *= y->num; }
        if (strcmp(op, "/") == 0)
        {
            if (y->num == 0) {
                lval_del(x);
                lval_del(y);
                x = lval_err("Division By Zero!"); break;
            }
            x->num /= y->num;
        }

        lval_del(y);
    }

    lval_del(v);
    return x;
}

lval_t *builtin_add(lenv_t *e, lval_t *v) { return builtin_op(e, v, "+"); }
lval_t *builtin_sub(lenv_t *e, lval_t *v) { return builtin_op(e, v, "-"); }
lval_t *builtin_mul(lenv_t *e, lval_t *v) { return builtin_op(e, v, "*"); }
lval_t *builtin_div(lenv_t *e, lval_t *v) { return builtin_op(e, v, "/"); }


/**
 * 引用表达式函数集。 
 *  支持 head、tail、join、list、eval 等引用表达式字符串处理。
 */
lval_t *builtin_head(lenv_t *e, lval_t *v)
{
    LASSERT_NUM("head", v, 1);
    LASSERT_TYPE("head", v, 0, LVAL_QEXPR);
    LASSERT_NOT_EMPTY("head", v, 0);

    lval_t *x = lval_take(v, 0);
    while (x->count > 1)
    {
        lval_del(lval_pop(x, 1));
    }
    return x;
}

lval_t *builtin_tail(lenv_t *e, lval_t *v)
{
    LASSERT_NUM("tail", v, 1);
    LASSERT_TYPE("tail", v, 0, LVAL_QEXPR);
    LASSERT_NOT_EMPTY("tail", v, 0);

    lval_t *x = lval_take(v, 0);
    lval_del(lval_pop(x, 0));
    return x;
}

static lval_t *lval_join(lval_t *x, lval_t *y)
{
    while (y->count)
    {
        x = lval_add(x, lval_pop(y, 0));
    }

    lval_del(y);
    return x;
}

lval_t *builtin_join(lenv_t *e, lval_t *v)
{
    for (int i=0; i < v->count; i++)
    {
        LASSERT_TYPE("join", v, i, LVAL_QEXPR);
    }

    lval_t *x = lval_pop(v, 0);
    while (v->count)
    {
        x = lval_join(x, lval_pop(v, 0));
    }

    lval_del(v);
    return x;
}

lval_t *builtin_list(lenv_t *e, lval_t *v)
{
    v->type = LVAL_QEXPR;
    return v;
}

lval_t *builtin_eval(lenv_t *e, lval_t *v)
{
    LASSERT_NUM("eval", v, 1);
    LASSERT_TYPE("eval", v, 0, LVAL_QEXPR);

    lval_t *x = lval_take(v, 0);
    x->type = LVAL_SEXPR;
    return lval_eval(e, x);
}

/**
 * 变量赋值表达式函数集。 
 *  使用 Q-Expression 作为右值表达式，左值函数名为 def（全局变量）或 =（局部变量）。
 */
lval_t *builtin_var(lenv_t *e, lval_t *v, char *func)
{
    LASSERT_TYPE(func, v, 0, LVAL_QEXPR);

    lval_t *syms = v->cell[0];

    for (int i=0; i < syms->count; i++)
    {
        LASSERT(v, (LVAL_SYM == syms->cell[i]->type),
            "Function '%s' cannot define non-symbol. "
            "Got %s, Expected %s.", func,
            ltype_name(syms->cell[i]->type),
            ltype_name(LVAL_SYM));
    }

    LASSERT(v, (syms->count == v->count-1),
        "Function '%s' passed too many arguments for symbols. "
        "Got %i, Expected %i.", func, syms->count, v->count-1);

    for (int i=0; i < syms->count; i++)
    {
        if (0 == strcmp(func, "def"))
        {
            lenv_def(e, syms->cell[i], v->cell[i+1]);
        }

        if (0 == strcmp(func, "="))
        {
            lenv_put(e, syms->cell[i], v->cell[i+1]);
        }
    }
    lval_del(v);
    return lval_sexpr();
}

lval_t *builtin_def(lenv_t *e, lval_t *v) { return builtin_var(e, v, "def"); }
lval_t *builtin_put(lenv_t *e, lval_t *v) { return builtin_var(e, v, "="); }


/**
 * Lambda 表达式函数。 
 */
lval_t *builtin_lambda(lenv_t *e, lval_t *v)
{
    LASSERT_NUM("\\", v, 2);
    LASSERT_TYPE("\\", v, 0, LVAL_QEXPR);
    LASSERT_TYPE("\\", v, 1, LVAL_QEXPR);

    for (int i=0; i < v->cell[0]->count; i++)
    {
        LASSERT(v, (v->cell[0]->cell[i]->type == LVAL_SYM),
            "Cannot define non-symbol. Got %s, Expected %s.",
            ltype_name(v->cell[0]->cell[i]->type),ltype_name(LVAL_SYM));
    }

    lval_t *formals = lval_pop(v, 0);
    lval_t *body = lval_pop(v, 0);
    lval_del(v);

    return lval_lambda(formals, body);
}


/**
 * 大小比较函数。
 *  比较两个 Number 类型数据，并返回 0（False）或 1（True）结果。
 */
lval_t *builtin_ord(lenv_t *e, lval_t *v, char *op)
{
    LASSERT_NUM(op, v, 2);
    LASSERT_TYPE(op, v, 0, LVAL_NUM);
    LASSERT_TYPE(op, v, 1, LVAL_NUM);

    int rst;
    if (0 == strcmp(op, ">")) { rst = (v->cell[0]->num > v->cell[1]->num); }
    if (0 == strcmp(op, "<")) { rst = (v->cell[0]->num < v->cell[1]->num); }
    if (0 == strcmp(op, ">=")) { rst = (v->cell[0]->num >= v->cell[1]->num); }
    if (0 == strcmp(op, "<=")) { rst = (v->cell[0]->num <= v->cell[1]->num); }

    lval_del(v);
    return lval_num(rst);
}

lval_t *builtin_gt(lenv_t *e, lval_t *v) { return builtin_ord(e, v, ">"); }
lval_t *builtin_lt(lenv_t *e, lval_t *v) { return builtin_ord(e, v, "<"); }
lval_t *builtin_ge(lenv_t *e, lval_t *v) { return builtin_ord(e, v, ">="); }
lval_t *builtin_le(lenv_t *e, lval_t *v) { return builtin_ord(e, v, "<="); }


/**
 * 等于比较函数。
 *  比较两个 lval 数据的所有字段是否都相等。
 *  将比较结果存储到一个新的 lval 中并返回它。
 */
int lval_eq(lval_t *x, lval_t *y)
{
    /* Different Types are always unequal */
    if (x->type != y->type) { return 0; }

    switch (x->type)
    {
        case LVAL_NUM: return (x->num == y->num);
        case LVAL_ERR: return (0 == strcmp(x->err, y->err));
        case LVAL_SYM: return (0 == strcmp(x->sym, y->sym));
        case LVAL_STR: return (0 == strcmp(x->str, y->str));
        case LVAL_FUN:
            if (x->builtin || y->builtin)
            {
                return x->builtin == y->builtin;  // 内建函数比较
            }
            else
            {
                return lval_eq(x->formals, y->formals) && lval_eq(x->body, y->body);  // 自定义 Lambda 函数比较
            }
        case LVAL_QEXPR:
        case LVAL_SEXPR:
            if (x->count != y->count) { return 0; }
            for (int i=0; i < x->count; i++)
            {
                if (0 == lval_eq(x->cell[i], y->cell[i])) { return 0; }
            }
            return 1;
        break;
    }
    return 0;
}

lval_t *builtin_cmp(lenv_t *e, lval_t *v, char *op)
{
    LASSERT_NUM(op, v, 2);
    int rst;
    if (0 == strcmp(op, "==")) { rst = lval_eq(v->cell[0], v->cell[1]); }
    if (0 == strcmp(op, "!=")) { rst = !lval_eq(v->cell[0], v->cell[1]); }

    lval_del(v);
    return lval_num(rst);
}

lval_t *builtin_eq(lenv_t *e, lval_t *v) { return builtin_cmp(e, v, "=="); }
lval_t *builtin_ne(lenv_t *e, lval_t *v) { return builtin_cmp(e, v, "!="); }

/**
 * if 关键字函数。
 *  语法规则类型与 C 语言中的三目运算符。
 *  分别使用 2 个 Q-Expression 来表示真或假的逻辑代码。
 */
lval_t *builtin_if(lenv_t *e, lval_t *a)
{
    LASSERT_NUM("if", a, 3);
    LASSERT_TYPE("if", a, 0, LVAL_NUM);
    LASSERT_TYPE("if", a, 1, LVAL_QEXPR);
    LASSERT_TYPE("if", a, 2, LVAL_QEXPR);


    /* Mark Both Expressions as evaluable */
    a->cell[1]->type = LVAL_SEXPR;
    a->cell[2]->type = LVAL_SEXPR;

    lval_t *x;

    if (a->cell[0]->num)
    {
        /* If condition is true evaluate first expression */
        x = lval_eval(e, lval_pop(a, 1));
    }
    else
    {
        /* Otherwise evaluate second expression */
        x = lval_eval(e, lval_pop(a, 2));
    }

    lval_del(a);
    return x;
}

/**
 * 源文件加载函数。
 */
lval_t *builtin_load(lenv_t *e, lval_t *a)
{
    LASSERT_NUM("load", a, 1);
    LASSERT_TYPE("load", a, 0, LVAL_STR);

    /* Parse File given by string name */
    mpc_result_t r;
    if (mpc_parse_contents(a->cell[0]->str, Lispy, &r))
    {
        /* Read contents */
        lval_t *expr = lval_read(r.output);
        mpc_ast_delete(r.output);

        while (expr->count)
        {
            lval_t *x = lval_eval(e, lval_pop(expr, 0));

            /* If Evaluation leads to error print it */
            if (LVAL_ERR == x->type) { lval_println(x); }
            lval_del(x);
        }

        lval_del(expr);
        lval_del(a);

        /* Return empty list */
        return lval_sexpr();
    }
    else
    {
         /* Get Parse Error as String */
        char *err_msg = mpc_err_string(r.error);
        mpc_err_delete(r.error);

        /* Create new error message using it */
        lval_t *err = lval_err("Could not load file %s", err_msg);

        free(err_msg);
        lval_del(a);
        return err;
    }
}


/**
 * print 关键字函数
 * 	打印由空格分隔的每个参数，然后打印换行符。
 * 	函数返回空表达式。
 */
lval_t *builtin_print(lenv_t *e, lval_t *a)
{
  /* Print each argument followed by a space */
  for (int i=0; i < a->count; i++)
  {
    lval_print(a->cell[i]); putchar(' ');
  }

  /* Print a newline and delete arguments */
  putchar('\n');
  lval_del(a);
  return lval_sexpr();
}


/**
 * error 错误反馈函数
 * 	将用户提供的字符串作为输入，并将其提供给 lval_err 作为报错信息。
 */
lval_t *builtin_error(lenv_t *e, lval_t *a)
{
  LASSERT_NUM("error", a, 1);
  LASSERT_TYPE("error", a, 0, LVAL_STR);

  /* Construct Error from first argument */
  lval_t *err = lval_err(a->cell[0]->str);

  /* Delete arguments and return */
  lval_del(a);
  return err;
}


/**
 * 函数路由器注册函数。
 */
static void lenv_add_builtin(lenv_t *e, char *name, lbuiltin func)
{
    lval_t *k = lval_sym(name);
    lval_t *v = lval_fun(func);
    lenv_put(e, k, v);
    lval_del(k);
    lval_del(v);
}


/**
 * 函数路由器。
 */
void lenv_add_builtins(lenv_t *e)
{
    /* File load Functions */
    lenv_add_builtin(e, "load",  builtin_load);
    lenv_add_builtin(e, "error", builtin_error);
    lenv_add_builtin(e, "print", builtin_print);

    /* Comparison Functions */
    lenv_add_builtin(e, "if", builtin_if);
    lenv_add_builtin(e, "==", builtin_eq);
    lenv_add_builtin(e, "!=", builtin_ne);
    lenv_add_builtin(e, ">",  builtin_gt);
    lenv_add_builtin(e, "<",  builtin_lt);
    lenv_add_builtin(e, ">=", builtin_ge);
    lenv_add_builtin(e, "<=", builtin_le);

    /* Variable Functions */
    lenv_add_builtin(e, "def", builtin_def);
    lenv_add_builtin(e, "=", builtin_put);
    lenv_add_builtin(e, "\\",  builtin_lambda);

    /* Q-Expression Functions */
    lenv_add_builtin(e, "list", builtin_list);
    lenv_add_builtin(e, "head", builtin_head);
    lenv_add_builtin(e, "tail", builtin_tail);
    lenv_add_builtin(e, "eval", builtin_eval);
    lenv_add_builtin(e, "join", builtin_join);

    /* Mathematical Functions */
    lenv_add_builtin(e, "+", builtin_add);
    lenv_add_builtin(e, "-", builtin_sub);
    lenv_add_builtin(e, "*", builtin_mul);
    lenv_add_builtin(e, "/", builtin_div);
}