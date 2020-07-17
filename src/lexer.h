#ifndef SQUEAKY_LEXER_H_INCLUDED
#define SQUEAKY_LEXER_H_INCLUDED

enum token_type {
    TOKEN_TYPE_UNDEFINED = 0,
    TOKEN_TYPE_FIXNUM,
    TOKEN_TYPE_BOOLEAN,
    TOKEN_TYPE_CHARACTER,
    TOKEN_TYPE_STRING,
    TOKEN_TYPE_LPAREN,
    TOKEN_TYPE_RPAREN,
};

struct token {
    int type;
    const char* value;
    long len;
};

struct lexer {
    const char* input;
    long input_len;
    long start;
    long pos;
    long width;
};

enum lexer_status {
    LEXER_OK = 0,
    LEXER_EOF,
    LEXER_ERROR,
};

void lexer_print(const struct token* token);
int lexer_lex(struct lexer* lexer, struct token* token);

#endif
