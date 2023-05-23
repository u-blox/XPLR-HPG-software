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

#if (XPLR_CFG_ENABLE_WEBSERVERDNS == 1)

#include "xplr_wifi_dns.h"
#include <sys/param.h>
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "mdns.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#define DNS_PORT        (53)
#define DNS_MAX_LEN     (256)

#define OPCODE_MASK     (0x7800)
#define QR_FLAG         (1 << 7)
#define QD_TYPE_A       (0x0001)
#define ANS_TTL_SEC     (300)

#if (1 == XPLRWIFIDNS_DEBUG_ACTIVE) && (1 == XPLR_HPGLIB_SERIAL_DEBUG_ENABLED)
#define XPLRWIFIDNS_CONSOLE(tag, message, ...)   esp_rom_printf(XPLR_HPGLIB_LOG_FORMAT(tag, message), esp_log_timestamp(), "xplrWifiDns", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define XPLRWIFIDNS_CONSOLE(message, ...) do{} while(0)
#endif

/* ----------------------------------------------------------------
 * STATIC TYPES
 * -------------------------------------------------------------- */

// DNS Header Packet
typedef struct __attribute__((__packed__))
{
    uint16_t id;
    uint16_t flags;
    uint16_t qd_count;
    uint16_t an_count;
    uint16_t ns_count;
    uint16_t ar_count;
}
dns_header_t;

// DNS Question Packet
typedef struct {
    uint16_t type;
    uint16_t class;
} dns_question_t;

// DNS Answer Packet
typedef struct __attribute__((__packed__))
{
    uint16_t ptr_offset;
    uint16_t type;
    uint16_t class;
    uint32_t ttl;
    uint16_t addr_len;
    uint32_t ip_addr;
}
dns_answer_t;

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

TaskHandle_t dnsTask;
int sock;
char hostnameConfigured[32 + 1];

/* ----------------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 * -------------------------------------------------------------- */

/* Used when in AP mode */

static char *parse_dns_name(char *raw_name, char *parsed_name, size_t parsed_name_max_len);
static int parse_dns_request(char *req, size_t req_len, char *dns_reply, size_t dns_reply_max_len);
void xDns_server(void *pvParameters);

/* Used when in STA mode */

static void mDnsInit(void);
static char *mDnsGenerateHostname(void);

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS DESCRIPTORS
 * -------------------------------------------------------------- */
void xplrWifiDnsStart(void)
{
    xTaskCreate(xDns_server, "xplrDnsServer", 4096, NULL, 5, dnsTask);
}

void xplrWifiDnsStop(void)
{
    XPLRWIFIDNS_CONSOLE(E, "Shutting down socket");
    shutdown(sock, 0);
    close(sock);
    vTaskDelete(dnsTask);
}

char *xplrWifiStaDnsStart(void)
{
    char *ret;
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());

    mDnsInit();

    if (strlen(hostnameConfigured) > 0) {
        ret = hostnameConfigured;
    } else {
        ret = NULL;
    }

    return ret;
}

void xplrWifiStaDnsStop(void)
{
    mdns_free();
}

/* ----------------------------------------------------------------
 * STATIC FUNCTION DESCRIPTORS
 * -------------------------------------------------------------- */

static char *parse_dns_name(char *raw_name, char *parsed_name, size_t parsed_name_max_len)
{
    char *label = raw_name;
    char *name_itr = parsed_name;
    int name_len = 0;

    do {
        int sub_name_len = *label;
        // (len + 1) since we are adding  a '.'
        name_len += (sub_name_len + 1);
        if (name_len > parsed_name_max_len) {
            return NULL;
        }

        // Copy the sub name that follows the the label
        memcpy(name_itr, label + 1, sub_name_len);
        name_itr[sub_name_len] = '.';
        name_itr += (sub_name_len + 1);
        label += sub_name_len + 1;
    } while (*label != 0);

    // Terminate the final string, replacing the last '.'
    parsed_name[name_len - 1] = '\0';
    // Return pointer to first char after the name
    return label + 1;
}

static int parse_dns_request(char *req, size_t req_len, char *dns_reply, size_t dns_reply_max_len)
{
    if (req_len > dns_reply_max_len) {
        return -1;
    }

    // Prepare the reply
    memset(dns_reply, 0, dns_reply_max_len);
    memcpy(dns_reply, req, req_len);

    // Endianess of NW packet different from chip
    dns_header_t *header = (dns_header_t *)dns_reply;
    XPLRWIFIDNS_CONSOLE(D, "DNS query with header id: 0x%X, flags: 0x%X, qd_count: %d",
                        ntohs(header->id), ntohs(header->flags), ntohs(header->qd_count));

    // Not a standard query
    if ((header->flags & OPCODE_MASK) != 0) {
        return 0;
    }

    // Set question response flag
    header->flags |= QR_FLAG;

    uint16_t qd_count = ntohs(header->qd_count);
    header->an_count = htons(qd_count);

    int reply_len = qd_count * sizeof(dns_answer_t) + req_len;
    if (reply_len > dns_reply_max_len) {
        return -1;
    }

    // Pointer to current answer and question
    char *cur_ans_ptr = dns_reply + req_len;
    char *cur_qd_ptr = dns_reply + sizeof(dns_header_t);
    char name[128];

    // Respond to all questions with the ESP32's IP address
    for (int i = 0; i < qd_count; i++) {
        char *name_end_ptr = parse_dns_name(cur_qd_ptr, name, sizeof(name));
        if (name_end_ptr == NULL) {
            XPLRWIFIDNS_CONSOLE(E, "Failed to parse DNS question: %s", cur_qd_ptr);
            return -1;
        }

        dns_question_t *question = (dns_question_t *)(name_end_ptr);
        uint16_t qd_type = ntohs(question->type);
        uint16_t qd_class = ntohs(question->class);

        XPLRWIFIDNS_CONSOLE(D, "Received type: %d | Class: %d | Question for: %s", qd_type, qd_class, name);

        if (qd_type == QD_TYPE_A) {
            dns_answer_t *answer = (dns_answer_t *)cur_ans_ptr;

            answer->ptr_offset = htons(0xC000 | (cur_qd_ptr - dns_reply));
            answer->type = htons(qd_type);
            answer->class = htons(qd_class);
            answer->ttl = htonl(ANS_TTL_SEC);

            esp_netif_ip_info_t ip_info;
            esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &ip_info);
            XPLRWIFIDNS_CONSOLE(D, "Answer with PTR offset: 0x%X and IP 0x%X", ntohs(answer->ptr_offset),
                                ip_info.ip.addr);

            answer->addr_len = htons(sizeof(ip_info.ip.addr));
            answer->ip_addr = ip_info.ip.addr;
        }
    }
    return reply_len;
}

void xDns_server(void *pvParameters)
{
    char rx_buffer[128];
    char addr_str[128];
    int addr_family;
    int ip_protocol;

    while (1) {

        struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(DNS_PORT);
        addr_family = AF_INET;
        ip_protocol = IPPROTO_IP;
        inet_ntoa_r(dest_addr.sin_addr, addr_str, sizeof(addr_str) - 1);

        sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
        if (sock < 0) {
            XPLRWIFIDNS_CONSOLE(E, "Unable to create socket: errno %d", errno);
            break;
        }
        XPLRWIFIDNS_CONSOLE(D, "Socket created");

        int err = bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err < 0) {
            XPLRWIFIDNS_CONSOLE(E, "Socket unable to bind: errno %d", errno);
        }
        XPLRWIFIDNS_CONSOLE(D, "Socket bound, port %d", DNS_PORT);

        while (1) {
            XPLRWIFIDNS_CONSOLE(D, "Waiting for data");
            struct sockaddr_in source_addr;
            socklen_t socklen = sizeof(source_addr);
            int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr,
                               &socklen);

            // Error occurred during receiving
            if (len < 0) {
                XPLRWIFIDNS_CONSOLE(E, "recvfrom failed: errno %d", errno);
                close(sock);
                break;
            }
            // Data received
            else {
                // Get the sender's ip address as string
                if (source_addr.sin_family == AF_INET) {
                    inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr.s_addr, addr_str, sizeof(addr_str) - 1);
                }

                // Null-terminate whatever we received and treat like a string...
                rx_buffer[len] = 0;

                char reply[DNS_MAX_LEN];
                int reply_len = parse_dns_request(rx_buffer, len, reply, DNS_MAX_LEN);

                XPLRWIFIDNS_CONSOLE(D, "Received %d bytes from %s | DNS reply with len: %d", len, addr_str,
                                    reply_len);
                if (reply_len <= 0) {
                    XPLRWIFIDNS_CONSOLE(E, "Failed to prepare a DNS reply");
                } else {
                    int err = sendto(sock, reply, reply_len, 0, (struct sockaddr *)&source_addr, sizeof(source_addr));
                    if (err < 0) {
                        XPLRWIFIDNS_CONSOLE(E, "Error occurred during sending: errno %d", errno);
                        break;
                    }
                }
            }
        }

        if (sock != -1) {
            XPLRWIFIDNS_CONSOLE(E, "Shutting down socket");
            shutdown(sock, 0);
            close(sock);
        }
    }
    vTaskDelete(dnsTask);
}

static void mDnsInit(void)
{
    char *hostname = mDnsGenerateHostname();
    if (strlen(hostname) < 32) {
        memcpy(hostnameConfigured, hostname, strlen(hostname));
    }
    mdns_txt_item_t serviceTxtData[3] = {
        {"product", "u-blox_xplr-hpg"},
        {"interface", "wifi"},
        {"service", "point-perfect"}
    };

    //initialize mDNS
    ESP_ERROR_CHECK(mdns_init());
    //set mDNS hostname (required if you want to advertise services)
    ESP_ERROR_CHECK(mdns_hostname_set(hostname));
    XPLRWIFIDNS_CONSOLE(I, "mdns hostname set to: [%s]", hostname);
    //set default mDNS instance name
    ESP_ERROR_CHECK(mdns_instance_name_set(CONFIG_XPLR_MDNS_INSTANCE));

    //initialize service
    ESP_ERROR_CHECK(mdns_service_add("XPLR-HPG-WebServer", "_http", "_tcp", 80, serviceTxtData, 3));

    free(hostname);
}

static char *mDnsGenerateHostname(void)
{
#ifndef CONFIG_XPLR_MDNS_ADD_MAC_TO_HOSTNAME
    return strdup(CONFIG_XPLR_MDNS_HOSTNAME);
#else
    uint8_t mac[6];
    char   *hostname;
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (-1 == asprintf(&hostname, "%s-%02X%02X%02X", CONFIG_XPLR_MDNS_HOSTNAME, mac[3], mac[4],
                       mac[5])) {
        abort();
    }
    return hostname;
#endif
}

#endif