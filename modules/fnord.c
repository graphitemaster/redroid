#include <module.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

MODULE_DEFAULT(fnord);

static const char *fnord_get(const char *word_type) {
    database_statement_t *stmt = database_statement_create(
        "SELECT word FROM fnord_words WHERE type=? ORDER BY random() LIMIT 1;");

    if (!stmt || !database_statement_bind(stmt, "s", word_type))
        return "<database error>";

    database_row_t *row = database_row_extract(stmt, "s");
    if (!row)
        return "<empty>";

    const char *s = database_row_pop_string(row);

    if (!database_statement_complete(stmt))
        return "<incomplete>";

    return s;
}

static void fnord_a(string_t *out, const char *word_type) {
    const char *text = fnord_get(word_type);
    if (!text)
        return;
    // special 'u' treatment for sounds like 'unique', 'usable', 'using', ...
    // also special treatment for 'honor', 'honorary', 'honest', ...
    if (!memcmp(text, "uniq", 4) ||
        !memcmp(text, "use",  3)  ||
        !memcmp(text, "usi",  3)  ||
        !memcmp(text, "usa",  3))
    {
        string_catf(out, "%s ", "a");
    }
    else if (!strcmp(text, "honest") || !memcmp(text, "honor", 5))
        string_catf(out, "%s ", "an");
    else if (text[0] == 'a' ||
             text[0] == 'i' ||
             text[0] == 'u' ||
             text[0] == 'e' ||
             text[0] == 'o')
    {
        string_catf(out, "%s ", "an");
    }
    else
        string_catf(out, "%s ", "a");
    string_catf(out, "%s ", text);
}

static void fnord_from(string_t *out, const char *word_type) {
    const char *text = fnord_get(word_type);
    if (text)
        string_catf(out, "%s ", text);
}

static void fnord_chance(string_t *out, int chance, const char *word_type) {
    if (rand()%100 < chance)
        fnord_from(out, word_type);
}

static void fnord_a_chance(string_t *out, int chance, const char *maybe, const char *from) {
    if (rand()%100 < chance) {
        fnord_a(out, maybe);
        fnord_from(out, from);
    } else {
        fnord_a(out, from);
    }
}

static void fnord_chance_prefix(string_t *out, int chance, const char *text, const char *word_type) {
    if (rand()%100 < chance)
        return;

    string_catf(out, "%s ", text);
    fnord_from(out, word_type);
}

static string_t *generate() {
    string_t *out = string_construct();

    fnord_chance(out, 50, "intros");
    switch (rand()%13) {
        default:
        case 0:
            string_catf(out, "%s ", "the");
            fnord_chance(out, 50, "adjectives");
            fnord_from(out, "nouns");
            fnord_chance_prefix(out, 20, "in", "places");
            string_catf(out, "%s ", "is");
            fnord_from(out, "adjectives");
            break;
        case 1:
            fnord_from(out, "names");
            fnord_from(out, "actions");
            string_catf(out, "%s ", "the");
            fnord_from(out, "adjectives");
            fnord_from(out, "nouns");
            string_catf(out, "%s ", "and the");
            fnord_from(out, "adjectives");
            fnord_from(out, "nouns");
            break;
        case 2:
            string_catf(out, "%s ", "the");
            fnord_from(out, "nouns");
            string_catf(out, "%s ", "from");
            fnord_from(out, "places");
            string_catf(out, "%s ", "will go to");
            fnord_from(out, "places");
            break;
        case 3:
            fnord_from(out, "names");
            string_catf(out, "%s ", "must take the");
            fnord_from(out, "adjectives");
            fnord_from(out, "nouns");
            string_catf(out, "%s ", "from");
            fnord_from(out, "places");
            break;
        case 4:
            fnord_from(out, "places");
            string_catf(out, "%s ", "is");
            fnord_from(out, "adjectives");
            string_catf(out, "%s ", "and the");
            fnord_from(out, "nouns");
            string_catf(out, "%s ", "is");
            fnord_from(out, "adjectives");
            break;
        case 5:
            fnord_from(out, "names");
            fnord_from(out, "prepositions");
            fnord_from(out, "places");
            string_catf(out, "%s ", "for the");
            fnord_from(out, "adjectives");
            fnord_from(out, "nouns");
            break;
        case 6:
            string_catf(out, "%s ", "the");
            fnord_chance(out, 50, "adjectives");
            fnord_from(out, "nouns");
            fnord_from(out, "actions");
            string_catf(out, "%s ", "the");
            fnord_from(out, "adjectives");
            fnord_from(out, "nouns");
            fnord_chance_prefix(out, 20, "in", "places");
            break;
        case 7:
            fnord_from(out, "names");
            fnord_from(out, "prepositions");
            fnord_from(out, "places");
            string_catf(out, "%s ", "and");
            fnord_from(out, "actions");
            string_catf(out, "%s ", "the");
            fnord_from(out, "nouns");
            break;
        case 8:
            fnord_from(out, "names");
            string_catf(out, "%s ", "takes");
            fnord_from(out, "pronouns");
            fnord_chance(out, 50, "adjectives");
            fnord_from(out, "nouns");
            string_catf(out, "%s ", "and");
            fnord_from(out, "prepositions");
            fnord_from(out, "places");
            break;
        case 9:
            fnord_from(out, "names");
            fnord_from(out, "actions");
            string_catf(out, "%s ", "the");
            fnord_chance(out, 50, "adjectives");
            fnord_from(out, "nouns");
            break;
        case 10:
            fnord_from(out, "names");
            fnord_from(out, "actions");
            fnord_from(out, "names");
            string_catf(out, "%s ", "and");
            fnord_from(out, "pronouns");
            fnord_chance(out, 50, "adjectives");
            fnord_from(out, "nouns");
            break;
        case 11:
            string_catf(out, "%s ", "you must meet");
            fnord_from(out, "names");
            string_catf(out, "%s ", "at");
            fnord_from(out, "places");
            string_catf(out, "%s ", "and get the");
            fnord_chance(out, 50, "adjectives");
            fnord_from(out, "nouns");
            break;
        case 12:
            fnord_a(out, "nouns");
            string_catf(out, "%s ", "from");
            fnord_from(out, "places");
            fnord_from(out, "actions");
            string_catf(out, "%s ", "the");
            fnord_chance(out, 50, "adjectives");
            fnord_chance(out, 20, "adjectives");
            fnord_from(out, "nouns");
            break;
    }

    return out;
}

static string_t *generate_about(const char *user) {
    string_t *out = string_construct();

    switch (rand()%3) {
        default:
        case 0:
            fnord_chance(out, 50, "intros");
            fnord_a_chance(out, 50, "adjectives", "nouns");
            string_catf(out, "%s ", "from");
            fnord_from(out, "places");
            fnord_from(out, "actions");
            string_catf(out, "%s ", user);
            string_catf(out, "%s ", "with");
            fnord_a_chance(out, 50, "adjectives", "nouns");
            break;
        case 1:
            fnord_from(out, "intros");
            string_catf(out, "%s ", user);
            string_catf(out, "%s ", "is");
            fnord_a_chance(out, 50, "adjectives", "nouns");
            string_catf(out, "%s ", "from");
            fnord_from(out, "places");
            break;
        case 2:
            fnord_from(out, "intros");
            string_catf(out, "%s ", user);
            string_catf(out, "%s ", "is");
            fnord_a_chance(out, 50, "adjectives", "nouns");
            string_catf(out, "%s ", "from");
            fnord_from(out, "places");
            break;
    }

    return out;
}

void module_enter(irc_t *irc, const char *channel, const char *user, const char *message) {
    string_t *line = NULL;

    // generate either a message about a person, or a general message
    if (message)
        line = generate_about(message);
    else
        line = generate();

    if (!line || !string_length(line))
        return;

    char  *fnord_line = string_contents(line);
    size_t fnord_len  = string_length(line);

    // line ending
    fnord_line[--fnord_len] = 0;
    if (fnord_line[fnord_len-1] != '.' &&
        fnord_line[fnord_len-1] != '!' &&
        fnord_line[fnord_len-1] != '?')
    {
        fnord_line[fnord_len] = '.';
    }

    // capitalize the beginning
    fnord_line[0] = toupper(fnord_line[0]);

    // let the world know stuff!
    irc_write(irc, channel, "%s: %s", user, fnord_line);
}
