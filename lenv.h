/*******
 * Lispy Environment 交互环境变量模块。
 *  提供 lenv 结构体，构造函数，析构函数，变量数据获取与存储等功能。
 */
#ifndef lenv_h
#define lenv_h

#include "lvalues.h"

/* 头文件循环嵌套，前置声明。*/
#ifndef predefinition
#define predefinition
struct lenv_s;
typedef struct lenv_s lenv_t;
struct lval_s;
typedef struct lval_s lval_t;
typedef lval_t *(*lbuiltin)(lenv_t*, lval_t*);  // 路由器函数指针类型 
#endif

struct lenv_s
{
    struct lenv_s *par;   // 父环境变量空间
    int    count;         // 变量数目
    char   **syms;        // 变量名列表，类型为指针数组
    struct lval_s **vals; // 变量值列表，类型为指针数组
};


/* 构造函数 */
lenv_t *lenv_init(void);

/* 析构函数 */
void lenv_del(lenv_t *e);

/* 交互环境变量获取接口 */
lval_t *lenv_get(lenv_t *e, lval_t *k);

/* 交互环境变量设置接口 */
void lenv_def(lenv_t *e, lval_t *k, lval_t *v);
void lenv_put(lenv_t *e, lval_t *k, lval_t *v);

/* 结构体数据拷贝接口 */
lval_t *lval_copy(lval_t *e_val);
lenv_t *lenv_copy(lenv_t *e);

#endif