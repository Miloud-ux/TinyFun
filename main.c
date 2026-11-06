#include "macros.h"
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NONE 0xDEAD
typedef enum {
  INT = 0xA,
  DOUBLE,
  BOOL,
  CHAR,
  FUNCTION = 0xFAFA,
} Type;

typedef char Byte;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint64_t u64;
typedef uintptr_t up64;
typedef uint32_t u32;
typedef unsigned long long ull;

struct Function;
typedef void *Unit; /* generic type alias like in Scala */
typedef Unit (*genericfuncPtr)(struct Function *func);
typedef Unit (*applyfunction)(struct Function *f, Unit *args, u64 nArgs);

/* function ptr val starts with FUNCTION (ie: 0xFAFA) */
#define TAG_FUNC_PTR(p) ((void *)((up64)(p) | ((up64)FUNCTION << 48)))

/*                                                                             \
 * cleans up a tagged pointer. This works for any 2-byte tagged                \
 * pointer, not just function pointers.                                        \
 *                                                                             \
 * assumption: The upper 16 bits are not used for virtual addressing           \
 * (true for x86_64 linux and i think windows's user-space pointers).          \
 *                                                                             \
 * WARNING: Always clean tagged pointers before use, otherwise boom            \
 */
#define CLEAN_PTR(p) (void *)((up64)(p) & 0x0000FFFFFFFFFFFFULL)

/* whenever i tag ptrs i only tag the upper 2 bytes */
/* this is inlined because i hate preprocessor's magic */
static inline u16 extract_ptr_tag(Unit ptr) { return ((up64)ptr >> 48); }
static inline u8 is_func_tagged(struct Function *ptr) {
  return 1 ? extract_ptr_tag(ptr) == 0xFAFA : 0;
}

#define DEFINE_BODY(func_name) Unit func_name(struct Function *func)

#define NEW_FUNCTION(behavior_func, ret_type, ...)                             \
  createFunction(sizeof((void *[]){__VA_ARGS__}) / sizeof(void *), NULL,       \
                 behavior_func, ret_type, NULL, (void *[]){__VA_ARGS__})

typedef struct Function {
  u64 argsNum;
  Type *inputTypes;
  genericfuncPtr behaviour;
  Type returnType;
  char padding[4];
  u64 closedVarsNum;
  Unit *closedVars;          /* When creating a closure */
  struct Function *origFunc; /* for currying */
  /* add function name in the future */
  Unit args[];
} Function;

typedef struct {
  u64 FunctionsNumber;
  u64 Counter;
  applyfunction apply;
  Function **Funcs;
} FunctionInterface;

static FunctionInterface fmt;

Function *createFunction(u64 argsNum, Type *inputTypes, genericfuncPtr b,
                         Type returnType, void *closedVars, void *args[]) {
  if (fmt.Counter >= fmt.FunctionsNumber) {
    u64 new_size = fmt.FunctionsNumber == 0 ? 10 : fmt.FunctionsNumber * 2;
    Function **new_funcs = realloc(fmt.Funcs, new_size * sizeof(Function *));
    assert(new_funcs != NULL);
    fmt.Funcs = new_funcs;
    fmt.FunctionsNumber = new_size;
  }

  Function *func = calloc(1, sizeof(Function) + (argsNum * sizeof(void *)));
  Function *f = TAG_FUNC_PTR(func);
  assert(func != NULL);

  func->argsNum = argsNum;

  if (inputTypes != NULL && argsNum > 0) {
    func->inputTypes = malloc(argsNum * sizeof(Type));
    assert(func->inputTypes != NULL);
    memcpy(func->inputTypes, inputTypes, argsNum * sizeof(Type));
  } else {
    func->inputTypes = NULL;
  }

  func->behaviour = b;
  func->returnType = returnType;
  func->closedVars = NULL;
  func->closedVarsNum = 0;
  func->origFunc = NULL;
  fmt.Funcs[fmt.Counter] = f;
  fmt.Counter += 1;

  if (args != NULL && argsNum > 0) {
    memcpy(func->args, args, argsNum * sizeof(void *));
  }

  return TAG_FUNC_PTR(func);
}

void freeFunction(Function *func) {

  Function *new_f;
  if (is_func_tagged(func)) {
    new_f = (Function *)CLEAN_PTR(func);
  } else {
    new_f = func;
  }

  new_f = (Function *)CLEAN_PTR(func);

  if (new_f != NULL) {
    if (new_f->inputTypes != NULL) {
      free(new_f->inputTypes);
    }
    if (new_f->closedVars != NULL) {
      free(new_f->closedVars);
    }
    free(new_f);
  }
}

Unit apply(Function *f, Unit *args, u64 nArgs);

DEFINE_BODY(apply_curried) {
  Function *clean_func =
      is_func_tagged(func) ? (Function *)CLEAN_PTR(func) : func;
  Function *orig = clean_func->origFunc;
  assert(orig != NULL);
  u64 total = clean_func->closedVarsNum + clean_func->argsNum;
  Unit *merged = malloc(total * sizeof(Unit));
  assert(merged != NULL);
  memcpy(merged, clean_func->closedVars,
         clean_func->closedVarsNum * sizeof(Unit));
  memcpy(merged + clean_func->closedVarsNum, clean_func->args,
         clean_func->argsNum * sizeof(Unit));
  Unit result = apply(orig, merged, total);
  free(merged);
  return result;
}

Unit apply(Function *f, Unit *args, u64 nArgs) {
  /* If it's tagged clean it otherwise it's clean
   * so you can use it */
  Function *new_f;
  if (is_func_tagged(f)) {
    new_f = (Function *)CLEAN_PTR(f);
  } else {
    new_f = f;
  }

  new_f = (Function *)CLEAN_PTR(f);

  if (nArgs > new_f->argsNum) {
    fprintf(stderr, "apply: too many args (%llu > %llu)\n",
            (unsigned long long)nArgs, (unsigned long long)new_f->argsNum);
    abort();
  }

  if (nArgs == new_f->argsNum) {
    memcpy(new_f->args, args, nArgs * sizeof(Unit));
    return new_f->behaviour(new_f);
  }

  u64 remaining = new_f->argsNum - nArgs;
  Function *out = (Function *)CLEAN_PTR(createFunction(
      remaining, NULL, apply_curried, new_f->returnType, NULL, NULL));

  /* save the tagged ptr because we need to know it's a ptr */
  out->origFunc = f;
  out->closedVarsNum = nArgs;
  out->closedVars = malloc(nArgs * sizeof(Unit));
  assert(out->closedVars != NULL);
  memcpy(out->closedVars, args, nArgs * sizeof(Unit));

  /* tag it again */
  return TAG_FUNC_PTR(out);
}

void InitFunctionsInterface(applyfunction applyFn) {
#define MIN_FUNCTIONS_NUMBER 10
  fmt.FunctionsNumber = MIN_FUNCTIONS_NUMBER;
  fmt.Counter = 0;
  fmt.apply = applyFn;
  fmt.Funcs = calloc(MIN_FUNCTIONS_NUMBER, sizeof(Function *));
#undef MIN_FUNCTIONS_NUMBER
}

DEFINE_BODY(add) {
  Function *clean = is_func_tagged(func) ? (Function *)CLEAN_PTR(func) : func;
  int *firstNum = (int *)clean->args[0];
  int *secondNum = (int *)clean->args[1];
  int *res = calloc(1, sizeof(int));
  *res = *firstNum + *secondNum;
  return res;
}

DEFINE_BODY(identity_behavior) {
  Function *clean = is_func_tagged(func) ? (Function *)CLEAN_PTR(func) : func;
  return clean->args[0];
}

typedef struct {
  unsigned int gp_offset;
  unsigned int fp_offset;
  void *overflow_arg_area;
  void *reg_save_area;
} SystemV_va_list;

// the args list must end with NULL
/* cool idea but deprecated
* size_t getPipeFuncArgsNumb(va_list args) {
* check the 6 registers according to the SystemV ABI
 * since we have 6 args registers and they dont' spill
 * in this case then we can assume we are primarely
 * looking for 6 functions, if we are passing more
 * functions we have to look at the stack because
 * they spill there

 * Only works for x86_x64 Linux since the layout for
 * the va_list can differ depending on the platform
 *
*   va_list copy;
*   va_copy(copy, args);
*   size_t count = 2;
*   Function *f;
*   while ((f = va_arg(copy, Function *)) != NULL && is_func_tagged(f)) {
*     count++;
*   }
*   va_end(copy);
*   return count;
* }
*/

// Build one 1-arg Function without NEW_FUNCTION's vararg requirement
#define NEW_FN1(beh, ty) createFunction(1, NULL, beh, ty, NULL, NULL)

/* Optional: debug print of each stage's result, off by default */
#ifdef PIPE_DEBUG
static void debug_stage(Function *f, Unit res, size_t i) {
  Function *clean = (Function *)CLEAN_PTR(f);
  switch (clean->returnType) {
  case INT:
    printf("stage %zu result = %d\n", i + 1, *(int *)res);
    break;
  case DOUBLE:
  case BOOL:
  case CHAR:
  case FUNCTION:
  default:
    printf("stage %zu returned a non-int value\n", i + 1);
    break;
  }
}
#endif

/* Feeds `input` through funcs[0..n-1]; each result is the next input.
 * Does NOT free `input`. Frees every intermediate result it created. */
Unit pipe_n(Unit input, size_t n, Function **funcs) {
  if (input == NULL) {
    /* special "no value" case: don't pipe anything */
    return NULL;
  }

  Unit cur = input;

  for (size_t i = 0; i < n; i++) {
    Function *f = funcs[i];
    Function *clean = (Function *)CLEAN_PTR(f);

    Unit in[1] = {cur};
    Unit next = fmt.apply(f, in, 1);

#ifdef PIPE_DEBUG
    debug_stage(f, next, i);
#endif

    /* free the previous intermediate, but never the caller's input,
     * never a function value, and never something that was passed through */
    if (cur != input && next != cur && clean->returnType != FUNCTION) {
      free(cur);
    }
    cur = next;
  }

  return cur;
}

/* The compiler counts the functions, so no sentinel is needed */
#define PIPE(val, ...)                                                         \
  pipe_n((val), sizeof((Function *[]){__VA_ARGS__}) / sizeof(Function *),      \
         (Function *[]){__VA_ARGS__})

DEFINE_BODY(add_five) {
  Function *clean = is_func_tagged(func) ? (Function *)CLEAN_PTR(func) : func;
  int *x = (int *)clean->args[0];
  int *r = malloc(sizeof(int));
  *r = *x + 5;
  return r;
}

DEFINE_BODY(square) {
  Function *clean = is_func_tagged(func) ? (Function *)CLEAN_PTR(func) : func;
  int *x = (int *)clean->args[0];
  int *r = malloc(sizeof(int));
  *r = (*x) * (*x);
  return r;
}

DEFINE_BODY(double_it) {
  Function *clean = is_func_tagged(func) ? (Function *)CLEAN_PTR(func) : func;
  int *x = (int *)clean->args[0];
  int *r = malloc(sizeof(int));
  *r = (*x) * 2;
  return r;
}

DEFINE_BODY(mult0) {
  Function *clean = is_func_tagged(func) ? (Function *)CLEAN_PTR(func) : func;
  int *x = (int *)clean->args[0];
  int *r = malloc(sizeof(int));
  *r = (*x) * 0;
  return r;
}

int main() {
  InitFunctionsInterface(apply);

  int x = 3;
  int y = 4;

  /* currying: add takes 2 args so we  give it one
   * get a new function that takes on arguement so we
   * can chain it later and get a function back ----
   */

  Function *add_fn = createFunction(2, NULL, add, INT, NULL, NULL);
  Function *add_x = fmt.apply(add_fn, UNIT_LIST(Unit, x), 1);
  Function *clean_add_x = (Function *)CLEAN_PTR(add_x);

  printf("add_x argsNum       = %llu\n", (ull)clean_add_x->argsNum);
  printf("add_x closedVarsNum = %llu\n", (ull)clean_add_x->closedVarsNum);

  int *sum = fmt.apply(add_x, UNIT_LIST(Unit, y), 1);
  printf("3 + 4 = %d\n", *sum);

  /* Pipe: every stage takes a curried function */
  Function *add5_fn = NEW_FN1(add_five, INT);
  Function *square_fn = NEW_FN1(square, INT);
  Function *double_fn = NEW_FN1(double_it, INT);

  int *r1 = PIPE(&x, add5_fn, square_fn, double_fn);
  printf("((3 + 5)^2) * 2 = %d\n", *r1); /* 128 */

  /* a curried 2-arg function used as a stage: (3 + 3) * 2 */
  int *r2 = PIPE(&x, add_x, double_fn);
  printf("(3 + 3) * 2 = %d\n", *r2); /* 12 */

  /* cleanup, not really necesasry here */
  free(sum);
  free(r1);
  free(r2);
  freeFunction(add_x);
  freeFunction(add_fn);
  freeFunction(add5_fn);
  freeFunction(square_fn);
  freeFunction(double_fn);
  free(fmt.Funcs);
  return 0;
}
