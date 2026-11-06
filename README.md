# TinyFun

A tiny functional runtime in C, built as a learning experiment.

First-class function objects, **automatic currying**, **pointer tagging**, and a **pipe** operator, all in a single C file with no dependencies.

```c
Function *add5   = NEW_FN1(add_five, INT);
Function *square = NEW_FN1(square, INT);
Function *double_ = NEW_FN1(double_it, INT);

int *r = PIPE(&x, add5, square, double_);   // ((3 + 5)^2) * 2 = 128
```

> This is a hobby project for learning how functional runtimes work under the hood. It is **not** production code. See [Limitations](#limitations).

---

## Features

- **Function objects on the heap**: a `Function` holds its behaviour, arity, return type and arguments.
- **Pointer tagging**: function pointers carry the tag `0xFAFA` in their upper 16 bits, so the runtime can tell a function value from plain data without a header lookup.
- **Auto-currying**: call `apply` with fewer arguments than a function needs and you get back a new function waiting for the rest.
- **Pipe**: feed a value through a chain of one-argument functions, each result becoming the next input.
- **Function registry**: every created function is recorded in a global table (the start of a possible GC).
- **Argument-counting macros**: `NEW_FUNCTION(f, INT, &a, &b)` works out the arity from the arguments you pass.

## How it works

### 1. Tagged pointers

On x86_64 Linux, user-space addresses fit in 48 bits, so the top 16 are free:

```
 63            48 47                                 0
+----------------+------------------------------------+
|     0xFAFA     |          real address              |
+----------------+------------------------------------+
   tag (FUNCTION)
```

```c
TAG_FUNC_PTR(p)   // set the tag
CLEAN_PTR(p)      // strip the tag. ALWAYS do this before dereferencing
```

Dereferencing a tagged pointer crashes, so clean first.

### 2. Currying

```c
Function *add_fn = createFunction(2, NULL, add, INT, NULL, NULL);
Function *add_x  = fmt.apply(add_fn, UNIT_LIST(Unit, x), 1);   // needs 1 more arg
int *sum         = fmt.apply(add_x,  UNIT_LIST(Unit, y), 1);   // 3 + 4 = 7
```

When `apply` receives fewer arguments than `argsNum`, it creates a new `Function` that:

- remembers the original function (`origFunc`),
- stores the arguments received so far (`closedVars`),
- merges them with the later arguments and calls the original via `apply_curried`.

### 3. Pipe

```c
pipe_n(input, n, funcs)    // functional form
PIPE(input, f1, f2, f3)    // macro form: the compiler counts the functions
```

The caller owns the input and the final result. `pipe_n` frees the intermediate results it creates. Every stage must have **exactly one argument remaining**; curry multi-argument functions first.

## Build and run

You need a C99 compiler and a `macros.h` that defines `UNIT_LIST`.

```bash
gcc -std=c11 -Wall -Wextra -g -fsanitize=address,undefined main.c -o tagfn
./tagfn
```

Building with the sanitizers is recommended, since most bugs in this project are memory bugs.

Expected output:

```
add_x argsNum       = 1
add_x closedVarsNum = 1
3 + 4 = 7
((3 + 5)^2) * 2 = 128
(3 + 3) * 2 = 12
```

## Limitations

Known and intentional for now. Contributions and ideas are welcome.

**Platform**

- **48-bit address assumption.** Tagging the upper 16 bits works on typical x86_64 Linux/Windows user space. It breaks with 5-level paging (57-bit addresses), ARM64 pointer authentication and memory tagging (MTE). Low-bit tagging (using alignment bits) would be more portable.
- Pointer-to-integer tricks like this are implementation-defined in C, not guaranteed by the standard.

**Semantics**

- **`apply` mutates the function it is called on.** When all args are supplied, they are copied into the function's own `args` array before it runs. Applying the same `Function` twice at the same time (threads, recursion, re-entrancy) will overwrite state. A fix is to copy args into a temporary frame.
- **No type checking.** `inputTypes` exists but is not enforced; behaviours cast `Unit` (`void *`) however they like.
- **Pipe is int-oriented.** Freeing and debug printing assume `INT` results. A typed `Value { Type type; void *ptr; }` wrapper is the planned fix.
- **Every pipe stage takes exactly one value.** Multi-argument functions must be curried first. `PIPE` needs at least one function.

**Memory**

- **Arguments are borrowed pointers.** Closed-over values like `&x` are not copied, so they dangle if the curried function outlives them.
- **Ownership of results is by convention.** Behaviours `malloc` their results, and the caller frees them.
- **The registry (`fmt.Funcs`) records functions but never frees them.** `freeFunction` must be called manually.
- **No thread safety.** The runtime state is one global (`fmt`).

## Roadmap

- [ ] Typed values instead of raw `void *`
- [ ] `apply` with a temporary frame, so functions are reusable and re-entrant
- [ ] Low-bit tagging for portability
- [ ] Registry-based cleanup or a simple GC
- [ ] Split into `tagfn.h` / `tagfn.c`, plus a test suite
- [ ] Function names for debugging

## Project layout

```
main.c     runtime + demo
macros.h   helper macros (UNIT_LIST)
```

## License

MIT (or whichever you prefer)
