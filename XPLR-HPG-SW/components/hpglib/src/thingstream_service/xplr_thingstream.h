/*
 * Copyright 2023 u-blox
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

#ifndef XPLR_THINGSTREAM_H_
#define XPLR_THINGSTREAM_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "./../../xplr_hpglib_cfg.h"
#include "xplr_thingstream_types.h"

/** @file
 * @brief This header file defines the thingstream service API,
 * including server and services configuration settings, message encoding and decoding,
 * helper functions for communicating with the thingstream platform etc.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

/**
 * @brief Initialize thingstream instance.
 *        Sets server url, server token, service paths etc.
 *
 * @param  ppToken      thingstream location device profile id.
 * @param  thingstream  thingstream instance to initialize.
 * @return XPLR_THINGSTREAM_OK on success, XPLR_THINGSTREAM_ERROR otherwise.
 */
xplr_thingstream_error_t xplrThingstreamInit(const char *ppToken,
                                             xplr_thingstream_t *thingstream);

/**
 * @brief Create a message according to thingstream API.
 *        Provided buffer should be big enough to fit message payload.
 *
 * @param  cmd          thingstream API command to create msg for.
 * @param  msg          thingstream location device profile id.
 * @param  size[in/out] On input defines provided buffer size.
 *                      On output denotes number of bytes written to buffer.
 * @return XPLR_THINGSTREAM_OK on success, XPLR_THINGSTREAM_ERROR otherwise.
 */
xplr_thingstream_error_t xplrThingstreamApiMsgCreate(xplr_thingstream_api_t cmd,
                                                     char *msg,
                                                     size_t *size,
                                                     xplr_thingstream_t *instance);

/**
 * @brief Configure thingstream point perfect settings.
 *        Data provided must include the ztp response payload.
 *
 * @param  data         configuration payload from thingstream.
 * @param  settings     thingstream location settings to configure.
 * @return XPLR_THINGSTREAM_OK on success, XPLR_THINGSTREAM_ERROR otherwise.
 */
xplr_thingstream_error_t xplrThingstreamPpConfig(const char *data,
                                                 xplr_thingstream_pp_region_t region,
                                                 xplr_thingstream_pp_settings_t *settings);

/**
 * @brief Returns point perfect server info based on given argument
 *
 * @param data          thingstream data payload.
 * @param value         char array to store required info.
 * @param size[in/out]  On entrance denotes max size of value buffer.
 *                      On output returns the actual size of the value.
 * @param info          required info to retrieve.
 * @return XPLR_THINGSTREAM_OK on success, XPLR_THINGSTREAM_ERROR otherwise.
 */
xplr_thingstream_error_t xplrThingstreamPpParseServerInfo(const char *data,
                                                          char *value,
                                                          size_t size,
                                                          xplr_thingstream_pp_serverInfo_type_t info);

/**
 * @brief Checks if LBAND is supported.
 *
 * @param data  thingstream data payload.
 * @param lband bool to store lband support value.
 * @return XPLR_THINGSTREAM_OK on success, XPLR_THINGSTREAM_ERROR otherwise.
 */
xplr_thingstream_error_t xplrThingstreamPpParseLbandSupport(const char *data,
                                                            bool *lband);

/**
 * @brief Checks if MQTT is supported.
 *
 * @param data  thingstream data payload.
 * @param mqtt  bool to store mqtt support value.
 * @return XPLR_THINGSTREAM_OK on success, XPLR_THINGSTREAM_ERROR otherwise.
 */
xplr_thingstream_error_t xplrThingstreamPpParseMqttSupport(const char *data,
                                                           bool *mqtt);

/**
 * @brief Returns point perfect related topic info for given type
 *
 * @param data      thingstream data payload.
 * @param type      topic type to parse info from.
 * @param topic     instance to store the key distribution topic info.
 * @return XPLR_THINGSTREAM_OK on success, XPLR_THINGSTREAM_ERROR otherwise.
 */
xplr_thingstream_error_t xplrThingstreamPpParseTopicInfo(const char *data,
                                                         xplr_thingstream_pp_region_t region,
                                                         xplr_thingstream_pp_topic_type_t type,
                                                         xplr_thingstream_pp_topic_t *topic);

/**
 * @brief Returns minimum required point perfect topics info by region
 *
 * @param data      thingstream data payload.
 * @param type      topic type to parse info from.
 * @param topics    instance list to store region related topics.
 * @return XPLR_THINGSTREAM_OK on success, XPLR_THINGSTREAM_ERROR otherwise.
 */
xplr_thingstream_error_t xplrThingstreamPpParseTopicsInfoByRegion(const char *data,
                                                                  xplr_thingstream_pp_region_t region,
                                                                  xplr_thingstream_pp_topic_t *topics);

/**
 * @brief Returns all region related point perfect topics info
 *
 * @param data      thingstream data payload.
 * @param type      topic type to parse info from.
 * @param topics    instance list to store region related topics.
 * @return XPLR_THINGSTREAM_OK on success, XPLR_THINGSTREAM_ERROR otherwise.
 */
xplr_thingstream_error_t xplrThingstreamPpParseTopicsInfoByRegionAll(const char *data,
                                                                     xplr_thingstream_pp_region_t region,
                                                                     xplr_thingstream_pp_topic_t *topics);

/**
 * @brief Returns all point perfect topics info
 *
 * @param data      thingstream data payload.
 * @param type      topic type to parse info from.
 * @param topics    instance list to store region related topics.
 * @return XPLR_THINGSTREAM_OK on success, XPLR_THINGSTREAM_ERROR otherwise.
 */
xplr_thingstream_error_t xplrThingstreamPpParseTopicsInfoAll(const char *data,
                                                             xplr_thingstream_pp_topic_t *topics);

/**
 * @brief Returns dynamic keys values, current and next.
 *
 * @param data      thingstream data payload.
 * @param keys      A struct containing the keys.
 * @return XPLR_THINGSTREAM_OK on success, XPLR_THINGSTREAM_ERROR otherwise.
 */
xplr_thingstream_error_t xplrThingstreamPpParseDynamicKeys(const char *data,
                                                           xplr_thingstream_pp_dKeys_t *keys);

/**
 * @brief Check if a given name is of type key distribution topic.
 *
 * @param name      topic name to check.
 * @param instance  thingstream instance.
 * @return true on success, false otherwise.
 */
bool xplrThingstreamPpMsgIsKeyDist(const char *name, const xplr_thingstream_t *instance);

/**
 * @brief Check if a given name is of type AssistNow topic.
 *
 * @param name      topic name to check.
 * @param instance  thingstream instance.
 * @return true on success, false otherwise.
 */
bool xplrThingstreamPpMsgIsAssistNow(const char *name, const xplr_thingstream_t *instance);

/**
 * @brief Check if a given name is of type correction data EU topic.
 *
 * @param name      topic name to check.
 * @param instance  thingstream instance.
 * @return true on success, false otherwise.
 */
bool xplrThingstreamPpMsgIsCorrectionData(const char *name, const xplr_thingstream_t *instance);

/**
 * @brief Check if a given name is of type GAD topic.
 *
 * @param name      topic name to check.
 * @param instance  thingstream instance.
 * @return true on success, false otherwise.
 */
bool xplrThingstreamPpMsgIsGAD(const char *name, const xplr_thingstream_t *instance);

/**
 * @brief Check if a given name is of type correction HPAC topic.
 *
 * @param name      topic name to check.
 * @param instance  thingstream instance.
 * @return true on success, false otherwise.
 */
bool xplrThingstreamPpMsgIsHPAC(const char *name, const xplr_thingstream_t *instance);

/**
 * @brief Check if a given name is of type OCB topic.
 *
 * @param name      topic name to check.
 * @param instance  thingstream instance.
 * @return true on success, false otherwise.
 */
bool xplrThingstreamPpMsgIsOCB(const char *name, const xplr_thingstream_t *instance);

/**
 * @brief Check if a given name is of type clock topic.
 *
 * @param name      topic name to check.
 * @param instance  thingstream instance.
 * @return true on success, false otherwise.
 */
bool xplrThingstreamPpMsgIsClock(const char *name, const xplr_thingstream_t *instance);

/**
 * @brief Check if a given name is of type frequencies topic.
 *
 * @param name      topic name to check.
 * @param instance  thingstream instance.
 * @return true on success, false otherwise.
 */
bool xplrThingstreamPpMsgIsFrequency(const char *name, const xplr_thingstream_t *instance);

#ifdef __cplusplus
}
#endif
#endif // XPLR_THINGSTREAM_H_

// End of file