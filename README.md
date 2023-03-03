# lispy

A lisp-like programming language.

# Use Guide

```bash
$ yum install readline-devel -y

$ gcc -g -std=c99 -Wall lispy.c mpc.c lvalues.c lenv.c lbuiltins.c -lreadline -lm -o lispy

$ ./lispy
Lispy Version 0.1
Press Ctrl+c to Exit

lispy> load "samples/hello.lspy"
"hello world."
```

# Documents & Blog

- [《用 C 语言开发一门编程语言》](https://blog.csdn.net/Jmilk/article/details/107193674)
