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

struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

typedef lval*(*lbuiltin)(lenv*, lval*);

struct lval {
  int type; // 型

  long num; // 値
  char* err; // エラー文字列(型がエラー)
  char* sym; // シンボル名(型がシンボル)

  lbuiltin builtin; // 組み込み関数(型が関数)。
  lenv* env; // ローカル環境。
  lval* formals; // 仮引数。
  lval* body; // 関数の実体。
  
  int count; // 子要素の数
  struct lval** cell; // 子要素の配列
};

struct lenv {
  lenv* par; // 外側の環境
  int count; // 定義された変数の数
  char** syms; // 変数名の配列
  lval** vals; // 値の配列
};

enum { LVAL_ERR, LVAL_NUM, LVAL_SYM, LVAL_FUN, LVAL_SEXPR, LVAL_QEXPR };
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

////////////////////////////////////////
// lval
////////////////////////////////////////

// 数値型lvalの作成。
lval* lval_num(long x) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->num = x;
  return v;
}

lval* lval_err(char *fmt, ...);

// 数値のバリデーションを行い、数値型のlvalを作成する。
lval* lval_read_num(mpc_ast_t* t) {
  errno = 0;
  long x = strtol(t->contents, NULL, 10);
  return errno != ERANGE ? lval_num(x) : lval_err("invalid number");
}

// エラー型lvalの作成。
lval* lval_err(char *fmt, ...) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_ERR;

  va_list va;
  va_start(va, fmt);

  v->err = malloc(512);

  vsnprintf(v->err, 511, fmt, va);

  v->err = realloc(v->err, strlen(v->err)+1);

  va_end(va);

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

// リスト型の作成。
lval* lval_qexpr(void) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_QEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

// ビルトイン関数の作成。
lval* lval_fun(lbuiltin func) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_FUN;
  v->builtin = func;
  return v;
}

lenv* lenv_new(void);

// ユーザー定義関数の作成。
lval* lval_lambda(lval* formals, lval* body) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_FUN;

  v->builtin = NULL;

  // ローカル環境。
  v->env = lenv_new();

  v->formals = formals;
  v->body = body;
  return v;
}

void lenv_del(lenv *e);

// lvalのデストラクタ
void lval_del(lval *v) {
  switch (v->type) {
  case LVAL_NUM: break;
  case LVAL_FUN:
    if (!v->builtin) {
      lenv_del(v->env);
      lval_del(v->formals);
      lval_del(v->body);
    }
    break;
    
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

lenv* lenv_copy(lenv* e);

// lvalをコピー。
lval* lval_copy(lval* v) {
  lval* x = malloc(sizeof(lval));
  x->type = v->type;

  switch (v->type) {
  case LVAL_FUN:
    if (v->builtin) { // 組み込み関数の場合。
      x->builtin = v->builtin;
    } else { // ユーザー定義関数の場合。
      x->builtin = NULL;
      x->env = lenv_copy(v->env);
      x->formals = lval_copy(v->formals);
      x->body = lval_copy(v->body);
    }
    break;
  case LVAL_NUM: x->num = v->num; break;

  case LVAL_ERR: x->err = malloc(strlen(v->err) + 1); strcpy(x->err, v->err); break;
  case LVAL_SYM: x->sym = malloc(strlen(v->sym) + 1); strcpy(x->sym, v->sym); break;

  case LVAL_SEXPR:
  case LVAL_QEXPR:
    x->count = v->count;
    x->cell = malloc(sizeof(lval*) * x->count);
    for (int i = 0; i < x->count; i++) {
      x->cell[i] = lval_copy(v->cell[i]);
    }
    break;
  }
  return x;
}

// 抽象構文木からlvalへマッピングする。
lval* lval_read(mpc_ast_t* t) {
  if (strstr(t->tag, "number")) { return lval_read_num(t); }
  if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

  lval* x = NULL;
  // ??
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
  case LVAL_FUN:
    if (v->builtin) {
      printf("<builtin>");
    } else {
      printf("(\\ "); lval_print(v->formals); putchar(' '); lval_print(v->body); putchar(')');
    }
    break;
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

// lvalの子要素から、特定のインデックスのものを取得し残りを削除する。
lval* lval_take(lval* v, int i) {
  lval* x = lval_pop(v, i);
  lval_del(v);
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

// Assertマクロ。
#define LASSERT(args, cond, fmt, ...)           \
  if (!(cond)) {                                \
    lval* err = lval_err(fmt, ##__VA_ARGS__);   \
    lval_del(args);                             \
    return err;                                 \
}

#define LASSERT_NUM(msg, val, cnt)          \
  LASSERT(val, (val->count == cnt), msg)    \

#define LASSERT_TYPE(msg, val, ind, aType)               \
  LASSERT(val, (val->cell[ind]->type == aType), msg)    \

// enumから型名。
char* ltype_name(int t) {
  switch (t) {
  case LVAL_FUN: return "Function";
  case LVAL_NUM: return "Number";
  case LVAL_ERR: return "Error";
  case LVAL_SYM: return "Symbol";
  case LVAL_SEXPR: return "S-Expression";
  case LVAL_QEXPR: return "Q-Expression";
  default: return "Unknown";
  }
}

lval* lval_eval(lenv* e, lval* v);
lval* builtin(lenv *e, lval* a, char* func);
lval* lval_call(lenv* e, lval* f, lval* a);

// S式を評価。
lval* lval_eval_sexpr(lenv* e, lval* v) {
  for (int i = 0; i < v->count; i++) {
    // 子要素のS式を再帰的に評価。
    v->cell[i] = lval_eval(e, v->cell[i]);
  }

  for (int i = 0; i < v->count; i++) {
    // エラーと評価されたものがあれば、残りの評価をせずにそのエラーを返す。
    if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
  }

  // 空のS式。
  if (v->count == 0) { return v; }

  // 要素が１つのS式は、それを取り出す。
  if (v->count == 1) { return lval_take(v, 0); }

  // lvalから先頭要素を取得。
  // このとき、もとのlvalからは先頭要素が削除されていることに注意。
  lval* f = lval_pop(v, 0);
  
  // S式の先頭要素は関数。
  if (f->type != LVAL_FUN) {
    return lval_err("S-Expression starts with incorrect type. Got %s, Expected %s.",
                    ltype_name(f->type), ltype_name(LVAL_FUN));
    lval_del(f); lval_del(v);
  }

  // 関数を実行し計算結果を取得。この時点でfは関数、vはその引数となっている。
  lval* result = lval_call(e, f, v);

  // fを破棄。
  lval_del(f);
  return result;
}

void lenv_put(lenv* e, lval* k, lval* v);
lval* builtin_eval(lenv *e, lval* a);
lval* builtin_list(lenv *e, lval* a);

// 関数適用。fは関数、aは実引数。
lval* lval_call(lenv* e, lval* f, lval* a) {
  // ビルトイン関数であれば、そのまま関数ポインタを実行。
  if (f->builtin) { return f->builtin(e, a); }

  int given = a->count; // 実引数の数。
  int total = f->formals->count; // 仮引数の数。

  while (a->count) {
    // 実引数が仮引数より多ければエラー。
    if (f->formals->count == 0) {
      lval_del(a); return lval_err("Function passed too many arguments. Got %i, Expected %i.", given, total);
    }

    // 先頭の仮引数。
    lval* sym = lval_pop(f->formals, 0);
    
    // 仮引数に可変長引数を表すトークンが現れた場合。
    if (strcmp(sym->sym, "&") == 0) {
      // &の後に仮引数が続かなければエラー。
      if (f->formals->count != 1) {
        lval_del(a);
        return lval_err("Function format invalid. Symbol '&' not followed by single symbol.");
      }

      // &の後続のシンボルを取得。
      lval* nsym = lval_pop(f->formals, 0);
      // 引数全てをリスト化し、それを関数の環境内で可変長仮引数のシンボルに束縛。
      lenv_put(f->env, nsym, builtin_list(e, a));
      lval_del(sym); lval_del(nsym);
      break; // 可変長引数より後にシンボルは続かないので、ループを抜ける。
    }
    
    // 一番目の実引数。
    lval* val = lval_pop(a, 0);

    // 関数の環境内で仮引数のシンボルと実引数を束縛。
    lenv_put(f->env, sym, val);

    lval_del(sym); lval_del(val);
  }

  lval_del(a);

  // 評価されていない仮引数があり、かつ次の仮引数が&である場合。
  // 可変長引数はオプションなので、指定されない場合を考慮する。
  if (f->formals->count > 0 &&
      strcmp(f->formals->cell[0]->sym, "&") == 0) {
    // &の後には1つのシンボルが続く。
    if (f->formals->count != 2) {
      return lval_err("Function format invalid. Symbol '&' not followed by single symbol");
    }
    lval_del(lval_pop(f->formals, 0));

    // &を削除。
    lval* sym = lval_pop(f->formals, 0);
    // 空のリストを作成。
    lval* val = lval_qexpr();

    // 可変長引数に空のリストを束縛する。
    lenv_put(f->env, sym, val);
    lval_del(sym); lval_del(val);
  }

  // それ以上仮引数が存在しない場合。
  if (f->formals->count == 0) {
    // 関数が評価される環境を、関数内の環境の親に設定。
    f->env->par = e;
    // 関数の評価値を返却。
    return builtin_eval(f->env, lval_add(lval_sexpr(), lval_copy(f->body)));
  } else {
    // 部分適応した関数を返却。
    return lval_copy(f);
  }
}

////////////////////////////////////////
// lenv
////////////////////////////////////////

lval* lenv_get(lenv* e, lval* v);

// lvalを評価。
lval* lval_eval(lenv *e, lval* v) {
  if (v->type == LVAL_SYM) {
    // シンボルの場合、環境から値を取得する。
    lval* x = lenv_get(e, v);
    lval_del(v);
    return x;
  }
  if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(e, v); }
  return v;
}

// コンストラクタ(lenv)
lenv* lenv_new(void) {
  lenv* e = malloc(sizeof(lenv));
  e->par = NULL;
  e->count = 0;
  e->syms = NULL;
  e->vals = NULL;
  return e;
}

// デストラクタ(lenv)
void lenv_del(lenv *e) {
  for (int i = 0; i < e->count; i++) {
    free(e->syms[i]);
    lval_del(e->vals[i]);
  }
  free(e->syms);
  free(e->vals);
  free(e);
}

// 変数の値の取得。
lval* lenv_get(lenv* e, lval* k) {
  for (int i = 0; i < e->count; i++) {
    // 環境の中に該当するシンボルがあれば、その値のコピーを返す。
    if (strcmp(e->syms[i], k->sym) == 0) { return lval_copy(e->vals[i]); }
  }

  if (e->par) {
    // 外側の環境を探索。
    return lenv_get(e->par, k);
  } else {
    // シンボルが見つからなければエラー。
    return lval_err("Unboud Symbol '%s'", k->sym);
  }
}

// 変数の束縛。
void lenv_put(lenv* e, lval* k, lval* v) {
  for (int i = 0; i < e->count; i++) {
    if (strcmp(e->syms[i], k->sym) == 0) {
      // 既にシンボルが登録済みの場合は、その値を上書きする。
      lval_del(e->vals[i]);
      e->vals[i] = lval_copy(v);
      return;
    }
  }

  // 新規の変数。
  e->count++;
  // 値の領域の拡張。
  e->vals = realloc(e->vals, sizeof(lval*) * e->count);
  // シンボルの領域の拡張。
  e->syms = realloc(e->syms, sizeof(char*) * e->count);

  // 値の配列の末尾に対象の値を追加。
  e->vals[e->count-1] = lval_copy(v);
  // シンボルの配列の末尾に対象のシンボルを追加。
  e->syms[e->count-1] = malloc(strlen(k->sym)+1);
  strcpy(e->syms[e->count-1], k->sym);
}

// lenvのコピー。
lenv* lenv_copy(lenv* e) {
  lenv* n = malloc(sizeof(lenv));
  n->par = e->par;
  n->count = e->count;
  n->syms = malloc(sizeof(char*) * n->count);
  n->vals = malloc(sizeof(lval*) * n->count);
  for (int i = 0; i < e->count; i++) {
    n->syms[i] = malloc(strlen(e->syms[i]) + 1);
    strcpy(n->syms[i], e->syms[i]);
    n->vals[i] = lval_copy(e->vals[i]);
  }
  return n;
}

// グローバル変数の設定。
void lenv_def(lenv* e, lval* k, lval* v) {
  while (e->par) { e = e->par; }
  lenv_put(e, k, v);
}

////////////////////////////////////////
// 組み込み関数
////////////////////////////////////////

// 組み込み関数head。
lval* builtin_head(lenv *e, lval* a) {
  // 引数が1つであること。
  LASSERT(a, (a->count == 1), "Function 'head' passed too many arguments. Got %i, Expected %i.", a->count, 1);
  // 型がリストであること。
  LASSERT(a, (a->cell[0]->type == LVAL_QEXPR), "Function 'head' passed incorrect type for argument 0. Got %s, Expected %s.", ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR));
  // 空リストでないこと。
  LASSERT(a, (a->cell[0]->count != 0), "Function 'head' passed {}!");

  lval* v = lval_take(a, 0);

  // 先頭要素以外を削除。要素がひとつになるまでリストの2番目を削除していく。
  while (v->count > 1) { lval_del(lval_pop(v, 1)); }
  return v;
}

// 組み込み関数tail。
lval* builtin_tail(lenv *e, lval* a) {
  LASSERT(a, (a->count == 1), "Function 'tail' passed too many arguments!");
  LASSERT(a, (a->cell[0]->type == LVAL_QEXPR), "Function 'tail' passed incorrect types!");
  LASSERT(a, (a->cell[0]->count != 0), "Function 'tail' passed {}!");

  lval* v = lval_take(a, 0);

  lval_del(lval_pop(v, 0));
  return v;
}

// 組み込み関数list。
lval* builtin_list(lenv *e, lval* a) {
  // S式をリストに変換する。
  a->type = LVAL_QEXPR;
  return a;
}

// 組み込み関数eval。
lval* builtin_eval(lenv *e, lval* a) {
  LASSERT(a, (a->count == 1), "Function 'eval' passed too many arguments!");
  LASSERT(a, (a->cell[0]->type == LVAL_QEXPR), "Function 'eval' passed incorrect type!");

  lval* x = lval_take(a, 0);
  // リストをS式に変換し評価。
  x->type = LVAL_SEXPR;
  return lval_eval(e, x);
}

// 組み込み関数join。リストの連結。
lval* builtin_join(lenv* e, lval* a) {
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

// 組み込みラムダ式。
lval* builtin_lamda(lenv* e, lval* a) {
  LASSERT_NUM("\\", a, 2);
  LASSERT_TYPE("\\", a, 0, LVAL_QEXPR);
  LASSERT_TYPE("\\", a, 1, LVAL_QEXPR);

  for (int i = 0; i < a->cell[0]->count; i++) {
    LASSERT(a, (a->cell[0]->cell[i]->type == LVAL_SYM),
            "Cannot define non-symbol. Got %s, Expected %s.",
            ltype_name(a->cell[0]->cell[i]->type), ltype_name(LVAL_SYM));
  }

  lval* formals = lval_pop(a, 0);
  lval* body = lval_pop(a, 0);
  lval_del(a);

  return lval_lambda(formals, body);
}

// オペランドのみが含まれたlvalとオペレータから、計算済みのlvalを返す。
// (1 2), '+' => 3 
lval* builtin_op(lenv* e, lval* a, char* op) {
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

lval* builtin_add(lenv* e, lval* a) { return builtin_op(e, a, "+"); }
lval* builtin_sub(lenv* e, lval* a) { return builtin_op(e, a, "-"); }
lval* builtin_mul(lenv* e, lval* a) { return builtin_op(e, a, "*"); }
lval* builtin_div(lenv* e, lval* a) { return builtin_op(e, a, "/"); }

// 変数への値の代入。funcによって挙動を変える。
lval* builtin_var(lenv* e, lval* a, char* func) {
  LASSERT_TYPE(func, a, 0, LVAL_QEXPR);

  // 第1引数はシンボルのリスト。
  lval* syms = a->cell[0];
  // 全ての要素がシンボルであるか確認。
  for (int i = 0; i < syms->count; i++) {
    LASSERT(a, (syms->cell[i]->type == LVAL_SYM),
            "Function ' %s' cannot define non-symbol. Got %s, Expected %s.",
            func, ltype_name(syms->cell[i]->type), ltype_name(LVAL_SYM));
  }

  // シンボルの数と、値の数が同一か確認。
  LASSERT(a, (syms->count == a->count-1),
          "Function '%s' passed too many arguments for symbols. Got %i, Expected %i.",
          func, syms->count, a->count-1);

  for (int i = 0; i < syms->count; i++) {
    // defはグローバル環境に、
    if (strcmp(func, "def") == 0) { lenv_def(e, syms->cell[i], a->cell[i+1]); }
    // =はローカル環境に束縛。
    if (strcmp(func, "=") == 0) { lenv_put(e, syms->cell[i], a->cell[i+1]); }
  }
  lval_del(a);
  return lval_sexpr();
}

lval* builtin_def(lenv* e, lval* a) { return builtin_var(e, a, "def"); }
lval* builtin_put(lenv* e, lval* a) { return builtin_var(e, a, "="); }

// 組み込み関数を環境に束縛。
void lenv_add_builtin(lenv* e, char* name, lbuiltin func) {
  lval* k = lval_sym(name);
  lval* v = lval_fun(func);
  lenv_put(e, k, v);
  lval_del(k); lval_del(v);
}

// 組み込み関数を初期化。
void lenv_add_builtins(lenv* e) {
  lenv_add_builtin(e, "list", builtin_list);
  lenv_add_builtin(e, "head", builtin_head);
  lenv_add_builtin(e, "tail", builtin_tail);
  lenv_add_builtin(e, "eval", builtin_eval);
  lenv_add_builtin(e, "join", builtin_join);
  lenv_add_builtin(e, "def",  builtin_def);

  lenv_add_builtin(e, "+",   builtin_add);
  lenv_add_builtin(e, "-",   builtin_sub);
  lenv_add_builtin(e, "*",   builtin_mul);
  lenv_add_builtin(e, "/",   builtin_div);

  lenv_add_builtin(e, "def", builtin_def);
  lenv_add_builtin(e, "=",   builtin_put);

  lenv_add_builtin(e, "\\",  builtin_lamda);
}

////////////////////////////////////////
// main
////////////////////////////////////////

int main(int argc, char** argv) {
  mpc_parser_t* Number = mpc_new("number");
  mpc_parser_t* Symbol = mpc_new("symbol");
  mpc_parser_t* Sexpr  = mpc_new("sexpr");
  mpc_parser_t *Qexpr  = mpc_new("qexpr");
  mpc_parser_t* Expr   = mpc_new("expr");
  mpc_parser_t* Lispy  = mpc_new("lispy");

  // 字句解析の規則を設定。
  mpca_lang(MPCA_LANG_DEFAULT,
            "                                   \
number   : /-?[0-9]+/ ;                                                 \
symbol   : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ;                           \
sexpr    : '(' <expr>* ')' ;                                            \
qexpr    : '{' <expr>* '}' ;                                            \
expr     : <number> | <symbol> | <sexpr> | <qexpr> ;                    \
lispy    : /^/ <expr>* /$/ ;                                            \
",
            Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

  puts("Lispy Version 0.0.0.0.1");
  puts("Press Ctrl+c to Exit\n");

  // グローバル環境を確保。
  lenv* e = lenv_new();
  // 組み込み関数をグローバル環境にロード。
  lenv_add_builtins(e);
  
  while (1) {
    char* input = readline("lispy> ");
    add_history(input);
    mpc_result_t r;

    // 入力をパース。
    if (mpc_parse("<stdin>", input, Lispy, &r)) {
      lval* x = lval_eval(e, lval_read(r.output));
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
