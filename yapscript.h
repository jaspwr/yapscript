/////////////////////////////////////////////////////
//           ⡀⢀ ⢀⣀ ⣀⡀ ⢀⣀ ⢀⣀ ⡀⣀ ⠄ ⣀⡀ ⣰⡀           //
//           ⣑⡺ ⠣⠼ ⡧⠜ ⠭⠕ ⠣⠤ ⠏  ⠇ ⡧⠜ ⠘⠤           //
//                                                 //
//  Header only game dialogue scripting language.  //
//                                                 //
// This software is provided under the MIT License //
//               Copyright (c) 2026                //
/////////////////////////////////////////////////////

//
// Yapscript Example:
// ```
// @set(player_name, John)
// [start]
// Jane: Hi there, @player_name!
// @player_name: Hello!
// Jane: How are you?
//    I'm doing well -> [doing well]
//    Goodbye -> [leaving]
//
// [doing well]
// Jane: Good to here!
//
// [leaving]
// Jane: see you later, @player_name!
//
// @end_scene
// ```
//
// Implementation Example:
// ```
// #include <yapscript.h>
//
// void err_handler(const char *msg) { fprintf(stderr, "ERR: %s\n", msg); }
// void function_handler(const char *function_name, size_t argc, const char
// **argv,
//                       StringBuilder *output) {
//   if (strcmp(function_name, "player_name") == 0) {
//     sb_append(output, "Bob");
//   } else if (strcmp(function_name, "print") == 0) {
//     for (size_t i = 0; i < argc; i++) printf("%s\n", argv[i]);
//   } else {
//     printf("UNDEFINED %s\n", function_name);
//   }
// }
//
// int main(int argc, char **argv) {
//   ParsingState ps = ps_new("[start]\nJohn: Hello!");
//   ps.error_handler = &err_handler;
//   ps.function_handler = &function_handler;
//   while (true) {
//     Message msg = ps_next_message(&ps);
//     if (!msg.text)
//       break;
//     printf("%s says %s\n", msg.name, msg.text);
//     for (size_t i = 0; i < msg.response_count; i++)
//       printf("   can respond %s jumping to %s\n", msg.responses[i],
//              msg.response_destinations[i]);
//     msg_free(&msg);
//   }
//   ps_jump_to(&ps, "[start]");
//   return 0;
// }
// ```

#ifndef YAPSCRIPT_INCLUDE
#define YAPSCRIPT_INCLUDE

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARROW "->"
#define COLON ":"
#define AT "@"
#define OPEN_BRACKET "("
#define CLOSE_BRACKET ")"
#define COMMA ","

static inline bool is_alphanum_underscore(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
         (c >= '0' && c <= '9') || c == '_';
}

static inline bool is_newline(char c) { return c == '\n' || c == '\r'; }

static inline bool is_whitespace(char c) { return c == ' ' || c == '\t'; }

static inline bool is_eof(char c) { return c == '\0'; }

typedef struct {
  char *data;
  size_t length;
  size_t capacity;
} StringBuilder;

static inline StringBuilder sb_new(size_t initial_capacity) {
  return (StringBuilder){
      .data = (char *)calloc(1, initial_capacity),
      .length = 0,
      .capacity = initial_capacity,
  };
}

static inline void sb_free(StringBuilder *sb) { free(sb->data); }

static inline void sb_clear(StringBuilder *sb) {
  if (!sb->data) {
    sb->capacity = 64;
    sb->data = (char *)malloc(sb->capacity);
  }
  sb->length = 0;
  assert(sb->capacity > 0);
  sb->data[0] = '\0';
}

static inline void sb_append_char(StringBuilder *sb, char c) {
  if (!sb->data) {
    sb->capacity = 64;
    sb->data = (char *)malloc(sb->capacity);
  }
  if (sb->length + 2 > sb->capacity) {
    sb->capacity = (sb->length + 2) * 2;
    sb->data = (char *)realloc(sb->data, sb->capacity);
  }
  sb->data[sb->length++] = c;
  sb->data[sb->length] = '\0';
}

static inline void sb_append(StringBuilder *sb, const char *str) {
  size_t add_len = strlen(str);
  if (add_len == 0)
    return;
  if (!sb->data) {
    sb->capacity = add_len + 1;
    sb->data = (char *)malloc(sb->capacity);
  }
  if (sb->length + add_len + 1 > sb->capacity) {
    sb->capacity = (sb->length + add_len + 1) * 2;
    sb->data = (char *)realloc(sb->data, sb->capacity);
  }
  strcpy(sb->data + sb->length, str);
  sb->length += add_len;
}

static inline void sb_trim_end(StringBuilder *sb) {
  while (sb->length > 0 && is_whitespace(sb->data[sb->length - 1])) {
    sb->data[--sb->length] = '\0';
  }
}

#define MAX_ARGUMENTS 64

typedef struct {
  StringBuilder identifier;
  StringBuilder value;
} Variable;

typedef struct {
  size_t cursor;
  const char *src;
  size_t src_len;
  void (*function_handler)(const char *function_name, size_t argc,
                           const char **argv, StringBuilder *output);
  void (*error_handler)(const char *message);
  Variable variables[256];
  size_t varibale_count;
} ParsingState;

static inline ParsingState ps_new(const char *src) {
  return (ParsingState){
      .cursor = 0,
      .src = src,
      .src_len = strlen(src),
  };
}

static inline void ps_set_variable(ParsingState *ps, const char *name,
                                   const char *value) {
  for (int i = 0; i < ps->varibale_count; i++) {
    char *ident = ps->variables[i].identifier.data;
    if (strcmp(ident, name) == 0) {
      sb_clear(&ps->variables[i].value);
      sb_append(&ps->variables[i].value, value);
      return;
    }
  }

  size_t idx = ps->varibale_count++;
  sb_clear(&ps->variables[idx].identifier);
  sb_append(&ps->variables[idx].identifier, name);
  sb_clear(&ps->variables[idx].value);
  sb_append(&ps->variables[idx].value, value);
}

// Unsafe and can be reallocated. Do not hold onto pointer.
static inline const char *ps_get_variable(ParsingState *ps, const char *name) {
  for (int i = 0; i < ps->varibale_count; i++) {
    char *ident = ps->variables[i].identifier.data;
    if (strcmp(ident, name) == 0) {
      return ps->variables[i].value.data;
    }
  }

  return NULL;
}

#define MAX_RESPONSES 16
typedef struct {
  const char *name;
  const char *text;
  float speed;
  size_t response_count;
  const char *responses[MAX_RESPONSES];
  const char *response_destinations[MAX_RESPONSES];
} Message;

static inline void msg_free(Message *msg) {
  free((void *)msg->name);
  free((void *)msg->text);
  for (size_t i = 0; i < msg->response_count; i++) {
    free((void *)msg->responses[i]);
    free((void *)msg->response_destinations[i]);
  }
}

static inline char ps_peek(ParsingState *ps) {
  assert(ps->cursor < ps->src_len + 1);
  return ps->src[ps->cursor];
}

static inline char ps_advance_char(ParsingState *ps) {
  assert(ps->cursor < ps->src_len + 1);
  return ps->src[ps->cursor++];
}

static inline void ps_skip_newlines(ParsingState *ps) {
  while (is_newline(ps_peek(ps)))
    ps_advance_char(ps);
}

static inline void ps_skip_whitespace(ParsingState *ps) {
  while (is_whitespace(ps_peek(ps)))
    ps_advance_char(ps);
}

static inline bool ps_starts_with(ParsingState *ps, const char *str) {
  size_t str_len = strlen(str);
  if (str_len > ps->src_len)
    return false;
  if (ps->cursor > ps->src_len - str_len)
    return false;
  return strncmp(ps->src + ps->cursor, str, str_len) == 0;
}

static inline void ps_skip_string(ParsingState *ps, const char *str) {
  size_t len = strlen(str);
  if (ps_starts_with(ps, str)) {
    assert(ps->cursor + len <= ps->src_len);
    ps->cursor += len;
  }
}

static inline void ps_error(ParsingState *ps, const char *msg) {
  if (ps->error_handler) {
    (ps->error_handler)(msg);
  } else {
    fprintf(stderr, "ERR: %s\n", msg);
  }
}

static inline void ps_flush_function(ParsingState *ps, StringBuilder *output,
                                     StringBuilder *fn_name_buffer);

static inline void parse_expression(ParsingState *ps, StringBuilder *sb,
                                    const char **terminators,
                                    size_t terminators_count) {
  ps_skip_whitespace(ps);
  bool in_fn_name = false;

  StringBuilder fn_name_buffer = {};
  while (!is_newline(ps_peek(ps)) && !is_eof(ps_peek(ps))) {
    for (size_t i = 0; i < terminators_count; i++) {
      if (ps_starts_with(ps, terminators[i]))
        goto outer_break;
    }

    if (ps_starts_with(ps, AT)) {
      ps_skip_string(ps, AT);
      in_fn_name = true;
    } else if (in_fn_name && is_alphanum_underscore(ps_peek(ps))) {
      sb_append_char(&fn_name_buffer, ps_advance_char(ps));
    } else if (in_fn_name) {
      ps_flush_function(ps, sb, &fn_name_buffer);
      in_fn_name = false;
    } else {
      sb_append_char(sb, ps_advance_char(ps));
    }
  }
outer_break:
  if (in_fn_name) {
    ps_flush_function(ps, sb, &fn_name_buffer);
  }
  sb_free(&fn_name_buffer);
  sb_trim_end(sb);
}

static inline bool builtin_function_handler(const char *function_name,
                                            size_t argc, const char **argv,
                                            StringBuilder *output,
                                            ParsingState *ps) {
  const char *from_var = ps_get_variable(ps, function_name);
  if (from_var) {
    sb_append(output, from_var);
    return true;
  }

  if (strcmp(function_name, "set") == 0) {
    assert(argc == 2);
    ps_set_variable(ps, argv[0], argv[1]);
  } else if (strcmp(function_name, "print") == 0) {
    for (size_t i = 0; i < argc; i++) {
      printf("%s\n", argv[i]);
    }
  } else if (strcmp(function_name, "caps") == 0) {
    assert(argc == 1);
    const char *arg = argv[0];
    for (size_t i = 0; arg[i] != '\0'; i++) {
      char c = arg[i];
      if (c >= 'a' && c <= 'z')
        c += ('A' - 'a');
      sb_append_char(output, c);
    }
  } else if (strcmp(function_name, "add") == 0) {
    assert(argc == 2);
    char buf[60];
    itoa(atoi(argv[0]) + atoi(argv[1]), buf, 10);
    for (size_t i = 0; buf[i] != '\0'; i++) {
      sb_append_char(output, buf[i]);
    }
  } else {
    return false;
  }

  return true;
}

static inline void ps_flush_function(ParsingState *ps, StringBuilder *output,
                                     StringBuilder *fn_name_buffer) {
  if (fn_name_buffer->length == 0) {
    ps_error(ps, "Empty function name");
    return;
  }

  StringBuilder fn_arg_buffers[MAX_ARGUMENTS] = {};

  size_t argument_count = 0;
  if (ps_starts_with(ps, OPEN_BRACKET)) {
    ps_skip_string(ps, OPEN_BRACKET);
    const char *TERMINATORS[2] = {COMMA, CLOSE_BRACKET};
    while (!is_eof(ps_peek(ps)) && !ps_starts_with(ps, CLOSE_BRACKET)) {
      if (argument_count >= MAX_ARGUMENTS) {
        ps_error(ps, "Max arguments exceeded");
        return;
      }

      sb_clear(&fn_arg_buffers[argument_count]);

      size_t previous_cursor = ps->cursor;

      parse_expression(ps, &fn_arg_buffers[argument_count], TERMINATORS, 2);

      if (ps->cursor == previous_cursor) {
        ps_error(ps, "Expected function argument or ')'");
        break;
      }

      ps_skip_string(ps, COMMA);
      argument_count++;
    }
    ps_skip_string(ps, CLOSE_BRACKET);
  }

  const char *argv[MAX_ARGUMENTS] = {};
  for (size_t i = 0; i < argument_count; i++) {
    argv[i] = fn_arg_buffers[i].data;
  }
  bool used_builtin = builtin_function_handler(
      fn_name_buffer->data, argument_count, argv, output, ps);

  if (ps->function_handler) {
    (ps->function_handler)(fn_name_buffer->data, argument_count, argv, output);
  }

  for (size_t i = 0; i < argument_count; i++) {
    sb_free(&fn_arg_buffers[i]);
  }

  sb_clear(fn_name_buffer);
}

static inline void ps_jump_to(ParsingState *ps, const char *label) {
  size_t label_len = strlen(label);

  if (label_len < 2 || label[0] != '[' || label[label_len - 1] != ']') {
    ps_error(ps, "Incorrect label format. Should be [label]");
    return;
  }

  bool line_start = true;

  if (label_len > ps->src_len) {
    ps_error(ps, "Label not found");
    return;
  }

  for (size_t cursor = 0; cursor <= ps->src_len - label_len; cursor++) {
    if (line_start && strncmp(ps->src + cursor, label, label_len) == 0) {
      ps->cursor = cursor;
      return;
    }
    char c = *(ps->src + cursor);
    if (is_newline(c))
      line_start = true;
    else if (!is_whitespace(c))
      line_start = false;
  }

  ps_error(ps, "Label not found");
}

static inline void parse_response(ParsingState *ps, Message *msg) {
  ps_skip_whitespace(ps);

  StringBuilder response_text = sb_new(64);
  StringBuilder destination_text = sb_new(64);

  const char *TERMINATORS[1] = {ARROW};
  parse_expression(ps, &response_text, TERMINATORS, 1);

  ps_skip_string(ps, ARROW);

  ps_skip_whitespace(ps);

  parse_expression(ps, &destination_text, NULL, 0);

  sb_trim_end(&response_text);
  sb_trim_end(&destination_text);
  msg->responses[msg->response_count] = response_text.data;
  msg->response_destinations[msg->response_count] = destination_text.data;
  msg->response_count++;
}

static inline Message ps_next_message(ParsingState *ps) {
  ps_skip_newlines(ps);

  ps_skip_whitespace(ps);

  if (ps_peek(ps) == '\0')
    return (Message){};

  StringBuilder name = sb_new(64);
  StringBuilder text = sb_new(64);
  Message msg = (Message){};

  const char *TERMINATORS[1] = {COLON};
  parse_expression(ps, &name, TERMINATORS, 1);

  sb_trim_end(&name);

  if (ps_starts_with(ps, COLON)) {
    ps_skip_string(ps, COLON);

    parse_expression(ps, &text, NULL, 0);

    // Get responses
    while (!is_eof(ps_peek(ps)) && msg.response_count < MAX_RESPONSES) {
      ps_skip_newlines(ps);
      bool indented = is_whitespace(ps_peek(ps));
      if (indented) {
        parse_response(ps, &msg);
      } else {
        break;
      }
    }
  } else {
    // Line had no colon and was not a message; Try next line
    sb_free(&name);
    sb_free(&text);
    return ps_next_message(ps);
  }

  msg.name = name.data;
  msg.text = text.data;
  return msg;
}
#endif
