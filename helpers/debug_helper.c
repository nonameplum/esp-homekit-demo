//
//  garage_debug.c
//  xcode
//
//  Created by Łukasz Śliwiński on 26/11/2019.
//  Copyright © 2019 plum. All rights reserved.
//

#include "debug_helper.h"
#include <time.h>

bool debug_helper_log_enabled = true;

char* getFormattedTime(void) {
    time_t ts = time(NULL);
    struct tm tm = *localtime(&ts);
    char *str = (char*)malloc(9 * sizeof(char));
    snprintf(str, 9, "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
    return str;
}

char* boolToString(bool value) {
    return value ? "true" : "false";
}

void silent_unused_args(const char *__restrict message, ...) {
}
