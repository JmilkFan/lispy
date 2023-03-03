/*******
 * Lispy Values 核心数据类型处理模块。
 *  提供 lval 结构体，构造函数，析构函数，用户输入数据存储与打印等功能。
 */
#ifndef lvalues_h
#define lvalues_h

#include "mpc.h"

#include "lenv.h"
#include "lbuiltins.h"


/* 头文件循环嵌套，前置声明。*/
#ifndef predefinition
#define predefinition
struct lenv_s;
typedef struct lenv_s lenv_t;
struct lval_s;
typedef struct lval_s lval_t;
typedef lval_t *(*lbuiltin)(lenv_t*, lval_t*);  // 路由器函数指针类型 
#endif


/* Lispy Values 用户输入数据存储器数据结构。*/
struct lval_s
{
    int      type;  // 用户输入的数据类型标记

    /* Basic */
    long     num;   // 操作数
    char     *sym;  // 操作符号
    char     *err;  // 错误处理信息
    char     *str;  // 字符串

    /* Function */
    lbuiltin builtin;       // 操作函数指针
    struct lenv_s *env;     // 函数运行时环境
    struct lval_s *formals; // 函数参数列表
    struct lval_s *body;    // 函数运算结果

    /* Expression */
    int      count;         // 子节点数量
    struct lval_s **cell;   // 子节点，指针数组类型 
};

/* Lispy Values 用户输入数据的类型。*/
enum ltypes
{
    LVAL_NUM,   // 数字类型
    LVAL_SYM,   // 符号类型
    LVAL_STR,   // 字符串类型
    LVAL_SEXPR, // S-Expression 类型
    LVAL_QEXPR, // Q-Expression 类型
    LVAL_FUN,   // 函数类型
    LVAL_ERR,   // 错误类型
};

char *ltype_name(int t);


/* 构造函数 */
lval_t *lval_num(long x);
lval_t *lval_sym(char *s);
lval_t *lval_sexpr(void);
lval_t *lval_str(char *s);
lval_t *lval_qexpr(void);
lval_t *lval_fun(lbuiltin func);
lval_t *lval_err(char *fmt, ...);
lval_t *lval_lambda(lval_t *formals, lval_t *body);

/* 析构函数 */
void lval_del(lval_t *v);

/* 用户输入读取与存储函数 */
lval_t *lval_read(mpc_ast_t *t);

/* 运算结果打印函数 */
void lval_print(lval_t *v);
void lval_println(lval_t *v);

/* 将子节点追加到父节点的指针数组中。*/
lval_t *lval_add(lval_t *parent, lval_t *children);

#endif