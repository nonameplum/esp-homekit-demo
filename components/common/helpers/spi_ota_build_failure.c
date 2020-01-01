#include <stdio.h>
#include <string.h>
#include "espressif/esp_common.h"
#include "spiflash.h"
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>
#include "rboot-api.h"
#include "user_exception.h"
#include <debug_helper.h>

#ifndef SPIFLASH_OTA_INFO_BASE_ADDR
#define SPIFLASH_OTA_INFO_BASE_ADDR 0x300000
#endif

typedef unsigned char byte;

char *appBuildInfo = NULL;
uint8_t appMaxFailureCount = 10;
int appInTimeIntervalMs = 60 * 1000;

typedef struct {
    TickType_t firstCrashTick;
    TickType_t lastCrashTick;
    byte crashCount;
    char buildTime[20];
} ota_failure_info_t;

void reset_ota_failure_info(ota_failure_info_t *info) {
    LOG("Reset ota failure info");
    info->firstCrashTick = xTaskGetTickCount();
    info->lastCrashTick = xTaskGetTickCount();
    info->crashCount = 1;
}

void store_ota_update_failure_build(char *buildInfo) {
    LOG("Ota update failure build, buildInfo: %s", buildInfo);
    ota_failure_info_t data;

    bool read_result = spiflash_read(SPIFLASH_OTA_INFO_BASE_ADDR, (byte *)&data, sizeof(data));
    if (read_result && strlen(data.buildTime) == 19) { // update
        data.lastCrashTick = xTaskGetTickCount();
        data.crashCount += data.crashCount;
        LOG("Update ota info. LastCrashTick: %d | CrashCount: %d", data.lastCrashTick, data.crashCount);

        if (data.crashCount >= appMaxFailureCount 
            && (data.lastCrashTick - data.firstCrashTick)*10 <= appInTimeIntervalMs) 
        {
            LOG("Switch to previous ROM & restart the device");
            LOG("CrashCount: %d | In time [ms]: %d | Allowed time interval [ms]: %d", 
                data.crashCount, 
                (data.lastCrashTick - data.firstCrashTick)*10, 
                appInTimeIntervalMs);
            memset(&data, 0, sizeof(data));
            if (!spiflash_write(SPIFLASH_OTA_INFO_BASE_ADDR, (byte *)&data, sizeof(data))) {
                LOG("Failed to remove ota failure info from flash during ROM switch");
            }

            rboot_config conf = rboot_get_config();
            int slot = (conf.current_rom + 1) % conf.count;
            if (slot == conf.current_rom) {
                LOG("Only one OTA slot!");
            }
            rboot_set_current_rom(slot);
            sdk_system_restart();
            return;
        }

        if ((data.lastCrashTick - data.firstCrashTick)*10 >= appInTimeIntervalMs) {
            LOG("Reset ota failure info, because crashes [%d] occured in time interval [ms] %d, exceeded max timer interval range [ms] %d",
                data.crashCount, 
                (data.lastCrashTick - data.firstCrashTick)*10, 
                appInTimeIntervalMs);
            reset_ota_failure_info(&data);
        }
    } else { // add
        LOG("Add ota failure info");
        memset(&data, 0, sizeof(data));
        reset_ota_failure_info(&data);
        strncpy(data.buildTime, buildInfo, sizeof(data.buildTime));
    }
    if (!spiflash_write(SPIFLASH_OTA_INFO_BASE_ADDR + sizeof(data), (byte *)&data, sizeof(data))) {
        LOG("Failed to remove ota failure info from flash");
    }
}

void exception_handler() {
    if (appBuildInfo == NULL) {
        return;
    }
    store_ota_update_failure_build(appBuildInfo);
}

void log_ota_config() {
    rboot_config conf = rboot_get_config();
    LOG("\r\n\r\nOTA Basic demo.\r\nCurrently running on flash slot %d / %d.\r\n\r\n",
           conf.current_rom, conf.count);

    LOG("Image addresses in flash:\r\n");
    for(int i = 0; i <conf.count; i++) {
        LOG("%c%d: offset 0x%08x\r\n", i == conf.current_rom ? '*':' ', i, conf.roms[i]);
    }
}

void init_ota_update_failure_check(char *buildInfo, uint8_t maxFailureCount, int inTimeIntervalMs) {
    log_ota_config();
    appMaxFailureCount = maxFailureCount;
    appInTimeIntervalMs = inTimeIntervalMs;
    strcpy(appBuildInfo, buildInfo);
    set_user_exception_handler(exception_handler);
}