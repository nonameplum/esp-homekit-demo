#ifndef udp_command_h
#define udp_command_h

#include <stdlib.h>
#include <stdio.h>

typedef void (*udp_command_callback_fn)(char *result, int result_size, char *param);

typedef struct _udp_command {
    char *command_name;
    udp_command_callback_fn callback;
    struct _udp_command *next;
} udp_command_t;

int udp_command_add(char *command_name, udp_command_callback_fn callback);
void udp_command_server_task_start(int udp_port);
void udp_command_server_task_start_with_default_commands(int udp_port);

#endif /* udp_command_h */