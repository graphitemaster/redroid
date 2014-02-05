#include <module.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

MODULE_DEFAULT(calc);

static const double calc_cvalues[] = {
    42,
    3.14159265358979323846,
    3.14159265358979323846 * 2,
    2.71828182845904523536,
    0
};

static const char *const calc_cnames[] = {
    "theanswertolifetheuniverseandeverything",
    "PI",
    "TAU",
    "E",
    0
};

static const char *calc_jokes[] = {
    "The continuous functions are having a ball. At the dance floor, cosine and sine are jumping up and down, and the polynomials are forming a ring. But the exponential function is standing seperately the whole evening. Due to sympathy for it, the identity joins it and suggests: \"Come one, just integrate yourself!\" – \"I've tried that already\", answers the exponential function, \"but it didn't change a bit!\"",
    "Every natural number is interesting. Let us assume, that there is an uninteresting natural number. Then there would be a smallest unteresting number – but this number would be very interesting indeed. Therefore it is an interesting number. This contradiction proves: All natural numbers are interesting.",
    "To every problem, there is a one line proof ... if we start sufficiently far to the left.",
    "The best moments in the life of a mathematician are the first few moments after one has proved the result, but before one finds the mistake.",
    "A topologist is a person who does not know the difference between a coffee cup and a doughnut.",
    "2 is the oddest prime.",
    "An engineer is convinced that his equations are an approximation of reality. A physicist thinks that reality is an approximation of his equations. A mathematician does not worry about that.",
    "My geometry teacher was sometimes acute, and sometimes obtuse, but always, he was right.",
    "It was mentioned on CNN that the new prime number discovered recently is four times bigger then the previous record.",
    "What is the shortest mathematicians joke? Let epsilon be smaller than zero.",
    "What is a rigorous definition of rigor?",
    "Classification of mathematical problems as linear and nonlinear is like classification of the Universe as bananas and non-bananas.",
    "A tragedy of mathematics is a beautiful conjecture ruined by an ugly fact.",
    "Algebraic symbols are used when you do not know what you are talking about.",
    "A mathematician is asked to design a table. He first designs a table with no legs. Then he designs a table with infinitely many legs. He spends the rest of his life generalizing the results for the table with N legs (where N is not necessarily a natural number).",
    "Golden rule for math teachers: You must tell the truth, and nothing but the truth, but not the whole truth.",
    "The problems for the exam will be similar to the discussed in the class. Of course, the numbers will be different. But not all of them. Pi will still be 3.14159...",
    "Relations between pure and applied mathematicians are based on trust and understanding. Namely, pure mathematicians do not trust applied mathematicians, and applied mathematicians do not understand pure mathematicians.",
    "These days, even the most pure and abstract mathematics is in danger to be applied.",
    "The reason that every major university maintains a department of mathematics is that it is cheaper to do this than to institutionalize all those people.",
    "The number you have dialed is imaginary. Please rotate your phone 90 degrees and try again.",
    "In modern mathematics, algebra has become so important that numbers will soon only have symbolic meaning.",
    "A circle is a round straight line with a hole in the middle.",
    "In the topologic hell the beer is packed in Klein's bottles.",
    "A statistician can have his head in an oven and his feet in ice, and he will say that on average he feels fine.",
    "Math and Alcohol don't mix, so... PLEASE DON'T DRINK AND DERIVE"
};

typedef struct {
    size_t      index;      // stack index
    char       *sp;         // stack pointer
    irc_t      *irc;        // irc instance
    const char *channel;    // irc channel
    const char *user;       // irc user
} calc_parser_t;

static const int8_t calc_sip['z' - 'E' + 1] = {
    ['y'-'E']= -24, ['z'-'E']= -21,
    ['a'-'E']= -18, ['f'-'E']= -15,
    ['p'-'E']= -12, ['n'-'E']= - 9,
    ['u'-'E']= - 6, ['m'-'E']= - 3,
    ['c'-'E']= - 2, ['d'-'E']= - 1,
    ['h'-'E']=   2, ['k'-'E']=   3,
    ['K'-'E']=   3, ['M'-'E']=   6,
    ['G'-'E']=   9, ['T'-'E']=  12,
    ['P'-'E']=  15, ['E'-'E']=  18,
    ['Z'-'E']=  21, ['Y'-'E']=  24,
};

static double calc_strtod(const char *numstr, char **tail) {
    char  *next;
    double d = strtod(numstr, &next);
    if (next != numstr) {
        if (next[0] == 'd' && next[1] == 'B') {
            d = pow(10, d / 20);
            next += 2;
        } else if (*next >= 'E' && *next <= 'z') {
            int e = calc_sip[*next - 'E'];
            if (e) {
                if (next[1] == 'i') {
                    d *= pow(2, e/0.3);
                    next += 2;
                } else {
                    d *= pow(10, e);
                    next++;
                }
            }
        }

        if (*next=='B') {
            d *= 8;
            next++;
        }
    }
    if (tail)
        *tail = next;
    return d;
}

static int calc_strmatch(const char *s, const char *prefix) {
    size_t i;
    for (i = 0; prefix[i]; i++)
        if (prefix[i] != s[i])
            return false;
    return !(s[i] - '0' <= 9U || s[i] - 'a' <= 25U || s[i] - 'A' <= 25U || s[i] == '_');
}

typedef struct eval_expr_s {
    enum {
        e_value, e_const, e_func0, e_isnan, e_isinf,
        e_mod,   e_max,   e_min,   e_eq,    e_gt,
        e_gte,   e_pow,   e_mul,   e_div,   e_add,
        e_last,  e_floor, e_ceil,  e_trunc, e_sqrt,
        e_not,
    } type;

    double value;

    union {
        int const_index;
        double (*func)(double);
    };

    struct eval_expr_s *param[2];
} calc_expr_t;

static double eval_expr(calc_parser_t *p, calc_expr_t *e) {
    switch (e->type) {
        case e_value:  return e->value;
        case e_const:  return e->value * calc_cvalues[e->const_index];
        case e_func0:  return e->value * e->func(eval_expr(p, e->param[0]));
        case e_isnan:  return e->value * !!isnan(eval_expr(p, e->param[0]));
        case e_isinf:  return e->value * !!isinf(eval_expr(p, e->param[0]));
        case e_floor:  return e->value * floor(eval_expr(p, e->param[0]));
        case e_ceil :  return e->value * ceil (eval_expr(p, e->param[0]));
        case e_trunc:  return e->value * trunc(eval_expr(p, e->param[0]));
        case e_sqrt:   return e->value * sqrt (eval_expr(p, e->param[0]));
        case e_not:    return e->value * eval_expr(p, e->param[0]) == 0;
        default: {
            double d1 = eval_expr(p, e->param[0]);
            double d2 = eval_expr(p, e->param[1]);
            switch (e->type) {
                case e_mod:  return e->value * (d1 - floor(d1/d2)*d2);
                case e_max:  return e->value * (d1 >  d2 ?  d1 : d2);
                case e_min:  return e->value * (d1 <  d2 ?  d1 : d2);
                case e_eq:   return e->value * (d1 == d2 ? 1.0 : 0.0);
                case e_gt:   return e->value * (d1 >  d2 ? 1.0 : 0.0);
                case e_gte:  return e->value * (d1 >= d2 ? 1.0 : 0.0);
                case e_pow:  return e->value * pow(d1, d2);
                case e_mul:  return e->value * (d1 * d2);
                case e_div:  return e->value * (d1 / d2);
                case e_add:  return e->value * (d1 + d2);
                case e_last: return e->value * d2;
                default:
                    break;
            }
        }
    }
    return NAN;
}

static bool parse_expr(calc_expr_t **e, calc_parser_t *p);

static bool parse_primary(calc_expr_t **e, calc_parser_t *p)
{
    calc_expr_t *d = memset(malloc(sizeof(*d)), 0, sizeof(*d));

    char *next = p->sp;
    char *s0   = p->sp;

    size_t i;

    d->value = calc_strtod(p->sp, &next);
    if (next != p->sp) {
        d->type = e_value;
        p->sp   = next;

        *e = d;
        return true;
    }
    d->value = 1;

    for (i = 0; calc_cnames && calc_cnames[i]; i++) {
        if (calc_strmatch(p->sp, calc_cnames[i])) {
            p->sp         += strlen(calc_cnames[i]);
            d->type        = e_const;
            d->const_index = i;
            *e = d;
            return true;
        }
    }

    if (!(p->sp = strchr(p->sp, '('))) {
        irc_write(p->irc, p->channel, "%s: Undefined constant or missing '(' in '%s'\n", p->user, s0);
        p->sp = next;
        return false;
    }

    p->sp++;
    if (*next == '(') {
        d = NULL;
        if (!parse_expr(&d, p))
            return false;
        if (p->sp[0] != ')') {
            irc_write(p->irc, p->channel, "%s: Missing ')' in '%s'\n", p->user, s0);
            return false;
        }
        p->sp++;
        *e = d;
        return true;
    }

    if (!parse_expr(&(d->param[0]), p))
        return false;

    if (p->sp[0]== ',') {
        p->sp++;
        parse_expr(&d->param[1], p);
    }

    if (p->sp[0] != ')') {
        irc_write(p->irc, p->channel, "%s: Missing ')' or too many arguments in '%s'\n", p->user, s0);
        return false;
    }

    p->sp++;
    d->type = e_func0;

         if (calc_strmatch(next, "sinh"  )) d->func = sinh;
    else if (calc_strmatch(next, "cosh"  )) d->func = cosh;
    else if (calc_strmatch(next, "tanh"  )) d->func = tanh;
    else if (calc_strmatch(next, "sin"   )) d->func = sin;
    else if (calc_strmatch(next, "cos"   )) d->func = cos;
    else if (calc_strmatch(next, "tan"   )) d->func = tan;
    else if (calc_strmatch(next, "atan"  )) d->func = atan;
    else if (calc_strmatch(next, "asin"  )) d->func = asin;
    else if (calc_strmatch(next, "acos"  )) d->func = acos;
    else if (calc_strmatch(next, "exp"   )) d->func = exp;
    else if (calc_strmatch(next, "log"   )) d->func = log;
    else if (calc_strmatch(next, "abs"   )) d->func = fabs;
    else if (calc_strmatch(next, "mod"   )) d->type = e_mod;
    else if (calc_strmatch(next, "max"   )) d->type = e_max;
    else if (calc_strmatch(next, "min"   )) d->type = e_min;
    else if (calc_strmatch(next, "eq"    )) d->type = e_eq;
    else if (calc_strmatch(next, "gte"   )) d->type = e_gte;
    else if (calc_strmatch(next, "gt"    )) d->type = e_gt;
    else if (calc_strmatch(next, "isnan" )) d->type = e_isnan;
    else if (calc_strmatch(next, "isinf" )) d->type = e_isinf;
    else if (calc_strmatch(next, "floor" )) d->type = e_floor;
    else if (calc_strmatch(next, "ceil"  )) d->type = e_ceil;
    else if (calc_strmatch(next, "trunc" )) d->type = e_trunc;
    else if (calc_strmatch(next, "sqrt"  )) d->type = e_sqrt;
    else if (calc_strmatch(next, "not"   )) d->type = e_not;
    else if (calc_strmatch(next, "lte"   )) { calc_expr_t *tmp = d->param[1]; d->param[1] = d->param[0]; d->param[0] = tmp; d->type = e_gte; }
    else if (calc_strmatch(next, "lt"    )) { calc_expr_t *tmp = d->param[1]; d->param[1] = d->param[0]; d->param[0] = tmp; d->type = e_gt; }
    else {
        irc_write(p->irc, p->channel, "%s: Unknown function in '%s'\n", p->user, s0);
        return false;
    }

    *e = d;
    return true;
}

static calc_expr_t *new_eval_expr(int type, int value, calc_expr_t *p0, calc_expr_t *p1) {
    return memcpy(
        malloc(sizeof(calc_expr_t)),
        &(calc_expr_t) {
            .type     = type,
            .value    = value,
            .param[0] = p0,
            .param[1] = p1
        },
        sizeof(calc_expr_t)
    );

}

static bool parse_pow(calc_expr_t **e, calc_parser_t *p, int *sign) {
    *sign = (*p->sp == '+') - (*p->sp == '-');
    p->sp += *sign & 1;
    return parse_primary(e, p);
}

static bool parse_dB(calc_expr_t **e, calc_parser_t *p, int *sign) {
    if (*p->sp == '-') {
        char  *next;
        double ignored = strtod(p->sp, &next);
        if (next != p->sp && next[0] == 'd' && next[1] == 'B') {
            *sign = 0;
            return parse_primary(e, p);
        }
    }
    return parse_pow(e, p, sign);
}

static bool parse_factor(calc_expr_t **e, calc_parser_t *p) {
    int sign;
    int sign2;

    calc_expr_t *e0;
    calc_expr_t *e1;
    calc_expr_t *e2;

    if (!parse_dB(&e0, p, &sign))
        return false;

    while (p->sp[0] == '^') {
        e1 = e0;
        p->sp++;
        if (!parse_dB(&e2, p, &sign2))
            return false;
        e0 = new_eval_expr(e_pow, 1, e1, e2);
        if (e0->param[1])
            e0->param[1]->value *= (sign2|1);
    }
    if (e0)
        e0->value *= (sign|1);
    *e = e0;
    return true;
}

static bool parse_term(calc_expr_t **e, calc_parser_t *p) {
    calc_expr_t *e0;
    calc_expr_t *e1;
    calc_expr_t *e2;

    if (!parse_factor(&e0, p))
        return false;

    while (p->sp[0] == '*' || p->sp[0] == '/') {
        int c = *p->sp++;
        e1 = e0;
        if (!parse_factor(&e2, p))
            return false;
        e0 = new_eval_expr(c == '*' ? e_mul : e_div, 1, e1, e2);
    }

    *e = e0;
    return true;
}

static bool parse_subexpr(calc_expr_t **e, calc_parser_t *p) {
    calc_expr_t *e0;
    calc_expr_t *e1;
    calc_expr_t *e2;

    if (!parse_term(&e0, p))
        return false;

    while (*p->sp == '+' || *p->sp == '-') {
        e1 = e0;
        if (!parse_term(&e2, p))
            return false;
        e0 = new_eval_expr(e_add, 1, e1, e2);
    };

    *e = e0;
    return true;
}

static bool parse_expr(calc_expr_t **e, calc_parser_t *p) {
    calc_expr_t *e0;
    calc_expr_t *e1;
    calc_expr_t *e2;

    if (p->index == 0)
        return false;
    p->index--;

    if (!parse_subexpr(&e0, p))
        return false;

    while (*p->sp == ';') {
        p->sp++;
        e1 = e0;
        if (!parse_subexpr(&e2, p))
            return false;
        e0 = new_eval_expr(e_last, 1, e1, e2);
    }

    p->index++;
    *e = e0;
    return true;
}

static bool calc_verify(calc_expr_t *e) {
    if (!e)
        return false;

    switch (e->type) {
        case e_value:
        case e_const:
            return true;

        case e_func0:
        case e_isnan:
        case e_isinf:
        case e_floor:
        case e_ceil:
        case e_trunc:
        case e_sqrt:
        case e_not:
            return calc_verify(e->param[0]);

        default:
            return calc_verify(e->param[0]) && calc_verify(e->param[1]);
    }
}

static bool calc(calc_expr_t **expr, const char *s, calc_parser_t p) {
    char       *w  = malloc(strlen(s) + 1);
    char       *wp = w;
    const char *s0 = s;

    while (*s)
        if (!isspace(*s++))
            *wp++ = s[-1];
    *wp++ = 0;

    p.index = 100;
    p.sp    = w;

    calc_expr_t *e = NULL;
    if (!parse_expr(&e, &p)) {
        if (p.index == 0)
            irc_write(p.irc, p.channel, "%s: Expression too long", p.user);
        return false;
    }

    if (*p.sp) {
        irc_write(p.irc, p.channel, "%s: Invalid chracter(s) '%s' at end of expression '%s'", p.user, p.sp, s0);
        return false;
    }

    if (!calc_verify(e))
        return false;

    *expr = e;
    return true;
}

void module_enter(irc_t *irc, const char *channel, const char *user, const char *message) {
    if (!strcmp(message, "joke")) {
        irc_write(irc, channel, "%s: %s", user, calc_jokes[urand() % (sizeof(calc_jokes) / sizeof(*calc_jokes))]);
        return;
    }

    calc_parser_t ctx = {
        .irc     = irc,
        .channel = channel,
        .user    = user
    };

    calc_expr_t *expr;
    bool         print;
    double       value = (!(print = calc(&expr, message, ctx)))
                            ? NAN
                            : eval_expr(&ctx, expr);

    if (print)
        irc_write(irc, channel, "%s: %g", user, value);
}
