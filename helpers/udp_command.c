#include "udp_command.h"

#include <stdio.h>
#include <string.h>
#include "espressif/esp_common.h"
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>
#include <debug_helper.h>
#include <homekit/homekit.h>

#define BUFLEN 512

udp_command_t *udp_commands = NULL;
int udp_command_port = 123456;

// Default commands

void udp_command_system_restart(char *result, int result_size, char *param) {
    sdk_system_restart();
}

void udp_command_homekit_reset(char *result, int result_size, char *param) {
    homekit_server_reset();
    sdk_system_restart();
}

void udp_command_gpio_read(char *result, int result_size, char *param) {
    if (param) {
        int gpio_num = atoi(param);
        char gpio_result[12];
        bool value = gpio_read(gpio_num);
        sprintf(gpio_result, "gpio[%d]: %d", gpio_num, value);
        strlcpy(result, gpio_result, result_size);
    } else {
        strlcpy(result, "Wrong param: (null)", result_size);
    }
}

void udp_command_log_enable(char *result, int result_size, char *param) {
    if (param) {
        int enable = atoi(param);
        debug_helper_log_enabled = enable;
        char *enabled = debug_helper_log_enabled ? "log_enabled" : "log_disabled";
        strlcpy(result, enabled, result_size);
    } else {
        strlcpy(result, "Wrong param: (null)", result_size);
    }
}

// Private udp command access helpers

static udp_command_t *udp_command_find_by_name(const char *command_name) {
    udp_command_t *udp_command = udp_commands;
    while (udp_command && strcmp(udp_command->command_name, command_name) != 0)
        udp_command = udp_command->next;

    return udp_command;
}

// Server task

static void udp_command_server_task(void *pvParameters) {
    LOG("Start udp command server with port: %d", udp_command_port);
    struct sockaddr_in myaddr;  // address of the server
    struct sockaddr_in claddr;  // address of the client
    memset((char *)&myaddr, 0, sizeof(myaddr));
    myaddr.sin_family = AF_INET;
    myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    myaddr.sin_port = htons(udp_command_port);

    int ret;
    int fd = lwip_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);  // Create a UDP socket.
    // fd = lwip_socket(AF_INET, SOCK_DGRAM, 0 ); // Create a UDP socket.
    LWIP_ASSERT("fd >= 0", fd >= 0);

    // bind server address to socket descriptor
    ret = bind(fd, (struct sockaddr *)&myaddr, sizeof(myaddr));
    LWIP_ASSERT("ret >= 0", ret >= 0);

    char buf[BUFLEN];
    long recvlen;
    unsigned int addrlen = sizeof(claddr);

    while (true) {
        // receive the datagram
        recvlen = recvfrom(fd, buf, BUFLEN, 0, (struct sockaddr *)&claddr, &addrlen);
        LOG("Received %ld bytes", recvlen);
        if (recvlen > 0) {
            buf[recvlen] = 0;
            LOG("Client IP %s, PORT %u", inet_ntoa(claddr.sin_addr), ntohs(claddr.sin_port));
            LOG("Received message: [%s]", buf);

            // remove trailing new line
            buf[strcspn(buf, "\n")] = 0;

            char command_str[BUFLEN];
            strcpy(command_str, buf);

            // Split by ':' to get the command name and the argument
            char *separator = strchr(command_str, ':');
            char *argument = "";
            bool separator_found = false;
            if (separator) {
                separator_found = true;
                argument = separator;
                argument++;
                *separator = 0;
            }
            int argument_len = strlen(argument);

            if (separator_found && argument_len == 0) {
                strlcat(buf, " - ARG", BUFLEN);
                sendto(fd, buf, strlen(buf), 0, (struct sockaddr *)&claddr, sizeof(claddr));
                LOG("upd command wrong format: '%s'", buf);
            } else {
                udp_command_t *command = udp_command_find_by_name(command_str);
                if (command) {
                    strlcat(buf, " - OK", BUFLEN);
                    sendto(fd, buf, strlen(buf), 0, (struct sockaddr *)&claddr, sizeof(claddr));

                    LOG("Execute udp command: '%s' | argument: '%s'", command->command_name, argument);
                    char command_response[BUFLEN];
                    command->callback(command_response, BUFLEN, argument_len > 0 ? argument : NULL);
                    if (strlen(command_response) > 0) {
                        LOG("udp command response: %s", command_response);
                        char result[sizeof(command_response) + 2];
                        strcpy(result, command_response);
                        sprintf(result, "\n%s", command_response);
                        sendto(fd, result, strlen(result), 0, (struct sockaddr *)&claddr, sizeof(claddr));
                    }
                } else {
                    strlcat(buf, " - NF", BUFLEN);
                    sendto(fd, buf, strlen(buf), 0, (struct sockaddr *)&claddr, sizeof(claddr));
                    LOG("upd command not found: '%s'", command_str);
                }
            }
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    // close
    ret = lwip_close(fd);
    LWIP_ASSERT("ret == 0", ret == 0);
    vTaskDelete(NULL);
}

// Public
/**
 * @brief  Add command that will be executed via udp message
 * @param  command_name: Name of the command that will be used to execute the command
 * @param  callback: A function that will be executed (param can be NULL, result can be provided but is not required)
 * @retval Returns -1 if command with given name already exists and cannot be added, 0 if succesfully added
 */
int udp_command_add(char *command_name, udp_command_callback_fn callback) {
    udp_command_t *udp_command = udp_command_find_by_name(command_name);
    if (udp_command) {
        return -1;
    }

    udp_command = malloc(sizeof(udp_command_t));
    memset(udp_command, 0, sizeof(*udp_command));

    udp_command->command_name = command_name;
    udp_command->callback = callback;
    udp_command->next = udp_commands;
    udp_commands = udp_command;
    return 0;
}

/**
 * @brief  Start udp command server
 * @note   UDP server will listen to plan text message send to this IP address 
 *         on given port and will try to execute a command if found.
 *         Valid command formats: 
 *            * 'name' (without param)
 *            * 'name:params' (with param)
 *         Example:
 *            * 'reset' or 'gpio_read:5'
 *            * echo "gpio_read:0" | nc -w 1 -u 192.168.1.2 9876 
 *         Param is sent to the command's callback function
 *         The server can respond on the command:
 *            * 'OK' - command executed
 *            * 'NF' - command not found
 *            * 'ARG' - wrong command format (missing argument)
 *         If the command will return a result, it will be send to the client.
 * @param  udp_port: Port number that the UDP server will listen to
 * @retval None
 */
void udp_command_server_task_start(int udp_port) {
    udp_command_port = udp_port;
    xTaskCreate(udp_command_server_task, "udp_command_server_task", 1024, NULL, 2, NULL);
}
/**
 * @brief  Same as udp_command_server_task_start_with_default_commands but it adds predefined commands
 * @note   Predefined commands:
 *            * restart (restart the device)
 *            * homekit_reset (reset the homekit storage and restart the device)
 *            * gpio_read (read gpio value for given gpio number ex. 'gpio_read:5')
 *            * log_enable (enable/disable LOG() from debug_helper.h)
 * @param  udp_port: Port number that the UDP server will listen to
 * @retval None
 */
void udp_command_server_task_start_with_default_commands(int udp_port) {
    udp_command_server_task_start(udp_port);
    udp_command_add("restart", udp_command_system_restart);
    udp_command_add("homekit_reset", udp_command_homekit_reset);
    udp_command_add("gpio_read", udp_command_gpio_read);
    udp_command_add("log_enable", udp_command_log_enable);
}