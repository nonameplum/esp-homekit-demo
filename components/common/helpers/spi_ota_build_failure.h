#ifndef spi_ota_build_failure_h
#define spi_ota_build_failure_h

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void init_ota_update_failure_check(char *buildInfo, uint8_t maxFailureCount, int inTimeIntervalMs);

#endif /* spi_ota_build_failure_h */