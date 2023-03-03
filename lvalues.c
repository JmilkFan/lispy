#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lvalues.h"
#include "lassert.h"

#define ERR_MSG_BUFFER 512  // 错误信息缓存长度


/* Lispy Values 用户输入数据类型 */
char *ltype_name(int t) {
    switch(t) {
        case LVAL_NUM:   return "Number";
        case LVAL_ERR:   return "Error";
        case LVAL_SYM:   return "Symbol";
        case LVAL_STR:   return "String";
        case LVAL_SEXPR: return "S-Expression";
        case LVAL_QEXPR: return "Q-Expression";
        case LVAL_FUN:   return "Function";
        default:         return "Unknown";
    }
}


/**
 * Lispy Values 用户输入数据存储器数据结构。
 */

lval_t *lval_num(long x) {
    lval_t* v = malloc(sizeof(lval_t));
    v->type = LVAL_NUM;
    v->num = x;
    return v;
}

lval_t *lval_err(char *fmt, ...)
{
    lval_t *v = malloc(sizeof(lval_t));
    v->type = LVAL_ERR;

    va_list va;
    va_start(va, fmt);

    v->err = malloc(ERR_MSG_BUFFER);
    vsnprintf(v->err, ERR_MSG_BUFFER-1, fmt, va);
    v->err = realloc(v->err, strlen(v->err) + 1);

    va_end(va);
    return v;
}

lval_t *lval_sym(char *s)
{
    lval_t *v = malloc(sizeof(lval_t));
    v->type = LVAL_SYM;
    v->sym = malloc(strlen(s) + 1);
    strcpy(v->sym, s);
    return v;
}

lval_t *lval_str(char *s)
{
    lval_t *v = malloc(sizeof(lval_t));
    v->type = LVAL_STR;
    v->str = malloc(strlen(s) + 1);
    strcpy(v->str, s);
    return v;
}

lval_t *lval_sexpr(void)
{
    lval_t *v = malloc(sizeof(lval_t));
    v->type = LVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

lval_t *lval_qexpr(void)
{
    lval_t *v = malloc(sizeof(lval_t));
    v->type = LVAL_QEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

lval_t *lval_fun(lbuiltin builtin)
{
    lval_t *v = malloc(sizeof(lval_t));
    v->type = LVAL_FUN;
    v->builtin = builtin;
    return v;
}

lval_t *lval_lambda(lval_t *formals, lval_t *body)
{
    lval_t *v = malloc(sizeof(lval_t));
    v->type = LVAL_FUN;
    v->builtin = NULL;
    v->env = lenv_init();
    v->formals = formals;
    v->body = body;
    return v;
}

void lval_del(lval_t *v)
{
    switch (v->type)
    {
        case LVAL_NUM: break;
        case LVAL_ERR: free(v->err); break;
        case LVAL_SYM: free(v->sym); break;
        case LVAL_STR: free(v->str); break;
        case LVAL_SEXPR:
        case LVAL_QEXPR:
            for (int i=0; i < v->count; i++)
            {
                lval_del(v->cell[i]);
            }
            free(v->cell);
            break;
        case LVAL_FUN:
            if (NULL == v->builtin)
            {
                lenv_del(v->env);
                lval_del(v->formals);
                lval_del(v->body);
            }
            break;
    }
    free(v);
}


/**
 * 将子节点追加到父节点的指针数组中。
 */
lval_t *lval_add(lval_t *parent, lval_t *children)
{
    parent->count++;
    parent->cell = realloc(parent->cell, sizeof(lval_t *) * parent->count);  // 父节点空间扩容
    parent->cell[parent->count-1] = children;
    return parent;
}

/**
 * 数字读取函数
 *  将 String 转换为 Long，并存储。
 */
static lval_t *lval_read_num(mpc_ast_t *t)
{
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return ERANGE != errno? lval_num(x): lval_err("Invalid number.");
}

/**
 * 字符串读取函数
 * 	首先，它必须剥离字符串两侧 `"` 字符。
 * 	然后，必须对转义字符串进行解码，将一系列转义字符（如 `\n`）转换成实际编码字符。
 * 	最后，必须创建一个新的 lval 并清理函数中使用过的内存。
 */
lval_t *lval_read_str(mpc_ast_t *ast)
{
    /* Cut off the final quote character */
    ast->contents[strlen(ast->contents) - 1] = '\0';

    /* Copy the string missing out the first quote character */
    char *unescaped = malloc(strlen(ast->contents + 1) + 1);
    strcpy(unescaped, ast->contents + 1);

    /* Pass through the unescape function */
    unescaped = mpcf_unescape(unescaped);

    /* Construct a new lval using the string */
    lval_t *str = lval_str(unescaped);

    /* Free the string and return */
    free(unescaped);
    return str;
}

/**
 * 接收 MPC AST，并根据树节点的 Tag 来读取相应的类型数据。
 */
lval_t *lval_read(mpc_ast_t *ast)
{   
    if (strstr(ast->tag, "number")) { return lval_read_num(ast); }      // 读取数据类型
    if (strstr(ast->tag, "symbol")) { return lval_sym(ast->contents); } // 读取符号类型
    if (strstr(ast->tag, "string")) { return lval_read_str(ast); }      // 读取字符串类型

    lval_t *parent = NULL;

    if (0 == strcmp(ast->tag, ">")) { parent = lval_sexpr(); }
    if (strstr(ast->tag, "sexpr"))  { parent = lval_sexpr(); }
    if (strstr(ast->tag, "qexpr"))  { parent = lval_qexpr(); }

    for (int i=0; i < ast->children_num; i++)
    {
        if (0 == strcmp(ast->children[i]->contents, "(")) { continue; }
        if (0 == strcmp(ast->children[i]->contents, ")")) { continue; }
        if (0 == strcmp(ast->children[i]->contents, "{")) { continue; }
        if (0 == strcmp(ast->children[i]->contents, "}")) { continue; }
        if (0 == strcmp(ast->children[i]->tag, "regex"))  { continue; }
        if (strstr(ast->children[i]->tag, "comment")) { continue; }

        parent = lval_add(parent, lval_read(ast->children[i]));  // 递归遍历 AST 树节点
    }

    return parent;
}


/**
 * 打印表达式。
 */
static void lval_expr_print(lval_t *v, char open_flag, char close_flag)
{
    putchar(open_flag);
    for (int i = 0; i < v->count; i++)
    {
        /* Print Value contained within */
        lval_print(v->cell[i]);

        /* Don't print trailing space if last element */
        if (i != (v->count-1))
        {
            putchar(' ');
        }
    }
    putchar(close_flag);
}

/**
 * 打印字符串。
 */
void lval_print_str(lval_t *v) {

    /* Make a Copy of the string */
    char *escaped = malloc(strlen(v->str) + 1);
    strcpy(escaped, v->str);

    /* Pass it through the escape function */
    escaped = mpcf_escape(escaped);

    /* Print it between " characters */
    printf("\"%s\"", escaped);

    /* free the copied string */
    free(escaped);
}

/**
 * 打印不同类型的数值。
 */
void lval_print(lval_t *v)
{
    switch (v->type)
    {
        case LVAL_NUM: printf("%li", v->num); break;
        case LVAL_ERR: printf("%s", v->err); break;
        case LVAL_SYM: printf("%s", v->sym); break;
        case LVAL_STR: lval_print_str(v); break;
        case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
        case LVAL_QEXPR: lval_expr_print(v, '{', '}'); break;
        case LVAL_FUN:
            if (v->builtin)
            {
                printf("<builtin>");
            }
            else
            {
                printf("(\\ "); lval_print(v->formals); putchar(' '); lval_print(v->body); putchar(')');
            }
            break;
    }
}

/**
 * 换行打印。
 */
void lval_println(lval_t *v)
{
    lval_print(v);
    putchar('\n');
}
