#ifndef debug_helper_h
#define debug_helper_h

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <udplogger.h>

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

char* getFormattedTime(void);
char* boolToString(bool value);
void silent_unused_args(const char *__restrict, ...);

extern bool debug_helper_log_enabled;

#ifdef DEBUG_HELPER

    #ifdef DEBUG_HELPER_UDP

        #define _LOG_(format, ...) \
        do { \
            UDPLSO(format, ##__VA_ARGS__); \
            UDPLUO(format, ##__VA_ARGS__); \
        } while(0)

    #else

        #define _LOG_(format, ...) \
        do { \
            printf(format, ##__VA_ARGS__); \
        } while(0)

    #endif

    #define LOG(format, ...) \
    do { \
        if (debug_helper_log_enabled) { \
        char *time = getFormattedTime(); \
        _LOG_("+++ %s [%s] [%s:%d] " format "\n", time, __func__, __FILENAME__, __LINE__, ##__VA_ARGS__); \
        free(time); \
        } \
    } while(0)

#else

    #define LOG(message, ...) silent_unused_args(message, ##__VA_ARGS__)

#endif /* DEBUG_HELPER */

#endif /* debug_helper_h */