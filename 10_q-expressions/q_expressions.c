#include <stdio.h>
#include <stdlib.h>
#include "mpc.h"

#ifdef _WIN32

#include <string.h>

static char buffer[2048];

char* readline(char* prompt) {
  fputs(prompt, stdout);
  fgets(buffer, 2048, stdin);
  char* cpy = malloc(strlen(buffer)+1);
  strcpy(cpy, buffer);
  cpy[strlen(cpy)-1] = '\0';
  return cpy;
}

void add_history(char* unused) {}

#else

#include <editline/readline.h>
//#include <editline/history.h>

#endif

typedef struct lval {
  int type; // 型
  long num; // 値

  char* err; // エラー文字列
  char* sym; // シンボル名

  int count; // 子要素の数
  struct lval** cell; // 子要素の配列
} lval;

enum { LVAL_ERR, LVAL_NUM, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR };
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

// コンストラクタ--->

// 数値型lvalの作成。
lval* lval_num(long x) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->num = x;
  return v;
}

// エラー型lvalの作成。
lval* lval_err(char *m) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_ERR;
  v->err = malloc(strlen(m) + 1);
  strcpy(v->err, m);
  return v;
}

// シンボル型lvalの作成。
lval* lval_sym(char *s) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SYM;
  v->sym = malloc(strlen(s) + 1);
  strcpy(v->sym, s);
  return v;
}

// S式型lvalの作成。
lval* lval_sexpr(void) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

lval* lval_qexpr(void) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_QEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

// lvalに子要素を追加する。
lval* lval_add(lval* v, lval* x) {
  // 要素数を増やす。
  v->count++;
  // 増やした要素数にあわせて、ヒープを拡張。
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  // cellの末尾に新しいlvalを参照させる。
  v->cell[v->count-1] = x;
  return v;
}
// <--コンストラクタ

// デストラクタ
void lval_del(lval *v) {
  switch (v->type) {
  case LVAL_NUM: break;

  case LVAL_ERR: free(v->err); break;
  case LVAL_SYM: free(v->sym); break;

  // S式, Q式の場合は全ての子要素を解放する。
  case LVAL_QEXPR:
  case LVAL_SEXPR:
    for (int i = 0; i < v->count; i++) {
      lval_del(v->cell[i]);
    }
    free(v->cell);
    break;
  }
  // lval自体を破棄。
  free(v);
}

// 数値のバリデーション。
lval* lval_read_num(mpc_ast_t* t) {
  errno = 0;
  long x = strtol(t->contents, NULL, 10);
  return errno != ERANGE ? lval_num(x) : lval_err("invalid number");
}

// 抽象構文木を読み込み、S式を作成する。
lval* lval_read(mpc_ast_t* t) {
  if (strstr(t->tag, "number")) { return lval_read_num(t); }
  if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

  lval* x = NULL;
  if (strcmp(t->tag, ">") == 0) { x = lval_sexpr(); }
  if (strstr(t->tag, "sexpr")) { x = lval_sexpr(); }
  if (strstr(t->tag, "qexpr")) { x = lval_qexpr(); }

  for (int i = 0; i < t->children_num; i++) {
    if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
    if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
    if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
    if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
    if (strcmp(t->children[i]->tag,  "regex") == 0) { continue; }
    x = lval_add(x, lval_read(t->children[i]));
  }
  return x;
}

// lval_printとlval_expr_printはお互いに呼び合うので、前方宣言する。
void lval_print(lval *v);

// 開始と終了の文字を指定して、式をプリントする。
void lval_expr_print(lval* v, char open, char close) {
  putchar(open);
  for (int i = 0; i < v->count; i++ ) {
    lval_print(v->cell[i]);
  
    if (i != (v->count-1)) {
      putchar(' ');
    }
  }
  putchar(close);
}

// lvalをプリントする。
void lval_print(lval* v) {
  switch (v->type) {
  case LVAL_NUM: printf("%li", v->num); break;
  case LVAL_ERR: printf("Error: %s", v->err); break;
  case LVAL_SYM: printf("%s", v->sym); break;
  case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
  case LVAL_QEXPR: lval_expr_print(v, '{', '}'); break;
  }
}

// 改行付きlval_print
void lval_println(lval* v) { lval_print(v); putchar('\n'); }  

// lvalの子要素から、特定のインデックスのものを抜き出し、その箇所に後続の要素を配置する。
// A->B->C => pop B => A->C
lval* lval_pop(lval* v, int i) {
  lval* x = v->cell[i];

  memmove(&v->cell[i], &v->cell[i+1], sizeof(lval*) * (v->count-i-1));

  v->count--;

  v->cell = realloc(v->cell, sizeof(lval *) * v->count);
  return x;
}

// 2つのlvalをjoinする。
lval* lval_join(lval* x, lval* y) {
  // yが空になるまで、yを先頭からpopしてxに追加し続ける。
  while (y->count) {
    x = lval_add(x, lval_pop(y, 0));
  }

  // yを削除。
  lval_del(y);
  return x;
}

// lvalの子要素から、特定のインデックスのものを取得し残りを削除する。
lval* lval_take(lval* v, int i) {
  lval* x = lval_pop(v, i);
  lval_del(v);
  return x;
}

// Assertマクロ。
#define LASSERT(args, cond, err) if (!(cond)) { lval_del(args); return lval_err(err); }

// lval_evalとlval_eval_sexprは互いに呼び合うので、前方宣言する。
lval* lval_eval(lval* v);

lval* builtin(lval* a, char* func);

// S式を評価。
lval* lval_eval_sexpr(lval* v) {
  for (int i = 0; i < v->count; i++) {
    v->cell[i] = lval_eval(v->cell[i]);
  }

  for (int i = 0; i < v->count; i++) {
    // エラーと評価されたものがあれば、残りの評価をせずにエラーを返す。
    if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
  }

  if (v->count == 0) { return v; }

  if (v->count == 1) { return lval_take(v, 0); }

  // lvalから先頭要素を取得。
  // このとき、もとのlvalからは先頭要素が削除されていることに注意。
  lval* f = lval_pop(v, 0);
  
  // S式の先頭要素がシンボルでない場合はエラー。
  if (f->type != LVAL_SYM) {
    lval_del(f); lval_del(v);
    return lval_err("S-expression Does not start with symbol!");
  }

  // lvalから演算の対象を抜き出したものとシンボルを渡して、計算結果を取得する。
  lval* result = builtin(v, f->sym);

  // fを破棄。
  lval_del(f);
  return result;
}

// lvalを評価。
lval* lval_eval(lval* v) {
  if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(v); }
  return v;
}

// 組み込み関数head
lval* builtin_head(lval* a) {
  // 引数が1つであること。
  LASSERT(a, (a->count == 1), "Function 'head' passed too many arguments!");
  // 型がリストであること。
  LASSERT(a, (a->cell[0]->type == LVAL_QEXPR), "Function 'head' passed incorrect types!");
  // 空リストでないこと。
  LASSERT(a, (a->cell[0]->count != 0), "Function 'head' passed {}!");

  lval* v = lval_take(a, 0);

  // 先頭要素以外を削除。要素がひとつになるまでリストの2番目を削除していく。
  while (v->count > 1) { lval_del(lval_pop(v, 1)); }
  return v;
}

// 組み込み関数tail
lval* builtin_tail(lval* a) {
  LASSERT(a, (a->count == 1), "Function 'tail' passed too many arguments!");
  LASSERT(a, (a->cell[0]->type == LVAL_QEXPR), "Function 'tail' passed incorrect types!");
  LASSERT(a, (a->cell[0]->count != 0), "Function 'tail' passed {}!");

  lval* v = lval_take(a, 0);

  lval_del(lval_pop(v, 0));
  return v;
}

// 組み込み関数list
lval* builtin_list(lval* a) {
  // S式をリストに変換する。
  a->type = LVAL_QEXPR;
  return a;
}

lval* builtin_eval(lval* a) {
  LASSERT(a, (a->count == 1), "Function 'eval' passed too many arguments!");
  LASSERT(a, (a->cell[0]->type == LVAL_QEXPR), "Function 'eval' passed incorrect type!");

  lval* x = lval_take(a, 0);
  // リストをS式に変換し評価。
  x->type = LVAL_SEXPR;
  return lval_eval(x);
}

lval* builtin_join(lval* a) {
  for (int i = 0; i < a->count; i++) {
    LASSERT(a, (a->cell[i]->type == LVAL_QEXPR), "Function 'join' passed incorrect type.");
  }

  lval* x = lval_pop(a, 0);
  
  while (a->count) {
    x= lval_join(x, lval_pop(a, 0));
  }

  lval_del(a);
  return x;
}

// オペランドのみが含まれたlvalとオペレータから、計算済みのlvalを返す。
// (1 2), '+' => 3 
lval* builtin_op(lval* a, char* op) {
  // オペランドが数値のみかチェック。
  for (int i = 0; i < a->count; i++) {
    if (a->cell[i]->type != LVAL_NUM) {
      lval_del(a);
      return lval_err("Cannot operate on non-number!");
    }
  }

  // 一つ目のオペランド。
  lval* x = lval_pop(a, 0);

  // オペランドが1つで、オペレータがマイナスのとき、オペランドを負数にしたものが計算結果。
  // (- 3) => -3
  if ((strcmp(op, "-") == 0) && a->count == 0) { x->num = -x->num; }

  while (a->count > 0) {
    // 2つ目のオペランド。
    lval* y = lval_pop(a, 0);

    if (strcmp(op, "+") == 0) { x->num += y->num; }
    if (strcmp(op, "-") == 0) { x->num -= y->num; }
    if (strcmp(op, "*") == 0) { x->num *= y->num; }
    if (strcmp(op, "/") == 0) {
      // 0除算をチェック。
      if (y->num == 0) {
        lval_del(x); lval_del(y);
        x = lval_err("Division By Zero!"); break;
      }   
      x->num /= y->num;
    }
    lval_del(y);    
  }
  lval_del(a);
  
  return x;
}

lval* builtin(lval* a, char* func) {
  if (strcmp("list", func) == 0) { return builtin_list(a); }
  if (strcmp("head", func) == 0) { return builtin_head(a); }
  if (strcmp("tail", func) == 0) { return builtin_tail(a); }
  if (strcmp("join", func) == 0) { return builtin_join(a); }
  if (strcmp("eval", func) == 0) { return builtin_eval(a); }
  if (strstr("+-/*", func)) { return builtin_op(a, func); }
  lval_del(a);
  return lval_err("Unknown Function!");
}

int main(int argc, char** argv) {
  mpc_parser_t* Number = mpc_new("number");
  mpc_parser_t* Symbol = mpc_new("symbol");
  mpc_parser_t* Sexpr  = mpc_new("sexpr");
  mpc_parser_t *Qexpr  = mpc_new("qexpr");
  mpc_parser_t* Expr   = mpc_new("expr");
  mpc_parser_t* Lispy  = mpc_new("lispy");

  mpca_lang(MPCA_LANG_DEFAULT,
            "                                   \
number   : /-?[0-9]+/ ;                                                 \
symbol   : \"list\" | \"head\" | \"tail\" | \"join\" | \"eval\" | '+' | '-' | '*' | '/' ; \
sexpr    : '(' <expr>* ')' ;                                            \
qexpr    : '{' <expr>* '}' ;                                            \
expr     : <number> | <symbol> | <sexpr> | <qexpr> ;                    \
lispy    : /^/ <expr>* /$/ ;                                            \
",
            Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

  puts("Lispy Version 0.0.0.0.1");
  puts("Press Ctrl+c to Exit\n");

  while (1) {
    char* input = readline("lispy> ");
    add_history(input);
    mpc_result_t r;

    if (mpc_parse("<stdin>", input, Lispy, &r)) {
      lval* x = lval_eval(lval_read(r.output));
      lval_println(x);
      lval_del(x);
    } else {
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }
    free(input);
  }

  mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
  
  return 0;
}
