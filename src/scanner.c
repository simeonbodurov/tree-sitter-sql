#include "tree_sitter/parser.h"
#include <stdlib.h>
#include <string.h>
#include <wctype.h>

enum TokenType {
  DOLLAR_QUOTED_STRING_START_TAG,
  DOLLAR_QUOTED_STRING_END_TAG,
  DOLLAR_QUOTED_STRING
};

#define MALLOC_STRING_SIZE 1024

typedef struct LexerState {
  char* start_tag;
} LexerState;

void *tree_sitter_sql_external_scanner_create() {
  LexerState *state = malloc(sizeof(LexerState));
  state->start_tag = NULL;
  return state;
}

void tree_sitter_sql_external_scanner_destroy(void *payload) {
  LexerState *state = (LexerState*)payload;
  if (state->start_tag != NULL) {
    free(state->start_tag);
    state->start_tag = NULL;
  }
  free(payload);
}

static char* add_char(char* text, size_t* text_size, char c, int index) {
  if (text == NULL) {
    text = malloc(sizeof(char) * MALLOC_STRING_SIZE);
    *text_size = MALLOC_STRING_SIZE;
  }

  // will break when indexes advances more than MALLOC_STRING_SIZE
  if (index + 1 >= *text_size) {
    *text_size += MALLOC_STRING_SIZE;
    char* tmp = malloc(*text_size * sizeof(char));
    strncpy(tmp, text, *text_size);
    free(text);
    text = tmp;
  }

  text[index] = c;
  text[index + 1] = '\0';
  return text;
}

// Scans a full dollar-string tag starting from '$' (e.g. reads "$a$").
// Returns the tag string, or NULL if not a valid tag.
static char* scan_dollar_string_tag(TSLexer *lexer) {
  char* tag = NULL;
  int index = 0;
  size_t* text_size = malloc(sizeof(size_t));
  *text_size = 0;
  if (lexer->lookahead == '$') {
    tag = add_char(tag, text_size, '$', index);
    lexer->advance(lexer, false);
  } else {
    free(text_size);
    return NULL;
  }

  while (lexer->lookahead != '$' && !iswspace(lexer->lookahead) && !lexer->eof(lexer)) {
    tag = add_char(tag, text_size, lexer->lookahead, ++index);
    lexer->advance(lexer, false);
  }

  if (lexer->lookahead == '$') {
    tag = add_char(tag, text_size, lexer->lookahead, ++index);
    lexer->advance(lexer, false);
    free(text_size);
    return tag;
  } else {
    free(tag);
    free(text_size);
    return NULL;
  }
}

// Scans the tag name and closing '$' AFTER the opening '$' has already been
// consumed.  Returns the full tag string (e.g. "$a$"), or NULL if there is no
// valid closing '$' (e.g. whitespace interrupts the tag name).
static char* scan_dollar_tag_body(TSLexer *lexer) {
  char* tag = NULL;
  int index = 0;
  size_t* text_size = malloc(sizeof(size_t));
  *text_size = 0;

  tag = add_char(tag, text_size, '$', index);   // prepend the already-consumed '$'

  while (lexer->lookahead != '$' && !iswspace(lexer->lookahead) && !lexer->eof(lexer)) {
    tag = add_char(tag, text_size, lexer->lookahead, ++index);
    lexer->advance(lexer, false);
  }

  if (lexer->lookahead == '$') {
    tag = add_char(tag, text_size, '$', ++index);
    lexer->advance(lexer, false);
    free(text_size);
    return tag;
  } else {
    free(tag);
    free(text_size);
    return NULL;
  }
}

bool tree_sitter_sql_external_scanner_scan(void *payload, TSLexer *lexer, const bool *valid_symbols) {
  LexerState *state = (LexerState*)payload;
  if (valid_symbols[DOLLAR_QUOTED_STRING_START_TAG] && state->start_tag == NULL) {
    while (iswspace(lexer->lookahead)) lexer->advance(lexer, true);

    char* start_tag = scan_dollar_string_tag(lexer);
    if (start_tag == NULL) {
      return false;
    }
    if (state->start_tag != NULL) {
      free(state->start_tag);
      state->start_tag = NULL;
    }
    state->start_tag = start_tag;
    lexer->result_symbol = DOLLAR_QUOTED_STRING_START_TAG;
    return true;
  }

  // When both DOLLAR_QUOTED_STRING and DOLLAR_QUOTED_STRING_END_TAG are valid
  // (i.e. inside a dollar-quoted function body where an embedded literal is
  // also grammatically possible), we try DOLLAR_QUOTED_STRING first so that
  // a tag different from the outer one (e.g. $b$ inside $a$…$a$) is consumed
  // as a literal rather than being rejected by END_TAG.  When the scanned tag
  // IS the outer tag we emit END_TAG directly — it has already been consumed.
  //
  // mark_end is called after the opening '$' so that on failure tree-sitter
  // resets to just after '$', leaving the tag name as the first unexpected
  // character rather than '$' itself.
  if (valid_symbols[DOLLAR_QUOTED_STRING]) {
    while (iswspace(lexer->lookahead)) lexer->advance(lexer, true);
    if (lexer->lookahead != '$') return false;
    lexer->advance(lexer, false);   // consume opening '$'
    lexer->mark_end(lexer);         // reset point on failure: after opening '$'

    char* start_tag = scan_dollar_tag_body(lexer);
    if (start_tag == NULL) {
      return false;
    }

    if (state->start_tag != NULL && strcmp(state->start_tag, start_tag) == 0) {
      // This tag closes the outer dollar-quote block.  Emit END_TAG now;
      // the full tag has been consumed so the END_TAG block below is skipped.
      free(start_tag);
      if (valid_symbols[DOLLAR_QUOTED_STRING_END_TAG]) {
        free(state->start_tag);
        state->start_tag = NULL;
        lexer->mark_end(lexer);
        lexer->result_symbol = DOLLAR_QUOTED_STRING_END_TAG;
        return true;
      }
      return false;
    }

    char* end_tag = NULL;
    while (true) {
      if (lexer->eof(lexer)) {
        free(start_tag);
        free(end_tag);
        return false;
      }

      end_tag = scan_dollar_string_tag(lexer);
      if (end_tag == NULL) {
        lexer->advance(lexer, false);
        continue;
      }

      if (strcmp(end_tag, start_tag) == 0) {
        free(start_tag);
        free(end_tag);
        lexer->mark_end(lexer);
        lexer->result_symbol = DOLLAR_QUOTED_STRING;
        return true;
      }

      free(end_tag);
      end_tag = NULL;
    }
  }

  // Standalone END_TAG path — only reached when DOLLAR_QUOTED_STRING is not
  // valid.  Same mark_end-after-'$' convention so that on mismatch tree-sitter
  // resets to after '$', not before it.
  if (valid_symbols[DOLLAR_QUOTED_STRING_END_TAG] && state->start_tag != NULL) {
    while (iswspace(lexer->lookahead)) lexer->advance(lexer, true);
    if (lexer->lookahead != '$') return false;
    lexer->advance(lexer, false);   // consume opening '$'
    lexer->mark_end(lexer);         // reset point on failure: after opening '$'

    char* end_tag = scan_dollar_tag_body(lexer);
    if (end_tag != NULL && strcmp(end_tag, state->start_tag) == 0) {
      free(state->start_tag);
      state->start_tag = NULL;
      lexer->mark_end(lexer);
      lexer->result_symbol = DOLLAR_QUOTED_STRING_END_TAG;
      free(end_tag);
      return true;
    }
    if (end_tag != NULL) {
      free(end_tag);
    }
    return false;
  }

  return false;
}

unsigned tree_sitter_sql_external_scanner_serialize(void *payload, char *buffer) {
  LexerState *state = (LexerState *)payload;
  if (state == NULL || state->start_tag == NULL) {
    return 0;
  }
  // + 1 for the '\0'
  int tag_length = strlen(state->start_tag) + 1;
  if (tag_length >= TREE_SITTER_SERIALIZATION_BUFFER_SIZE) {
    return 0;
  }

  memcpy(buffer, state->start_tag, tag_length);
  if (state->start_tag != NULL) {
    free(state->start_tag);
    state->start_tag = NULL;
  }
  return tag_length;
}

void tree_sitter_sql_external_scanner_deserialize(void *payload, const char *buffer, unsigned length) {
  LexerState *state = (LexerState *)payload;
  state->start_tag = NULL;
  // A length of 1 can't exists.
  if (length > 1) {
    state->start_tag = malloc(length);
    memcpy(state->start_tag, buffer, length);
  }
}
