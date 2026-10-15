## Yapscript Example:
```
@set(player_name, John)
[start]
Jane: Hi there, @player_name!
@player_name: Hello!
Jane: How are you?
   I'm doing well -> [doing well]
   Goodbye -> [leaving]

[doing well]
Jane: Good to here!

[leaving]
Jane: see you later, @player_name!

@end_scene
```

## Implementation Example:
```c
#include <yapscript.h>

void err_handler(const char *msg) { fprintf(stderr, "ERR: %s\n", msg); }
void function_handler(const char *function_name, size_t argc, const char **argv,
                      StringBuilder *output) {
  if (strcmp(function_name, "player_name") == 0) {
    sb_append(output, "Bob");
  } else if (strcmp(function_name, "print") == 0) {
    for (size_t i = 0; i < argc; i++) printf("%s\n", argv[i]);
  } else if (strcmp(function_name, "print") == 0) {
    printf("UNDEFINED %s\n", function_name);
  }
}

int main(int argc, char **argv) {
  ParsingState ps = ps_new("[start]\nJohn: Hello!");
  ps.error_handler = &err_handler;
  ps.function_handler = &function_handler;
  while (true) {
    Message msg = ps_next_message(&ps);
    if (!msg.text)
      break;
    printf("%s says %s\n", msg.name, msg.text);
    for (size_t i = 0; i < msg.response_count; i++)
      printf("   can respond %s jumping to %s\n", msg.responses[i],
             msg.response_destinations[i]);
    msg_free(&msg);
  }
  ps_jump_to(&ps, "[start]");
  return 0;
}
```
