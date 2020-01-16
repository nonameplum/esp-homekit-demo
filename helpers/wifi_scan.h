//
//  wifi_scan.h
//  xcode
//
//  Created by Łukasz Śliwiński on 28/11/2019.
//  Copyright © 2019 plum. All rights reserved.
//

#ifndef wifi_scan_h
#define wifi_scan_h

#include <stdio.h>
#include <stdbool.h>

typedef void (*wifi_scan_callback_fn)(bool wifi_found, bool socked_connected);

void start_wifi_scan(char *ssid, wifi_scan_callback_fn callback);
void stop_wifi_scan();

#endif /* wifi_scan_h */
