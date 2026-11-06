// Helper macros to count up to 5 arguments (extendable as needed)
#define VA_NARGS_IMPL(_1, _2, _3, _4, _5, N, ...) N
#define VA_NARGS(...) VA_NARGS_IMPL(__VA_ARGS__, 5, 4, 3, 2, 1)

// Dispatcher macro to pick the right expansion based on argument count
#define CONCAT(a, b) a##b
#define EXPAND(x) x
#define DISPATCH(macro, nargs, ...) EXPAND(CONCAT(macro, nargs)(__VA_ARGS__))

// Specific expansion macros for 1 to 5 arguments
#define UNIT_ARRAY_1(t, a)                                                                         \
  (t[]) { &(a) }
#define UNIT_ARRAY_2(t, a, b)                                                                      \
  (t[]) { &(a), &(b) }
#define UNIT_ARRAY_3(t, a, b, c)                                                                   \
  (t[]) { &(a), &(b), &(c) }
#define UNIT_ARRAY_4(t, a, b, c, d)                                                                \
  (t[]) { &(a), &(b), &(c), &(d) }
#define UNIT_ARRAY_5(t, a, b, c, d, e)                                                             \
  (t[]) { &(a), &(b), &(c), &(d), &(e) }

// Public macro interface: UNIT_LIST(Type, arg1, arg2, ...)
#define UNIT_LIST(type, ...) DISPATCH(UNIT_ARRAY_, VA_NARGS(__VA_ARGS__), type, __VA_ARGS__)
