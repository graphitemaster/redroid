#ifndef REDROID_DEFAULT_HDR
#define REDROID_DEFAULT_HDR

#define PP_PHELP(X, Y) \
    X ## Y
#define PP_PASTE(X, Y) \
    PP_PHELP(X, Y)
#define PP_SKIP(_0, _1, _2, _3, _4, _5, _6, N, ...) \
    N
#define PP_SCAN(_0, _1, _2, _3, _4, _5, _6, ...) \
    PP_SKIP(, __VA_ARGS__, _0, _1, _2, _3, _4, _5, _6)
#define PP_COUNT(...) \
    PP_SCAN(6, 5, 4, 3, 2, 1, 0, __VA_ARGS__)

#define DEFAULT(NAME, ...) \
    PP_PASTE(PP_PASTE(NAME, _), PP_COUNT(__VA_ARGS__)) (__VA_ARGS__)

#endif
