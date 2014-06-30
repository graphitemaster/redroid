#ifndef __CTYPE
#define __CTYPE

static inline int isupper(int ch) {
    return (unsigned)ch-'A' < 26;
}

static inline int islower(int ch) {
    return (unsigned)ch-'a' < 26;
}

static inline int tolower(int ch) {
    return isupper(ch) ? ch | 32 : ch;
}

static inline int toupper(int ch) {
    return islower(ch) ? ch & 0x5F : ch;
}

static inline int isspace(int ch) {
    return ch == ' ' || (unsigned)ch-'\t' < 5;
}

static inline int isdigit(int ch) {
    return (unsigned)ch-'0' < 10;
}

static inline int isalpha(int ch) {
    return ((unsigned)ch|32)-'a' < 26;
}

static inline int isalnum(int ch) {
    return isalpha(ch) || isdigit(ch);
}

#endif
