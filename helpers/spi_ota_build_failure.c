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

// Types
typedef unsigned char byte;
typedef struct {
    char buildTime[19];
    TickType_t crashTickCount;
} ota_failure_info_t;

// Definitons & Global varialbes
#ifndef SPIFLASH_OTA_INFO_BASE_ADDR
#define SPIFLASH_OTA_INFO_BASE_ADDR 0x300000
#endif

#define OTA_INFO_MAX_CRASH 10
// Treat a crash as a OTA update failure
// if the time difference between the current and previous
// crash is less than the 30 seconds
#define OTA_INFO_CRASH_MAX_INTERVAL_MS 500

char *appBuildInfo = NULL;

// Private methods
static void log_ota_info_data(ota_failure_info_t data) {
    LOG("ota info [%d] { buildTime: %s, crashTickCount: %d }", 
        sizeof(data), data.buildTime, data.crashTickCount);
}

static int find_empty_block() {
    byte data[sizeof(ota_failure_info_t)];

    for (int i=0; i<OTA_INFO_MAX_CRASH; i++) {
        spiflash_read(SPIFLASH_OTA_INFO_BASE_ADDR + sizeof(data)*i, data, sizeof(data));

        bool block_empty = true;
        for (int j=0; j<sizeof(data); j++)
            if (data[j] != 0xff) {
                block_empty = false;
                break;
            }

        if (block_empty)
            return i;
    }

    return -1;
}

static void store_ota_update_failure_build(char *buildInfo) {
    LOG("Ota update failure build, buildInfo: %s", buildInfo);

    int next_block_idx = find_empty_block();
    LOG("Found next empty block no: %d", next_block_idx);
    if (next_block_idx == -1) {
        LOG("Failed to write failure info. Max number of crashes %d", OTA_INFO_MAX_CRASH);
        LOG("Switch to previous ROM & restart the device");
        spiflash_erase_sector(SPIFLASH_OTA_INFO_BASE_ADDR);
        rboot_config conf = rboot_get_config();
        int slot = (conf.current_rom + 1) % conf.count;
        if (slot == conf.current_rom) {
            LOG("Only one OTA slot!");
        }
        rboot_set_current_rom(slot);
        sdk_system_restart();
        return;
    }

    if (next_block_idx > 0) {
        ota_failure_info_t prevInfo;
        memset(&prevInfo, 0, sizeof(prevInfo));
        if (!spiflash_read(SPIFLASH_OTA_INFO_BASE_ADDR + sizeof(prevInfo)*(next_block_idx-1), (byte *)&prevInfo, sizeof(prevInfo))) {
            LOG("Previous %d ota failure info read failed", next_block_idx-1);
            spiflash_erase_sector(SPIFLASH_OTA_INFO_BASE_ADDR);
            return;
        }
        LOG("Loaded previous ota crash info");
        log_ota_info_data(prevInfo);
        if (strncmp(prevInfo.buildTime, buildInfo, sizeof(prevInfo.buildTime)) != 0) {
            LOG("New version detected '%s'. Erase old '%s' stored crash info.", buildInfo, prevInfo.buildTime);
            spiflash_erase_sector(SPIFLASH_OTA_INFO_BASE_ADDR);
            next_block_idx = find_empty_block();
            if (next_block_idx == -1) {
                return;
            }
        }

        TickType_t prevCrashDiffMs = abs((prevInfo.crashTickCount - xTaskGetTickCount()) * 10);
        LOG("Time difference to previous crash %d > %d ms.", prevCrashDiffMs, OTA_INFO_CRASH_MAX_INTERVAL_MS);
        if (prevCrashDiffMs > OTA_INFO_CRASH_MAX_INTERVAL_MS) {
            LOG("Skip ota info crash");
            return;
        }
    }

    ota_failure_info_t info;
    memset(&info, 0, sizeof(info));
    strncpy(info.buildTime, buildInfo, sizeof(info.buildTime));
    info.crashTickCount = xTaskGetTickCount();
    LOG("Write ota info at index: %d", next_block_idx);
    log_ota_info_data(info);
    
    if (!spiflash_write(SPIFLASH_OTA_INFO_BASE_ADDR + sizeof(info)*next_block_idx, (byte *)&info, sizeof(info))) {
        LOG("Failed to write ota failure info to flash");
    } else {
        LOG("Success to write ota failure info to flash");
    }
}

static void exception_handler() {
    LOG("Exception handler");
    if (appBuildInfo == NULL) {
        return;
    }
    store_ota_update_failure_build(appBuildInfo);
}

static void log_ota_config() {
    rboot_config conf = rboot_get_config();
    LOG("\r\n\r\nOTA Basic demo.\r\nCurrently running on flash slot %d / %d.\r\n\r\n",
           conf.current_rom, conf.count);

    LOG("Image addresses in flash:\r\n");
    for(int i = 0; i <conf.count; i++) {
        LOG("%c%d: offset 0x%08x\r\n", i == conf.current_rom ? '*':' ', i, conf.roms[i]);
    }
}

// Public methods
void init_ota_update_failure_check(char *buildInfo) {
    LOG("buildInfo: %s", buildInfo);    
    set_user_exception_handler(exception_handler);
    log_ota_config();
    appBuildInfo = strdup(buildInfo);
}