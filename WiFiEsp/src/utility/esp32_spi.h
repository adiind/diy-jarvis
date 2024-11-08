#ifndef __ESP32_SPI_H
#define __ESP32_SPI_H

#include <stdbool.h>
#include <stdint.h>

#include "Arduino.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef K210
#ifndef WIFI_CS
#define WIFI_CS   SPI0_CS1
#endif
#ifndef WIFI_MISO
#define WIFI_MISO SPI0_MISO
#define WIFI_SCLK SPI0_SCLK
#define WIFI_MOSI SPI0_MOSI
#endif
#ifndef WIFI_RDY
#define WIFI_RDY ORG_PIN_MAP(9)
#define WIFI_RST ORG_PIN_MAP(8)
#define WIFI_RX  ORG_PIN_MAP(7)
#define WIFI_TX  ORG_PIN_MAP(6)
#endif
#endif

/* clang-format off */
#define ESP32_SPI_DEBUG                 (0)

#define ESP32_ADC_CH_NUM                (6)
#define SPI_MAX_DMA_LEN 4000 //(4096-4)

#if 1
#define _DEBUG()
#else
#define _DEBUG()                                     \
    do                                               \
    {                                                \
        printf("%s --> %d\r\n", __func__, __LINE__); \
    } while (0)
#endif

typedef enum
{
    SET_NET_CMD                 = (0x10),
    SET_PASSPHRASE_CMD          = (0x11),
    SET_KEY_CMD                 = (0x12),
//    TEST_CMD                    = (0x13),
    SET_IP_CONFIG_CMD           = (0x14),
    SET_DNS_CONFIG_CMD          = (0x15),
    SET_HOSTNAME_CMD            = (0x16),
    SET_POWER_MODE_CMD          = (0x17),
    SET_AP_NET_CMD              = (0x18),
    SET_AP_PASSPHRASE_CMD       = (0x19),
    SET_DEBUG_CMD               = (0x1A),
    GET_CONN_STATUS_CMD         = (0x20),
    GET_IPADDR_CMD              = (0x21),
    GET_MACADDR_CMD             = (0x22),
    GET_CURR_SSID_CMD           = (0x23),
    GET_CURR_BSSID_CMD          = (0x24),
    GET_CURR_RSSI_CMD           = (0x25),
    GET_CURR_ENCT_CMD           = (0x26),
    SCAN_NETWORKS               = (0x27),
    START_SERVER_TCP_CMD        = (0x28),
    GET_SOCKET_CMD              = (0x3F),
    GET_STATE_TCP_CMD           = (0x29),
    DATA_SENT_TCP_CMD           = (0x2A),
    AVAIL_DATA_TCP_CMD          = (0x2B),
    GET_DATA_TCP_CMD            = (0x2C),
    START_CLIENT_TCP_CMD        = (0x2D),
    STOP_CLIENT_TCP_CMD         = (0x2E),
    GET_CLIENT_STATE_TCP_CMD    = (0x2F),
    DISCONNECT_CMD              = (0x30),
//    GET_IDX_SSID_CMD            = (0x31),
    GET_IDX_RSSI_CMD            = (0x32),
    GET_IDX_ENCT_CMD            = (0x33),
    REQ_HOST_BY_NAME_CMD        = (0x34),
    GET_HOST_BY_NAME_CMD        = (0x35),
    START_SCAN_NETWORKS         = (0x36),
    GET_FW_VERSION_CMD          = (0x37),
//    GET_TEST_CMD                = (0x38),
    SEND_UDP_DATA_CMD           = (0x39), // START_CLIENT_TCP_CMD set ip,port, then ADD_UDP_DATA_CMD to add data then SEND_UDP_DATA_CMD to call sendto
    GET_REMOTE_INFO_CMD         = (0x3A),
    GET_TIME                    = (0x3B),
    GET_IDX_BSSID_CMD           = (0x3C),
    GET_IDX_CHAN_CMD            = (0x3D),
    PING_CMD                    = (0x3E),
    SEND_DATA_TCP_CMD           = (0x44),
    GET_DATABUF_TCP_CMD         = (0x45),
    ADD_UDP_DATA_CMD            = (0x46), // adafruit_esp32spi: INSERT_DATABUF_TCP_CMD
    SET_ENT_IDENT_CMD           = (0x4A),
    SET_ENT_UNAME_CMD           = (0x4B),
    SET_ENT_PASSWD_CMD          = (0x4C),
    SET_ENT_ENABLE_CMD          = (0x4F),
    SET_CLI_CERT                = (0x40),
    SET_PK                      = (0x41),
    SET_PIN_MODE_CMD            = (0x50),
    SET_DIGITAL_WRITE_CMD       = (0x51),
    SET_ANALOG_WRITE_CMD        = (0x52),
    GET_ADC_VAL_CMD             = (0x53), // adafruit_esp32spi: SET_DIGITAL_READ_CMD
    SOFT_RESET_CMD              = (0x54), // adafruit_esp32spi: SET_ANALOG_READ_CMD
    START_CMD                   = (0xE0),
    END_CMD                     = (0xEE),
    ERR_CMD                     = (0xEF)
}esp32_cmd_enum_t;

typedef enum {
    CMD_FLAG                    = (0),
    REPLY_FLAG                  = (1<<7)
}esp32_flag_t;

typedef enum{
    SOCKET_CLOSED               = (0),
    SOCKET_LISTEN               = (1),
    SOCKET_SYN_SENT             = (2),
    SOCKET_SYN_RCVD             = (3),
    SOCKET_ESTABLISHED          = (4),
    SOCKET_FIN_WAIT_1           = (5),
    SOCKET_FIN_WAIT_2           = (6),
    SOCKET_CLOSE_WAIT           = (7),
    SOCKET_CLOSING              = (8),
    SOCKET_LAST_ACK             = (9),
    SOCKET_TIME_WAIT            = (10),
    //
    SOCKET_STATUS_ERROR         = (0xFF)
}esp32_socket_enum_t;

typedef enum
{
    WL_IDLE_STATUS              = (0),
    WL_NO_SSID_AVAIL            = (1),
    WL_SCAN_COMPLETED           = (2),
    WL_CONNECTED                = (3),
    WL_CONNECT_FAILED           = (4),
    WL_CONNECTION_LOST          = (5),
    WL_DISCONNECTED             = (6),
    WL_AP_LISTENING             = (7),
    WL_AP_CONNECTED             = (8),
    WL_AP_FAILED                = (9),
    //
    WL_NO_MODULE                = (0xFF)
}esp32_wlan_enum_t;

typedef enum
{
    TCP_MODE                    = (0),
    UDP_MODE                    = (1),
    TLS_MODE                    = (2)
}esp32_socket_mode_enum_t;

/* clang-format on */

typedef void (*esp32_spi_params_del)(void *arg);

typedef struct
{
    uint32_t param_len;
    uint8_t *param;
} esp32_spi_param_t;

typedef struct
{
    uint32_t params_num;
    esp32_spi_param_t **params;
    esp32_spi_params_del del;
} esp32_spi_params_t;

///////////////////////////////////////////////////////////////////////////////
typedef void (*esp32_spi_aps_list_del)(void *arg);

typedef struct
{
    int8_t rssi;
    uint8_t encr;
    uint8_t ssid[33];
} esp32_spi_ap_t;

typedef struct
{
    uint32_t aps_num;
    esp32_spi_ap_t **aps;
    esp32_spi_aps_list_del del;
} esp32_spi_aps_list_t;

typedef struct
{
    uint8_t localIp[32];
    uint8_t subnetMask[32];
    uint8_t gatewayIp[32];
} esp32_spi_net_t;

void esp32_spi_init(uint8_t cs_num, uint8_t rst_num, uint8_t rdy_num, uint8_t is_hard_spi);
bool esp32_spi_begin(int cs, int rst, int rdy, int mosi, int miso, int sclk, int spi);
bool esp32_spi_init_soft(int cs, int rst, int rdy, int mosi, int miso, int sclk);
bool esp32_spi_init_hard(int cs, int rst, int rdy, int spi);
int8_t esp32_spi_status(void);
char *esp32_spi_firmware_version(char* fw_version);
uint8_t *esp32_spi_MAC_address(void);

int8_t esp32_spi_start_scan_networks(void);
esp32_spi_aps_list_t *esp32_spi_get_scan_networks(void);
esp32_spi_aps_list_t *esp32_spi_scan_networks(void);
int8_t esp32_spi_wifi_set_network(uint8_t *ssid);
int8_t esp32_spi_wifi_wifi_set_passphrase(uint8_t *ssid, uint8_t *passphrase);
int8_t esp32_spi_wifi_set_ip_config(uint8_t validParams, uint8_t *local_ip, uint8_t *gateway, uint8_t *subnet);
char *esp32_spi_get_ssid(void);
char *esp32_spi_get_bssid(void);
int8_t esp32_spi_get_rssi(void);
esp32_spi_net_t *esp32_spi_get_network_data(void);
int8_t esp32_spi_ip_address(uint8_t *net_data);
uint8_t esp32_spi_is_connected(void);
void esp32_spi_connect(uint8_t *secrets);
int8_t esp32_spi_connect_AP(uint8_t *ssid, uint8_t *password, uint8_t retry_times);
int8_t esp32_spi_disconnect_from_AP(void);
int8_t esp32_spi_wifi_set_ap_network(uint8_t *ssid, uint8_t channel);
int8_t esp32_spi_wifi_set_ap_passphrase(uint8_t *ssid, uint8_t *passphrase, uint8_t channel);
void esp32_spi_pretty_ip(uint8_t *ip, uint8_t *str_ip);
int esp32_spi_get_host_by_name(uint8_t *hostname, uint8_t *ip);
int32_t esp32_spi_ping(uint8_t *dest, uint8_t dest_type, uint8_t ttl);

uint8_t esp32_spi_get_socket(void);
int8_t esp32_spi_socket_open(uint8_t sock_num, uint8_t *dest, uint8_t dest_type, uint16_t port, esp32_socket_mode_enum_t conn_mode);
int8_t esp32_spi_get_remote_data(uint8_t sock, uint8_t *ip, uint8_t *port);
esp32_socket_enum_t esp32_spi_socket_status(uint8_t socket_num);
uint8_t esp32_spi_socket_connected(uint8_t socket_num);
uint32_t esp32_spi_socket_write(uint8_t socket_num, uint8_t *buffer, uint16_t len);
int8_t esp32_spi_check_data_sent(uint8_t socket_num);
int esp32_spi_socket_available(uint8_t socket_num);
int esp32_spi_socket_read(uint8_t socket_num, uint8_t *buff, uint16_t size);
int8_t esp32_spi_socket_connect(uint8_t socket_num, uint8_t *dest, uint8_t dest_type, uint16_t port, esp32_socket_mode_enum_t conn_mod);
int8_t esp32_spi_socket_close(uint8_t socket_num);
esp32_socket_enum_t esp32_spi_server_socket_status(uint8_t socket_num);
int8_t esp32_spi_start_server(uint16_t port, uint8_t sock, uint8_t protMode);

int8_t esp32_spi_get_adc_val(uint8_t* channels, uint8_t len, uint16_t *val);

const char *socket_enum_to_str(esp32_socket_enum_t x);
const char *wlan_enum_to_str(esp32_wlan_enum_t x);

int8_t esp32_spi_add_udp_data(uint8_t sock_num, uint8_t* data, uint16_t data_len);
int8_t esp32_spi_send_udp_data(uint8_t sock_num);
int8_t esp32_spi_get_remote_info(uint8_t socket_num, uint8_t* ip, uint16_t* port);

uint8_t connect_server_port(char *host, uint16_t port);

#ifdef __cplusplus
}
#endif

#endif
