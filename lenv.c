#include <stdlib.h>
#include <string.h>

#include "lenv.h"


/**
 * 构造函数。
 */
lenv_t *lenv_init(void)
{
    lenv_t *e = malloc(sizeof(lenv_t));
    e->par = NULL;
    e->count = 0;
    e->syms = NULL;
    e->vals = NULL;
    return e;
}

/**
 * 析构函数。
 */
void lenv_del(lenv_t *e)
{
    for (int i=0; i < e->count; i++)
    {
        free(e->syms[i]);
        lval_del(e->vals[i]);
    }
    free(e->syms);
    free(e->vals);
    free(e);
}


/**
 * 变量访问函数：
 * 
 * 1、获取交互环境的变量数据，返回 lval 类型。
 *  如果符号已经在 Lenv 中，则以 Lval 类型返回。
 *  如果符号还没在 Lenv 中，则返回错误。
 * 
 * 2、检查是否关联了父环境，如果是，则从父环境中继续检索变量。
 */
lval_t *lenv_get(lenv_t *e, lval_t *v)
{
    for (int i=0; i < e->count; i++)
    {
        if (0 == strcmp(e->syms[i], v->sym))
        {
            return lval_copy(e->vals[i]);
        }
    }

    if (e->par)
    {
        return lenv_get(e->par, v);
    }
    else
    {
        return lval_err("Unbound symbol '%s'", v->sym);
    }
}

/**
 * 本地变量设置函数：添加或更新交互环境变量的数据。
 *  如果符号还没在 Lenv 中，则添加新数据。 
 *  如果符号已经在 Lenv 中，则删除旧数据，录入新数据。
 */
void lenv_put(lenv_t *e, lval_t *k, lval_t *v)
{
    for (int i=0; i < e->count; i++)
    {
        if (0 == strcmp(e->syms[i], k->sym))
        {
            lval_del(e->vals[i]);
            e->vals[i] = lval_copy(v);
            return;
        }
    }

    e->count++;
    e->vals = realloc(e->vals, sizeof(lval_t *) * e->count);
    e->syms = realloc(e->syms, sizeof(char *) * e->count);

    e->vals[e->count - 1] = lval_copy(v);

    e->syms[e->count - 1] = malloc(strlen(k->sym) + 1);
    strcpy(e->syms[e->count - 1], k->sym);
}

/**
 * 全局变量设置函数：添加或更新交互环境变量的数据。
 *  如果符号还没在 Parent Lenv 中，则添加新数据。 
 *  如果符号已经在 Parent Lenv 中，则删除旧数据，录入新数据。
 */
void lenv_def(lenv_t *e, lval_t *k, lval_t *v)
{
    while (e->par)
    {
        e = e->par;
    }
    lenv_put(e, k, v);
}


/**
 * 将 Lenv-vals 拷贝到 Lval-cells。
 */
lval_t *lval_copy(lval_t *e_val)
{
    lval_t *l_val = malloc(sizeof(lval_t));

    l_val->type = e_val->type;
    switch (e_val->type)
    {
        case LVAL_FUN:
            if (e_val->builtin)
            {
                l_val->builtin = e_val->builtin;
            }
            else
            {
                l_val->builtin = NULL;
                l_val->env = lenv_copy(e_val->env);
                l_val->formals = lval_copy(e_val->formals);
                l_val->body = lval_copy(e_val->body);
            }
            break;
            
        case LVAL_NUM: l_val->num = e_val->num; break;

        case LVAL_ERR:
            l_val->err = malloc(strlen(e_val->err) + 1);
            strcpy(l_val->err, e_val->err);
            break;
        case LVAL_SYM:
            l_val->sym = malloc(strlen(e_val->sym) + 1);
            strcpy(l_val->sym, e_val->sym);
            break;
        case LVAL_STR:
            l_val->str = malloc(strlen(e_val->str) + 1);
            strcpy(l_val->str, e_val->str);
            break;
        
        case LVAL_SEXPR:
        case LVAL_QEXPR:
            l_val->count = e_val->count;
            l_val->cell = malloc(sizeof(lval_t *) * l_val->count);
            for (int i=0; i < l_val->count; i++)
            {
                l_val->cell[i] = lval_copy(e_val->cell[i]);  // 遍历所有子节点
            }
            break;
    }
    return l_val;
}


/**
 * 将 Lenv-vals 拷贝到 Lenv-vals。
 */
lenv_t *lenv_copy(lenv_t *e)
{
    lenv_t *n = malloc(sizeof(lenv_t));
    n->par = e->par;
    n->count = e->count;
    n->syms = malloc(sizeof(char *) * n->count);
    n->vals = malloc(sizeof(lval_t *) * n->count);

    for (int i=0; i < e->count; i++)
    {
        n->syms[i] = malloc(strlen(e->syms[i]) + 1);
        strcpy(n->syms[i], e->syms[i]);

        n->vals[i] = lval_copy(e->vals[i]);
    }

    return n;
}