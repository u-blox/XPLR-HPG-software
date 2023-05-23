/*
 * Copyright 2022 u-blox Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _XPLR_WIFI_DNS_H_
#define _XPLR_WIFI_DNS_H_

#include "./../../hpglib/xplr_hpglib_cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

/**
 * @brief Set ups and starts a simple DNS server that will respond to all queries
 * with the soft AP's IP address
 *
 */
void xplrWifiDnsStart(void);

/**
 * @brief Stops DNS server
 *
 */
void xplrWifiDnsStop(void);

/**
 * @brief Set ups and starts a simple DNS server that will register a hostname.
 * Normally to be used when in STA mode.
 *
 */
char *xplrWifiStaDnsStart(void);

/**
 * @brief Stops DNS server.
 * Wi-Fi events are removed, use with caution!
 *
 */
void xplrWifiStaDnsStop(void);

#ifdef __cplusplus
}
#endif

#endif /* _XPLR_WIFI_DNS_H_ */