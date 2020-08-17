#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "list.h"
#include "reader.h"
#include "value.h"

#define MAX_NUMBER_SIZE 128
#define MAX_STRING_SIZE 256
#define MAX_SYMBOL_SIZE 256

// R5RS 7.1.1: Lexical Structure

#define is_whitespace(c)  \
  (strchr(" \f\n\r\t\v", c) != NULL)
#define is_letter(c)  \
  ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
#define is_digit(c)  \
  (c >= '0' && c <= '9')
#define is_delimiter(c)  \
  (is_whitespace(c) || c == EOF || strchr("()\";", c) != NULL)
#define is_special_initial(c)  \
  (strchr("!$%&*/:<=>?^_~", c) != NULL)
#define is_initial(c)  \
  (is_letter(c) || is_special_initial(c))
#define is_special_subsequent(c)  \
  (strchr("+-.@", c) != NULL)
#define is_subsequent(c)  \
  (is_initial(c) || is_digit(c) || is_special_subsequent(c))
#define is_peculiar_identifier(c)  \
  (c == '+' || c == '-' || c == '.')

static int
advance(FILE* fp)
{
    // grab the next character
    return fgetc(fp);
}

static int
peek(FILE* fp)
{
    // grab the next character without changing position
    int c = fgetc(fp);
    ungetc(c, fp);
    return c;
}

static void
rollback(FILE*fp, int c)
{
    ungetc(c, fp);
}

static void
eat_whitespace(FILE* fp)
{
    int c;
    while ((c = advance(fp)) != EOF) {
        // if whitespace, eat and advance
        if (is_whitespace(c)) continue;

        // if comment found, eat til end of line
        if (c == ';') {
            while (((c = advance(fp)) != EOF) && (c != '\n'));
            continue;
        }

        // found a non-whitespace char at this point (put it back and break)
        rollback(fp, c);
        break;
    }
}

static void
eat_string(FILE* fp, const char* s)
{
    // ensure that an expected string comes next in the file
    while (*s != '\0') {
        int c = advance(fp);
        if (c != *s) {
            fprintf(stderr, "reader: incomplete character literal\n");
            exit(EXIT_FAILURE);
        }
        s++;
    }
}

static void
peek_expect_delimiter(FILE* fp)
{
    // expect a delimiter to follow some token otherwise raise an error
    if (!is_delimiter(peek(fp))) {
        fprintf(stderr, "reader: token not followed by delimiter\n");
        exit(EXIT_FAILURE);
    }
}

struct value*
read_character(FILE* fp)
{
    int c = advance(fp);
    switch (c) {
        case EOF:
            fprintf(stderr, "reader: incomplete character literal\n");
            exit(EXIT_FAILURE);
        case 's':
            if (peek(fp) == 'p') {
                eat_string(fp, "pace");
                peek_expect_delimiter(fp);
                return value_make_character(' ');
            }
            break;
        case 'n':
            if (peek(fp) == 'e') {
                eat_string(fp, "ewline");
                peek_expect_delimiter(fp);
                return value_make_character('\n');
            }
            break;
    }

    if (c < '!' || c > '~') {
        fprintf(stderr, "reader: non-printable character literal\n");
        exit(EXIT_FAILURE);
    }

    peek_expect_delimiter(fp);
    return value_make_character(c);
}

struct value*
read_number(FILE* fp)
{
    // temp buffer to hold the number's contents
    char buf[MAX_NUMBER_SIZE] = { 0 };
    long i = 0;

    // read characters into the buffer until:
    // EOF is reached, non-numeric char is read, or buffer is filled
    int c;
    while ((c = advance(fp)) != EOF) {
        if (!is_digit(c)) break;

        // "- 2" here to account for terminator
        if (i >= MAX_NUMBER_SIZE - 2) {
            fprintf(stderr, "reader: numeric literal larger than %d characters\n", MAX_NUMBER_SIZE);
            exit(EXIT_FAILURE);
        }

        buf[i++] = c;
    }

    // put the last non-numeric char back
    rollback(fp, c);
    peek_expect_delimiter(fp);

    long number = strtol(buf, NULL, 10);
    return value_make_number(number);
}

struct value*
read_string(FILE* fp)
{
    // temp buffer to hold the string's contents
    char buf[MAX_STRING_SIZE] = { 0 };
    long i = 0;

    // skip leading quote
    advance(fp);

    // read characters into the buffer until:
    // EOF is reached, closing quote, or buffer is filled
    int c;
    while ((c = advance(fp)) != EOF) {
        if (c == '"') break;

        // "- 2" here to account for terminator
        if (i >= MAX_STRING_SIZE - 2) {
            fprintf(stderr, "reader: string literal larger than %d characters\n", MAX_STRING_SIZE);
            exit(EXIT_FAILURE);
        }

        buf[i++] = c;
    }

    // unterminated string literal is an error
    if (c == EOF) {
        fprintf(stderr, "reader: incomplete string literal\n");
        exit(EXIT_FAILURE);
    }

    peek_expect_delimiter(fp);
    return value_make_string(buf);
}

struct value*
read_symbol(FILE* fp)
{
    // temp buffer to hold the symbol's contents
    char buf[MAX_STRING_SIZE] = { 0 };
    long i = 0;

    // grab the first character
    int c = advance(fp);
    buf[i++] = c;

    // check for peculiar identifier
    if (is_peculiar_identifier(c)) {
        if (c == '+' || c == '-') {
            peek_expect_delimiter(fp);
            return value_make_symbol(buf);
        }

        // only other peculiar identifier left at this point is "..."
        buf[i++] = advance(fp);
        buf[i++] = advance(fp);
        if (strcmp(buf, "...") == 0) {
            peek_expect_delimiter(fp);
            return value_make_symbol(buf);
        }

        fprintf(stderr, "reader: invalid symbol: %s\n", buf);
        exit(EXIT_FAILURE);
    }

    // at this point, the first char in buf must be an 'initial'

    // read characters into the buffer until:
    // EOF is reached, non-subsequent char is read, or buffer is filled
    while ((c = advance(fp)) != EOF) {
        if (!is_subsequent(c)) break;

        // "- 2" here to account for terminator
        if (i >= MAX_SYMBOL_SIZE - 2) {
            fprintf(stderr, "reader: symbol larger than %d characters\n", MAX_SYMBOL_SIZE);
            exit(EXIT_FAILURE);
        }

        buf[i++] = c;
    }

    // put non-symbol char back and ensure it was a delimiter
    rollback(fp, c);
    peek_expect_delimiter(fp);
    return value_make_symbol(buf);
}

struct value*
read_pair(FILE* fp)
{
    eat_whitespace(fp);

    // return the empty list upon finding a closing paren
    if (peek(fp) == ')') {
        advance(fp);
        return value_make_empty_list();
    }

    // read the first half of the pair
    struct value* car = reader_read(fp);
    eat_whitespace(fp);

    // check for an "improper" list
    if (peek(fp) == '.') {
        advance(fp);
        peek_expect_delimiter(fp);

        // read the last expr
        struct value* cdr = reader_read(fp);
        eat_whitespace(fp);

        // ensure a closing paren comes next
        if (peek(fp) != ')') {
            fprintf(stderr, "reader: expected closing paren after last expr in improper list\n");
            exit(EXIT_FAILURE);
        }

        // consume the closing paren
        advance(fp);
        return cons(car, cdr);
    }

    // read the next expr in a "normal" list
    struct value* cdr = read_pair(fp);
    return cons(car, cdr);
}

struct value*
reader_read(FILE* fp)
{
    assert(fp != NULL);

    eat_whitespace(fp);
    int c = peek(fp);

    if (c == EOF) {
        return value_make_eof();
    }

    // sharp expr: boolean, character, vector, etc
    if (c == '#') {
        advance(fp);  // skip sharp
        c = advance(fp);
        if (c == 't') {
            return value_make_boolean(true);
        } else if (c == 'f') {
            return value_make_boolean(false);
        } else if (c == '\\') {
            return read_character(fp);
        // TODO: read vectors
//        } else if (c == '(') {
//            return read_vector(fp);
        } else {
            fprintf(stderr, "reader: invalid sharp expression\n");
            exit(EXIT_FAILURE);
        }
    }

    // numeric literal
    if (is_digit(c)) {
        return read_number(fp);
    }

    // string literal
    if (c == '"') {
        return read_string(fp);
    }

    // symbol
    if (is_initial(c) || is_peculiar_identifier(c)) {
        return read_symbol(fp);
    }

    // quoted expr
    if (c == '\'') {
        advance(fp);  // skip quote
        struct value* exp = reader_read(fp);
        return list_make(2, value_make_symbol("quote"), exp);
    }

    // quasiquoted expr
    if (c == '`') {
        advance(fp);  // skip quasiquote
        struct value* exp = reader_read(fp);
        return list_make(2, value_make_symbol("quasiquote"), exp);
    }

    // unquote / unquote-splicing expr
    if (c == ',') {
        advance(fp);  // skip unquote
        if (peek(fp) == '@') {
            advance(fp);  // skip splicing
            struct value* exp = reader_read(fp);
            return list_make(2, value_make_symbol("unquote-splicing"), exp);
        }

        struct value* exp = reader_read(fp);
        return list_make(2, value_make_symbol("unquote"), exp);
    }

    // pair / list / s-expression
    if (c == '(') {
        advance(fp);  // skip opening paren
        return read_pair(fp);
    }

    fprintf(stderr, "reader: invalid expression\n");
    exit(EXIT_FAILURE);
}
