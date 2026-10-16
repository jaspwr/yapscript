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
// #define YAPSCRIPT_STRIP_PREFIXES
// #include <yapscript.h>
//
// void err_handler(const char *msg, void *user_data) { fprintf(stderr, "ERR:
// %s\n", msg); } void function_handler(const char *function_name, size_t argc,
// const char **argv,
//                       StringBuilder *output, void *user_data) {
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

#define Vec(T)                                                                 \
  struct {                                                                     \
    T *data;                                                                   \
    size_t length;                                                             \
    size_t capacity;                                                           \
  }
#define da_free(da) free((da).data)
#define da_push(da, val)                                                       \
  do {                                                                         \
    if ((da).length >= (da).capacity) {                                        \
      (da).capacity = (da).capacity == 0 ? 4 : (da).capacity * 2;              \
      *(void **)&(da).data =                                                   \
          realloc((da).data, (da).capacity * sizeof(*(da).data));              \
    }                                                                          \
    (da).data[(da).length++] = (val);                                          \
  } while (0)

static inline bool ys_is_alphanum_underscore(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
         (c >= '0' && c <= '9') || c == '_';
}

#define is_alphanum_underscore ys_is_alphanum_underscore
#define is_newline ys_is_newline
#define is_whitespace ys_is_whitespace
#define is_eof ys_is_eof

#define StringBuilder YsStringBuilder
#define ParsingState YsParsingState
#define Response YsResponse
#define Message YsMessage
#define Variable YsVariable

static inline bool ys_is_newline(char c) { return c == '\n' || c == '\r'; }

static inline bool ys_is_whitespace(char c) { return c == ' ' || c == '\t'; }

static inline bool ys_is_eof(char c) { return c == '\0'; }

typedef struct {
  char *data;
  size_t length;
  size_t capacity;
} YsStringBuilder;

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

static inline void sb_replace(StringBuilder *sb, const char *str) {
  sb_clear(sb);
  sb_append(sb, str);
}

static inline void sb_trim_end(StringBuilder *sb) {
  while (sb->length > 0 && is_whitespace(sb->data[sb->length - 1])) {
    sb->data[--sb->length] = '\0';
  }
}

typedef struct {
  StringBuilder identifier;
  StringBuilder value;
} YsVariable;

typedef struct {
  size_t cursor;
  const char *src;
  size_t src_len;
  void (*function_handler)(const char *function_name, size_t argc,
                           const char **argv, StringBuilder *output,
                           void *user_data);
  void (*error_handler)(const char *message, void *user_data);
  Vec(Variable) variables;
  void *user_data;
} YsParsingState;

static inline ParsingState ps_new(const char *src) {
  return (ParsingState){
      .cursor = 0,
      .src = src,
      .src_len = strlen(src),
  };
}

static inline void ps_set_variable(ParsingState *ps, const char *name,
                                   const char *value) {
  for (int i = 0; i < ps->variables.length; i++) {
    char *ident = ps->variables.data[i].identifier.data;
    if (strcmp(ident, name) == 0) {
      sb_clear(&ps->variables.data[i].value);
      sb_append(&ps->variables.data[i].value, value);
      return;
    }
  }

  Variable new_var = (Variable){};
  sb_replace(&new_var.identifier, name);
  sb_replace(&new_var.value, value);
  da_push(ps->variables, new_var);
}

// Unsafe and can be reallocated. Do not hold onto pointer.
static inline const char *ps_get_variable(ParsingState *ps, const char *name) {
  for (int i = 0; i < ps->variables.length; i++) {
    char *ident = ps->variables.data[i].identifier.data;
    if (strcmp(ident, name) == 0) {
      return ps->variables.data[i].value.data;
    }
  }

  return NULL;
}

typedef struct {
  const char *message;
  const char *destination;
} YsResponse;

static inline void response_free(Response *r) {
  free((void *)r->message);
  free((void *)r->destination);
}

typedef struct {
  const char *name;
  const char *text;
  float speed;
  Vec(Response) responses;
} YsMessage;

static inline void msg_free(Message *msg) {
  free((void *)msg->name);
  free((void *)msg->text);
  for (size_t i = 0; i < msg->responses.length; i++) {
    response_free(&msg->responses.data[i]);
  }
  da_free(msg->responses);
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
    (ps->error_handler)(msg, ps->user_data);
  } else {
    fprintf(stderr, "ERR: %s\n", msg);
  }
}

static inline void ps_flush_function(ParsingState *ps, StringBuilder *output,
                                     StringBuilder *fn_name_buffer);

static inline void __parse_expression(ParsingState *ps, StringBuilder *sb,
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

static inline bool __builtin_function_handler(const char *function_name,
                                              size_t argc, const char **argv,
                                              StringBuilder *output,
                                              ParsingState *ps) {
  const char *from_var = ps_get_variable(ps, function_name);
  if (from_var) {
    size_t len = strlen(from_var);
    if (len > 0) {
      sb_append(output, from_var);
      return true;
    }
  }

  if (strcmp(function_name, "set") == 0) {
    if (argc != 2) {
      ps_error(ps, "`set` takes 2 arguments. Usage: set(identifier, value)");
      return true;
    }
    ps_set_variable(ps, argv[0], argv[1]);
  } else if (strcmp(function_name, "print") == 0) {
    for (size_t i = 0; i < argc; i++) {
      printf("%s\n", argv[i]);
    }
  } else if (strcmp(function_name, "caps") == 0) {
    if (argc != 1) {
      ps_error(ps, "`caps` takes 1 argument. Usage: caps(value)");
      return true;
    }
    const char *arg = argv[0];
    for (size_t i = 0; !is_eof(arg[i]); i++) {
      char c = arg[i];
      if (c >= 'a' && c <= 'z')
        c += ('A' - 'a');
      sb_append_char(output, c);
    }
  } else if (strcmp(function_name, "eq") == 0) {
    if (argc != 2) {
      ps_error(ps, "`eq` takes 2 arguments. Usage: eq(a, b)");
      return true;
    }

    if (strcmp(argv[0], argv[1]) == 0)
      sb_append_char(output, '1');
  } else if (strcmp(function_name, "if") == 0) {
    if (argc != 3) {
      ps_error(
          ps,
          "`if` takes 3 arguments. Usage: if(cond, true_branch, false_branch)");
      return true;
    }
    if (strlen(argv[0]) > 0 && strcmp(argv[0], "0") != 0)
      sb_append(output, argv[1]);
    else
      sb_append(output, argv[2]);
  } else if (strcmp(function_name, "add") == 0) {
    if (argc != 2) {
      ps_error(ps, "`add` takes 2 arguments. Usage: add(a, b)");
      return true;
    }
    char buf[60] = {};
    itoa(atoi(argv[0]) + atoi(argv[1]), buf, 10);
    for (size_t i = 0; !is_eof(buf[i]); i++)
      sb_append_char(output, buf[i]);
  } else if (strcmp(function_name, "gt") == 0) {
    if (argc != 2) {
      ps_error(ps, "`gt` takes 2 arguments. Usage: gt(a, b)");
      return true;
    }
    char buf[60] = {};
    itoa(atoi(argv[0]) > atoi(argv[1]), buf, 10);
    for (size_t i = 0; !is_eof(buf[i]); i++)
      sb_append_char(output, buf[i]);
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

  Vec(StringBuilder) arguments = {};

  if (ps_starts_with(ps, OPEN_BRACKET)) {
    ps_skip_string(ps, OPEN_BRACKET);
    const char *TERMINATORS[2] = {COMMA, CLOSE_BRACKET};
    while (!is_eof(ps_peek(ps)) && !ps_starts_with(ps, CLOSE_BRACKET)) {
      StringBuilder arg = {};
      sb_clear(&arg);

      size_t previous_cursor = ps->cursor;

      __parse_expression(ps, &arg, TERMINATORS, 2);

      if (ps->cursor == previous_cursor && !ps_starts_with(ps, COMMA)) {
        ps_error(ps, "Expected function argument or ')'");
        break;
      }

      ps_skip_string(ps, COMMA);
      da_push(arguments, arg);
    }
    ps_skip_string(ps, CLOSE_BRACKET);
  }

  const char *argv[64] = {};
  for (size_t i = 0; i < arguments.length; i++) {
    argv[i] = arguments.data[i].data;
  }
  bool used_builtin = __builtin_function_handler(
      fn_name_buffer->data, arguments.length, argv, output, ps);

  if (!used_builtin && ps->function_handler) {
    (ps->function_handler)(fn_name_buffer->data, arguments.length, argv, output,
                           ps->user_data);
  }

  for (size_t i = 0; i < arguments.length; i++) {
    sb_free(&arguments.data[i]);
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

static inline void __parse_response(ParsingState *ps, Message *msg) {
  ps_skip_whitespace(ps);

  StringBuilder response_text = sb_new(64);
  StringBuilder destination_text = sb_new(64);

  const char *TERMINATORS[1] = {ARROW};
  __parse_expression(ps, &response_text, TERMINATORS, 1);

  ps_skip_string(ps, ARROW);

  ps_skip_whitespace(ps);

  __parse_expression(ps, &destination_text, NULL, 0);

  sb_trim_end(&response_text);
  sb_trim_end(&destination_text);

  Response r = (Response){
      .message = response_text.data,
      .destination = destination_text.data,
  };
  da_push(msg->responses, r);
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
  __parse_expression(ps, &name, TERMINATORS, 1);

  sb_trim_end(&name);

  if (ps_starts_with(ps, COLON)) {
    ps_skip_string(ps, COLON);

    __parse_expression(ps, &text, NULL, 0);

    // Get responses
    while (!is_eof(ps_peek(ps))) {
      ps_skip_newlines(ps);
      bool indented = is_whitespace(ps_peek(ps));
      if (indented) {
        __parse_response(ps, &msg);
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

#undef Vec
#undef da_free
#undef da_push
#undef ARROW
#undef COLON
#undef AT
#undef OPEN_BRACKET
#undef CLOSE_BRACKET
#undef COMMA

#undef is_alphanum_underscore
#undef is_newline
#undef is_whitespace
#undef is_eof

#ifndef YAPSCRIPT_STRIP_PREFIXES
#undef StringBuilder
#undef ParsingState
#undef Response
#undef Message
#undef Variable
#endif

#endif
