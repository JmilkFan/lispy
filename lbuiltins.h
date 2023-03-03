/*******
 * Lispy Builtins 内建函数模块，提供多种函数功能，并通过函数路由器进行映射。
 */
#ifndef lbuiltins_h
#define lbuiltins_h

#include "lvalues.h"
#include "lenv.h"
#include "lassert.h"


/* 头文件循环嵌套，前置声明。*/
#ifndef predefinition
#define predefinition
struct lenv_s;
typedef struct lenv_s lenv_t;
struct lval_s;
typedef struct lval_s lval_t;
typedef lval_t *(*lbuiltin)(lenv_t*, lval_t*);  // 路由器函数指针类型 
#endif


/* 符号表达式处理函数 */
lval_t *lval_eval(lenv_t *e, lval_t *v);

/* 源文件加载函数 */
lval_t *builtin_load(lenv_t *e, lval_t *a);

/* 函数路由器 */
void lenv_add_builtins(lenv_t *e);

#endif