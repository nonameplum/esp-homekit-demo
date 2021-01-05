#ifndef wifi_setup_h
#define wifi_setup_h

#include <stdbool.h>

typedef void (*wifi_connected_callback)(void);

void wifi_init(const char* ssid, const char* password, const char* hostName, wifi_connected_callback callback);

#endif /* wifi_setup_h */