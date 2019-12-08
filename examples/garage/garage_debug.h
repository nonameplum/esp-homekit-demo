//
//  garage_debug.h
//  garage
//
//  Created by Łukasz Śliwiński on 26/11/2019.
//  Copyright © 2019 plum. All rights reserved.
//

#ifndef garage_debug_h
#define garage_debug_h

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

char* getFormattedTime(void);
char* boolToString(bool value);
void silent_unused_args(const char *__restrict, ...);

#ifdef GARAGE_DEBUG

    #ifdef GARAGE_DEBUG_UDP

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

    #endif /* GARAGE_DEBUG_UDP */

    #define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

    #define LOG(format, ...) \
    do { \
        char *time = getFormattedTime(); \
        _LOG_("+++ %s [%s] [%s:%d] " format "\n", time, __func__, __FILENAME__, __LINE__, ##__VA_ARGS__); \
        free(time); \
    } while(0)

#else

    #define LOG(message, ...) silent_unused_args(message, ##__VA_ARGS__)

#endif /* GARAGE_DEBUG */

#endif /* garage_debug_h */
