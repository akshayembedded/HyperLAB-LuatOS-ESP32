#include "luat_base.h"
#include "luat_msgbus.h"
#include "luat_wlan.h"
#include "luat_timer.h"


#include "esp_attr.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_smartconfig.h"
#include "esp_mac.h"
#include "esp_netif_net_stack.h"
#include "nvs.h"

#define LUAT_LOG_TAG "wlan"
#include "luat_log.h"

#include "lwip/netif.h"
#include "luat_network_adapter.h"
#include "luat_timer.h"
#include "net_lwip2.h"
#include "lwip/tcp.h"

void net_lwip2_set_link_state(uint8_t adapter_index, uint8_t updown);

static uint8_t wlan_inited = 0;
static uint8_t wlan_is_ready = 0;

static smartconfig_event_got_ssid_pswd_t *sc_evt;

static char sta_connected_bssid[6];

char luat_sta_hostname[32];

static uint8_t smartconfig_state = 0; // 0 - idle, 1 - running
static uint8_t auto_reconnection = 0;
static uint8_t dhcp_enable = 1;

// static uint8_t ap_stack_inited = 0;
static esp_netif_t* wifiAP;
static esp_netif_t* wifiSTA;

// 从nvs恢复mac地址
#if 1
static void macaddr_restore(int id) {
    nvs_handle_t handle;
    uint8_t mac[16] = {0};
    uint8_t mac2[16] = {0};
    uint8_t empty[] = {0, 0, 0, 0, 0, 0};
    int ret = 0;
    size_t len = 6;
    // LLOGW("尝试恢复mac %d", id);
    ret = nvs_open("macaddr", NVS_READONLY, &handle);
    if (ret) {
        // LLOGD("未自定义mac地址");
        return;
    }
    if (id == 0) {
        ret = nvs_get_blob(handle, "sta", mac, &len);
        if (ESP_OK == ret && wifiSTA && 6 == len && memcmp(empty, mac, 6)) {
            esp_netif_get_mac(wifiSTA, (uint8_t*)mac2);
            LLOGD("恢复sta的自定义mac地址 %02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            ret = esp_netif_set_mac(wifiSTA, (uint8_t*)mac);
            if (ret) {
                LLOGE("恢复sta的自定义mac地址失败");
            }
        }
        else {
            // LLOGD("没有sta的自定义mac地址");
        }
    }
    else if (id == 1) {
        ret = nvs_get_blob(handle, "ap", mac, &len);
        if (ESP_OK == ret && wifiAP && 6 == len && memcmp(empty, mac, 6)) {
            esp_netif_get_mac(wifiAP, (uint8_t*)mac2);
            LLOGD("恢复ap的自定义mac地址 %02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            ret = esp_netif_set_mac(wifiAP, (uint8_t*)mac);
            if (ret) {
                LLOGE("恢复ap的自定义mac地址失败");
            }
        }
        else {
            // LLOGD("没有ap的自定义mac地址");
        }
    }
    
    // LLOGD("恢复mac结束 %d", id);
    nvs_close(handle);
}

static void macaddr_set(int id, uint8_t* mac) {
    nvs_handle_t handle;
    int ret = 0;
    // LLOGD("保存mac地址到nvs %d", id);
    ret = nvs_open("macaddr", NVS_READWRITE, &handle);
    if (ret) {
        // LLOGD("未自定义mac地址");
        return;
    }
    if (id == 0) {
        if (mac == NULL) {
            nvs_erase_key(handle, "sta");
            nvs_commit(handle);
            nvs_close(handle);
            return;
        }
        // LLOGD("存储sta的mac地址为 %02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        ret = nvs_set_blob(handle, "sta", mac, 6);
        if (ret) {
            LLOGE("存储sta的自定义mac地址失败");
        }
        else {
            nvs_commit(handle);
        }
    }
    else if (id == 1) {
        if (mac == NULL) {
            nvs_erase_key(handle, "ap");
            nvs_commit(handle);
            nvs_close(handle);
            return;
        }
        // LLOGD("存储ap的mac地址为 %02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        ret = nvs_set_blob(handle, "ap", mac, 6);
        if (ret) {
            LLOGE("存储ap的自定义mac地址失败");
        }
        else {
            nvs_commit(handle);
        }
    }
    // LLOGD("保存结果 %d", ret);
    nvs_close(handle);
}
#else
static void macaddr_restore(int id) {}
static void macaddr_set(int id, uint8_t* mac) {}
#endif

static int l_wlan_handler(lua_State *L, void* ptr) {
    rtos_msg_t* msg = (rtos_msg_t*)lua_topointer(L, -1);
    int32_t event_id = msg->arg1;
    int32_t event_tp = msg->arg2;
    char buff[64] = {0};
    lua_getglobal(L, "sys_pub");
    if (event_tp == 0) {
        switch (event_id)
        {
        case WIFI_EVENT_WIFI_READY:
            LLOGD("wifi protocol is ready");
            lua_pushstring(L, "WLAN_READY");
            lua_pushinteger(L, 1);
            //esp_netif_get_ip_info(ESP_IF_WIFI_STA,&ip_info);
            lua_call(L, 2, 0);
            break;
        case WIFI_EVENT_STA_CONNECTED:
            LLOGD("wifi connected!!!");
            lua_pushstring(L, "WLAN_STA_CONNECTED");
            lua_call(L, 1, 0);
            lua_getglobal(L, "sys_pub");
            lua_pushstring(L, "WLAN_STATUS");
            lua_pushinteger(L, 1);
            lua_call(L, 2, 0);
            if (!dhcp_enable) {
                LLOGD("dhcp is disabled, static ip, send IP_READY");
                lua_getglobal(L, "sys_pub");
                lua_pushstring(L, "IP_READY");
                luat_wlan_get_ip(0, buff);
                lua_pushstring(L, buff);
                lua_pushinteger(L, NW_ADAPTER_INDEX_LWIP_WIFI_STA);
                lua_call(L, 3, 0);
            }
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            LLOGD("wifi disconnected!!!");
            lua_pushstring(L, "WLAN_STA_DISCONNECTED");
            lua_call(L, 1, 0);
            lua_getglobal(L, "sys_pub");
            lua_pushstring(L, "WLAN_STATUS");
            lua_pushinteger(L, 0);
            lua_call(L, 2, 0);
            lua_getglobal(L, "sys_pub");
            lua_pushstring(L, "WLAN_READY");
            lua_pushinteger(L, 0);
            lua_call(L, 2, 0);
            if (auto_reconnection && smartconfig_state == 0) {
                LLOGD("auto reconnecting ...");
                esp_wifi_connect();
            }
            break;
        case WIFI_EVENT_STA_START:
            LLOGD("wifi station start");
            macaddr_restore(0);
            break;
        case WIFI_EVENT_STA_STOP:
            LLOGD("wifi station stop");
            break;
        case WIFI_EVENT_SCAN_DONE:
            LLOGD("wifi scan done");
            lua_pushstring(L, "WLAN_SCAN_DONE");
            lua_call(L, 1, 0);
            break;
        case WIFI_EVENT_AP_START:
            LLOGD("wifi ap start");
            macaddr_restore(1);
            break;
        case WIFI_EVENT_AP_STOP:
            LLOGD("wifi ap stop");
            break;
        case WIFI_EVENT_AP_STACONNECTED :
            LLOGD("wifi ap sta connected");
            lua_pushstring(L, "WLAN_AP_CONNECTED");
            lua_call(L, 1, 0);
            break;
        case WIFI_EVENT_AP_STADISCONNECTED :
            LLOGD("wifi ap sta disconnected");
            lua_pushstring(L, "WLAN_AP_DISCONNECTED");
            lua_call(L, 1, 0);
            break;
        default:
            LLOGI("unkown event %d", event_id);
            break;
        }
    }
    else if (event_tp == 1) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            luat_wlan_get_ip(0, buff);
            LLOGD("sta_ip %s", buff);
            lua_pushstring(L, "IP_READY");
            lua_pushstring(L, buff);
            lua_pushinteger(L, NW_ADAPTER_INDEX_LWIP_WIFI_STA);
            lua_call(L, 3, 0);
        }
        else if (event_id == IP_EVENT_AP_STAIPASSIGNED) {
            LLOGD("wifi ap sta connected");
            lua_pushstring(L, "WLAN_AP_STA_IP_READY");
            char tmp[32];
            esp_ip4_addr_t ip4 = {
                .addr = (uint32_t)ptr
            };
            sprintf(tmp, IPSTR, IP2STR(&ip4));
            lua_pushstring(L, tmp);
            lua_call(L, 2, 0);
        }
    }
    else if (event_tp == 2) {
        if (event_id == SC_EVENT_SCAN_DONE) {
            LLOGD("smartconfig Scan done");
        }
        else if (event_id == SC_EVENT_SCAN_DONE) {
            LLOGD("smartconfig Found channel");
        }
        else if (event_id == SC_EVENT_GOT_SSID_PSWD) {
            LLOGD("smartconfig Got ssid and password");
            smartconfig_event_got_ssid_pswd_t *evt = sc_evt;
            wifi_config_t wifi_config;
            uint8_t ssid[33] = { 0 };
            uint8_t password[65] = { 0 };
            // uint8_t rvd_data[33] = { 0 };

            bzero(&wifi_config, sizeof(wifi_config_t));
            memcpy(wifi_config.sta.ssid, evt->ssid, sizeof(wifi_config.sta.ssid));
            memcpy(wifi_config.sta.password, evt->password, sizeof(wifi_config.sta.password));
            wifi_config.sta.bssid_set = evt->bssid_set;
            if (wifi_config.sta.bssid_set == true) {
                memcpy(wifi_config.sta.bssid, evt->bssid, sizeof(wifi_config.sta.bssid));
            }

            memcpy(ssid, evt->ssid, sizeof(evt->ssid));
            memcpy(password, evt->password, sizeof(evt->password));
            LLOGD("SSID [%s] PASSWORD [%s]", ssid, password);
            // ESP_LOGI(TAG, "SSID:%s", ssid);
            // ESP_LOGI(TAG, "PASSWORD:%s", password);
            // if (evt->type == SC_TYPE_ESPTOUCH_V2) {
            //     ESP_ERROR_CHECK( esp_smartconfig_get_rvd_data(rvd_data, sizeof(rvd_data)) );
            //     ESP_LOGI(TAG, "RVD_DATA:");
            //     for (int i=0; i<33; i++) {
            //         printf("%02x ", rvd_data[i]);
            //     }
            //     printf("\n");
            // }

            esp_wifi_disconnect();
            esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
            esp_wifi_connect();

            lua_pushstring(L, "SC_RESULT");
            lua_pushstring(L, (const char*)ssid);
            lua_pushstring(L, (const char*)password);
            lua_call(L, 3, 0);
        }
        else if (event_id == SC_EVENT_SEND_ACK_DONE) {
            esp_smartconfig_stop();
            smartconfig_state = 0;
        }
    }
    return 0;
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data) {
    rtos_msg_t msg = {0};
    msg.handler = l_wlan_handler;
    msg.arg1 = event_id;

    LLOGD("wifi event %d", event_id);
    switch (event_id) {
        case WIFI_EVENT_STA_DISCONNECTED: {
            wlan_is_ready = 0;
            #ifdef LUAT_USE_NETWORK
            net_lwip2_set_link_state(NW_ADAPTER_INDEX_LWIP_WIFI_STA, 0);
            #endif
            // wifi_event_sta_disconnected_t* sta = (wifi_event_sta_disconnected_t*)event_data;
            memset(sta_connected_bssid, 0, sizeof(sta_connected_bssid));
            break;
        }
        case WIFI_EVENT_STA_CONNECTED: {
            wifi_event_sta_connected_t *sta = (wifi_event_sta_connected_t*)event_data;
            memcpy(sta_connected_bssid, sta->bssid, 6);
            break;
        }
        case WIFI_EVENT_AP_START: {
            #ifdef LUAT_USE_NETWORK
            net_lwip2_set_netif(NW_ADAPTER_INDEX_LWIP_WIFI_AP, esp_netif_get_netif_impl(wifiAP));
            net_lwip2_set_link_state(NW_ADAPTER_INDEX_LWIP_WIFI_AP, 1);
            #endif
            break;
        }
        case WIFI_EVENT_AP_STOP: {
            #ifdef LUAT_USE_NETWORK
            net_lwip2_set_link_state(NW_ADAPTER_INDEX_LWIP_WIFI_AP, 0);
            #endif
            break;
        }
        case WIFI_EVENT_STA_BEACON_TIMEOUT : {
            LLOGD("wifi sta beacon timeout");
            return;
        }
    }
    luat_msgbus_put(&msg, 0);
}

static void ip_event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data) {
    rtos_msg_t msg = {0};
    msg.handler = l_wlan_handler;
    msg.arg1 = event_id;
    msg.arg2 = 1;
    // ip_event_got_ip_t *event;
    ip_event_ap_staipassigned_t* ap_sta_event;

    LLOGD("ip event %d", event_id);
    if (event_id == IP_EVENT_STA_GOT_IP) {
        wlan_is_ready = 1;
        // event = (ip_event_got_ip_t*)event_data;
        // sprintf(sta_ip, IPSTR, IP2STR(&event->ip_info.ip));
        // sprintf(sta_gw, IPSTR, IP2STR(&event->ip_info.gw));
        #ifdef LUAT_USE_NETWORK
        net_lwip2_set_netif(NW_ADAPTER_INDEX_LWIP_WIFI_STA, esp_netif_get_netif_impl(wifiSTA));
        net_lwip2_set_link_state(NW_ADAPTER_INDEX_LWIP_WIFI_STA, 1);
        #endif
    }
    else if (event_id == IP_EVENT_AP_STAIPASSIGNED) {
        ap_sta_event = (ip_event_ap_staipassigned_t*)event_data;
        char tmp[32];
        sprintf(tmp, IPSTR, IP2STR(&ap_sta_event->ip));
        LLOGD("ap sta ip %s", tmp);
        msg.ptr = (void*)ap_sta_event->ip.addr;
    }
    luat_msgbus_put(&msg, 0);
    // LLOGD("ip_event_handler is done");
}

static void sc_event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data) {
    rtos_msg_t msg = {0};
    msg.handler = l_wlan_handler;
    msg.arg1 = event_id;
    msg.arg2 = 2;
    
    if (event_id == SC_EVENT_GOT_SSID_PSWD && sc_evt != NULL) {
        memcpy(sc_evt, event_data, sizeof(smartconfig_event_got_ssid_pswd_t));
    }

    LLOGD("sc event %d", event_id);
    luat_msgbus_put(&msg, 0);
}

extern void luat_ntp_autosync(void);
int luat_wlan_init(luat_wlan_config_t *conf) {
    int ret = 0;
    if (wlan_inited == 0) {
        esp_netif_init();
        esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
        esp_event_handler_register(IP_EVENT,   ESP_EVENT_ANY_ID, &ip_event_handler,   NULL);
        esp_event_handler_register(SC_EVENT,   ESP_EVENT_ANY_ID, &sc_event_handler,   NULL);

        wifiSTA = esp_netif_create_default_wifi_sta();

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        cfg.static_rx_buf_num = 2;
        cfg.static_tx_buf_num = 2;

        ret = esp_wifi_init(&cfg);
        if (ret)
            LLOGD("esp_wifi_init ret %d", ret);
        esp_wifi_set_mode(WIFI_MODE_STA);
        ret = esp_netif_set_hostname(wifiSTA, luat_wlan_get_hostname(0));
        #ifdef LUAT_USE_NETWORK
            net_lwip2_register_adapter(NW_ADAPTER_INDEX_LWIP_WIFI_AP);
            net_lwip2_register_adapter(NW_ADAPTER_INDEX_LWIP_WIFI_STA);
        #endif
        if (ret)
            LLOGD("esp_netif_set_hostname ret %d", ret);
        if (dhcp_enable) {
            // LLOGD("自动启动dhcp");
            ret = esp_netif_dhcpc_start(wifiSTA);
            if (ret)
                LLOGD("esp_netif_dhcpc_start ret %d", ret);
        }
        else {
            // LLOGD("禁用dhcp");
        }
    }
#ifdef LUAT_USE_NIMBLE
#if CONFIG_BT_ENABLED
    //esp_wifi_set_ps(WIFI_PS_NONE);
#endif
#endif
    ret = esp_wifi_start();
    if (ret)
        LLOGD("esp_wifi_start ret %d", ret);
    
    wlan_inited = 1;

    // 是否自动开启ntp
#ifndef LUAT_USE_SNTP_NOT_AUTO
    luat_ntp_autosync();
#endif

    return 0;
}

// 设置wifi模式
int luat_wlan_mode(luat_wlan_config_t *conf) {
    switch (conf->mode)
    {
    case LUAT_WLAN_MODE_NULL:
        esp_wifi_set_mode(WIFI_MODE_NULL);
        break;
    case LUAT_WLAN_MODE_STA:
        esp_wifi_set_mode(WIFI_MODE_STA);
        break;
    case LUAT_WLAN_MODE_AP:
        esp_wifi_set_mode(WIFI_MODE_AP);
        break;
    case LUAT_WLAN_MODE_APSTA:
        esp_wifi_set_mode(WIFI_MODE_APSTA);
        break;
    }
    return 0;
}

// 是否已经连上wifi
int luat_wlan_ready(void) {
    return wlan_is_ready;
}

int luat_wlan_connect(luat_wlan_conninfo_t* info) {
    int ret = 0;
    wifi_config_t cfg = {0};
    // 允许重用一起的ssid和passwd数据直接重连
    if (strlen(info->ssid) || info->bssid[5]) {
        // LLOGD("connect %s %s", info->ssid, info->password);
        memcpy(cfg.sta.ssid, info->ssid, strlen(info->ssid));
        memcpy(cfg.sta.password, info->password, strlen(info->password));
        memcpy(cfg.sta.bssid, info->bssid, 6);
        esp_wifi_set_config(WIFI_IF_STA, &cfg);
    }
    auto_reconnection = 1;
    
    ret = esp_wifi_connect();
    if (ret)
        LLOGD("esp_wifi_connect ret %d", ret);
    return ret;
}

int luat_wlan_disconnect(void) {
    int ret = 0;
    auto_reconnection = 0;
    ret = esp_wifi_disconnect();
    if (ret)
        LLOGD("esp_wifi_disconnect ret %d", ret);
    return ret;
}

int luat_wlan_scan(void) {
    static const wifi_scan_config_t conf = {0};
    return esp_wifi_scan_start(&conf, false);
}

int luat_wlan_scan_get_result(luat_wlan_scan_result_t *results, size_t ap_limit) {
    uint16_t num = ap_limit;
    luat_wlan_scan_result_t *tmp = results;
    wifi_ap_record_t *ap_info = luat_heap_malloc(sizeof(wifi_ap_record_t) * ap_limit);
    if (ap_info == NULL)
        return 0;
    esp_wifi_scan_get_ap_records(&num, ap_info);
    for (size_t i = 0; i < num; i++)
    {
        memcpy(tmp[i].ssid, ap_info[i].ssid, strlen((const char*)ap_info[i].ssid));
        memcpy(tmp[i].bssid, ap_info[i].bssid, 6);
        tmp[i].rssi = ap_info[i].rssi;
    }
    luat_heap_free(ap_info);
    return num;
}

int luat_wlan_smartconfig_start(int tp) {
    if (smartconfig_state != 0) {
        LLOGI("smartconfig is running");
        return 0;
    }
    switch (tp)
    {
    case LUAT_SC_TYPE_ESPTOUCH:
        esp_smartconfig_set_type(SC_TYPE_ESPTOUCH);
        break;
    case LUAT_SC_TYPE_ESPTOUCH_AIRKISS:
        esp_smartconfig_set_type(SC_TYPE_ESPTOUCH_AIRKISS);
        break;
    case LUAT_SC_TYPE_AIRKISS:
        esp_smartconfig_set_type(SC_TYPE_AIRKISS);
        break;
    case LUAT_SC_TYPE_ESPTOUCH_V2:
        esp_smartconfig_set_type(SC_TYPE_ESPTOUCH_V2);
        break;
    default:
        esp_smartconfig_set_type(SC_TYPE_ESPTOUCH);
        break;
    }
    if (sc_evt == NULL) {
        sc_evt = luat_heap_malloc(sizeof(smartconfig_event_got_ssid_pswd_t));
    }
    smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
    esp_smartconfig_start(&cfg);
    smartconfig_state = 1;
    return 0;
}

int luat_wlan_smartconfig_stop(void) {
    esp_smartconfig_stop();
    smartconfig_state = 0;
    return 0;
}

int luat_wlan_get_mac(int id, char* mac) {
    // LLOGD("尝试读取MAC %d", id);
    int ret = -1;
    if (id == 0 && wifiSTA != NULL) {
        ret = esp_netif_get_mac(wifiSTA, (uint8_t*)mac);
    }
    if (id == 1 && wifiAP != NULL) {
        ret = esp_netif_get_mac(wifiAP, (uint8_t*)mac);
    }
    return ret;
}

int luat_wlan_set_mac(int id, const char* mac) {
    const char emac[6] = {0,0,0,0,0,0};
    // LLOGD("设置MAC地址 %d %02X%02X%02X%02X%02X%02X", id, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    int ret = -1;
    if (id == 0 && wifiSTA != NULL) {
        if (!memcmp(mac, emac, 6)) {
            // 清除自定义mac
            macaddr_set(0, NULL);
            return 0;
        }
        ret = esp_netif_set_mac(wifiSTA, (uint8_t*)mac);
        if (ret == 0) {
            macaddr_set(0, (uint8_t*)mac);
        }
    }
    if (id == 1 && wifiAP != NULL) {
        if (!memcmp(mac, emac, 6)) {
            // 清除自定义mac
            macaddr_set(1, NULL);
            return 0;
        }
        ret = esp_netif_set_mac(wifiAP, (uint8_t*)mac);
        if (ret == 0) {
            macaddr_set(1, (uint8_t*)mac);
        }
    }
    if (ret)
        LLOGE("设置结果 %d", ret);
    return ret;
}

static inline uint32_t to_esp_ipv4(uint8_t* data) {
    return data[0] + (data[1] << 8) + (data[2] << 16) + (data[3] << 24);
}

int luat_wlan_ap_start(luat_wlan_apinfo_t *apinfo) {
    wifi_mode_t mode = 0;
    int ret = 0;
    wifi_config_t cfg = {0};

    cfg.ap.channel = 1;
    cfg.ap.max_connection = 5;

    memcpy(cfg.ap.ssid, apinfo->ssid, strlen(apinfo->ssid));
    cfg.ap.ssid_len = strlen(apinfo->ssid);
    if (strlen(apinfo->password) >= 6) {
        memcpy(cfg.ap.password, apinfo->password, strlen(apinfo->password));
        cfg.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    }
    else {
        cfg.ap.authmode = WIFI_AUTH_OPEN;
    }
    if (apinfo->channel) {
        cfg.ap.channel = apinfo->channel;
    }
    if (apinfo->hidden) {
        cfg.ap.ssid_hidden = apinfo->hidden;
    }
    if (apinfo->max_conn) {
        cfg.ap.max_connection = apinfo->max_conn;
    }

    LLOGD("softap %s %s", apinfo->ssid, apinfo->password);

    ret = esp_wifi_stop();
    if (ret)
        LLOGD("esp_wifi_stop ret %d", ret);

    if (wifiAP == NULL) {
        wifiAP = esp_netif_create_default_wifi_ap();
    }
    esp_netif_ip_info_t ipInfo = {0};
    if (apinfo->gateway[0]) {
        ipInfo.ip.addr = apinfo->gateway[0] + (apinfo->gateway[1] << 8) + (apinfo->gateway[2] << 16) + (apinfo->gateway[3] << 24);
        ipInfo.gw.addr =   apinfo->gateway[0] + (apinfo->gateway[1] << 8) + (apinfo->gateway[2] << 16) + (apinfo->gateway[3] << 24);
        ipInfo.netmask.addr =   apinfo->netmask[0] + (apinfo->netmask[1] << 8) + (apinfo->netmask[2] << 16) + (apinfo->netmask[3] << 24);
        ret = esp_netif_dhcps_stop(wifiAP);
        if (ret)
            LLOGD("esp_netif_dhcps_stop ret %d", ret);
        ret = esp_netif_set_ip_info(wifiAP, &ipInfo);
        if (ret)
            LLOGD("esp_netif_set_ip_info ret %d", ret);
        ret = esp_netif_dhcps_start(wifiAP);
        if (ret)
            LLOGD("esp_netif_dhcps_start ret %d", ret);
    }
    else {
        LLOGD("default gw 192.168.4.1 mask 255.255.255.0");
    }

    ret = esp_wifi_get_mode(&mode);
    if (mode == WIFI_MODE_NULL || mode == WIFI_MODE_STA) {
        LLOGI("auto set to APSTA mode");
        ret = esp_wifi_set_mode(WIFI_MODE_APSTA);
        if (ret)
            LLOGD("esp_wifi_set_mode ret %d", ret);
    }
    ret = esp_wifi_set_config(WIFI_IF_AP, &cfg);
    if (ret)
        LLOGD("esp_wifi_set_config ret %d", ret);
    ret = esp_wifi_start();
    if (ret)
        LLOGD("esp_wifi_start ret %d", ret);
    return ret;
}

int luat_wlan_get_ip(int type, char* data) {
    struct netif* nt = NULL;
    if (type == 0) {
        if (wifiSTA != NULL)
            nt = esp_netif_get_netif_impl(wifiSTA);
    }
    else {
        if (wifiAP != NULL)
            nt = esp_netif_get_netif_impl(wifiAP);
    }
    if (nt == NULL) {
        data[0] = 0;
        return 0;
    }
    
    // Use lwIP's built-in functions instead of the framework's macros
    ipaddr_ntoa_r(&nt->ip_addr, data, IP4ADDR_STRLEN_MAX);
    return 0;
}

int luat_wlan_set_ps(int mode) {
    if (mode == 0) {
        esp_wifi_set_ps(WIFI_PS_NONE);
    }
    else if (mode == 1) {
        esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    }
    else if (mode == 2) {
        esp_wifi_set_ps(WIFI_PS_MAX_MODEM);
    }
    else {
        LLOGE("unkown wifi ps mode %d", mode);
        return -1;
    }
    return 0;
}

int luat_wlan_get_ps(void) {
    wifi_ps_type_t mode = WIFI_PS_NONE;
    esp_wifi_get_ps(&mode);
    return mode;
}

int luat_wlan_get_ap_bssid(char* buff) {
    memcpy(buff, sta_connected_bssid, 6);
    return 0;
}

int luat_wlan_get_ap_rssi(void) {
    wifi_ap_record_t ap;
    esp_wifi_sta_get_ap_info(&ap);
    return ap.rssi;
}

int luat_wlan_get_ap_gateway(char* buff) {
    esp_netif_ip_info_t ipInfo = {0};
    buff[0] = 0x00;
    if (wifiSTA == NULL) {
        return 0;
    }
    int ret = esp_netif_get_ip_info(wifiSTA, &ipInfo);
    if (ret == 0) {
        sprintf(buff, IPSTR, IP2STR(&ipInfo.gw));
    }
    return 0;
}

const char* luat_wlan_get_hostname(int id) {
    if (luat_sta_hostname[0] == 0) {
        char mac[6];
        esp_read_mac((uint8_t*)mac, 0);
        sprintf_(luat_sta_hostname, "LUATOS_%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
    return (const char*)luat_sta_hostname;
}

int luat_wlan_set_hostname(int id, const char* hostname) {
    if (hostname == NULL || hostname[0] == 0) {
        return 0;
    }
    memcpy(luat_sta_hostname, hostname, strlen(hostname) + 1);
    esp_netif_set_hostname(wifiSTA, luat_sta_hostname);
    return 0;
}

int luat_wlan_ap_stop(void) {
    esp_wifi_stop();
    return 0;
}

int luat_wlan_set_station_ip(luat_wlan_station_info_t *info) {
    esp_netif_ip_info_t ipInfo = {0};
    int ret = 0;
    if (wifiSTA == NULL) {
        LLOGE("call wlan.init() first");
        return -1;
    }
    dhcp_enable = info->dhcp_enable;
    if (!info->dhcp_enable) {
        esp_netif_dhcpc_stop(wifiSTA);
        ipInfo.ip.addr = to_esp_ipv4(info->ipv4_addr);
        ipInfo.gw.addr = to_esp_ipv4(info->ipv4_gateway);
        ipInfo.netmask.addr = to_esp_ipv4(info->ipv4_netmask);
        ret = esp_netif_set_ip_info(wifiSTA, &ipInfo);
        if (ret)
            LLOGD("esp_netif_set_ip_info ret %d", ret);
        LLOGD("dhcp is disabled");
        LLOGD("Sta IP %d.%d.%d.%d MASK %d.%d.%d.%d GW %d.%d.%d.%d", 
                info->ipv4_addr[0],   info->ipv4_addr[1],   info->ipv4_addr[2],   info->ipv4_addr[3],
                info->ipv4_netmask[0],info->ipv4_netmask[1],info->ipv4_netmask[2],info->ipv4_netmask[3],
                info->ipv4_gateway[0],info->ipv4_gateway[1],info->ipv4_gateway[2],info->ipv4_gateway[3]
        );
    }
    else {
        ret = esp_netif_dhcpc_start(wifiSTA);
        if (ret)
            LLOGD("esp_netif_dhcpc_start ret %d", ret);
        LLOGD("dhcp enabled");
    }
    return ret;
}
