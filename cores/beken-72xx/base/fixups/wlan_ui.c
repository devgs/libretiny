#include "include.h"
#include "wlan_ui_pub.h"
#include "rw_pub.h"
#include "param_config.h"
#include "wpa_psk_cache.h"

#include "lt_logger.h"

extern void bk_wlan_sta_init_adv(network_InitTypeDef_adv_st *inNetworkInitParaAdv);

extern void bk_wlan_sta_adv_param_2_sta(network_InitTypeDef_adv_st *sta_adv,network_InitTypeDef_st* sta);

OSStatus bk_wlan_start_sta_adv_fix(network_InitTypeDef_adv_st *inNetworkInitParaAdv)
{
    if (bk_wlan_is_monitor_mode()) {
        return 0;
    }

    LT_WM(WIFI, "TESTING1 %u: entering bk_wlan_start_sta_adv_fix", millis());

#if !CFG_WPA_CTRL_IFACE
#if CFG_ROLE_LAUNCH
    rl_status_set_private_state(RL_PRIV_STATUS_STA_ADV_RDY);
    LT_WM(WIFI, "TESTING2 %u", millis());
#endif
#if CFG_ROLE_LAUNCH
    rl_status_set_st_state(RL_ST_STATUS_RESTART_ST);
    LT_WM(WIFI, "TESTING3 %u", millis());
#endif


    bk_wlan_stop(BK_STATION);
    LT_WM(WIFI, "TESTING4 %u", millis());
#if CFG_ROLE_LAUNCH
    if (rl_pre_sta_set_status(RL_STATUS_STA_INITING))
        return -1;

    rl_status_reset_st_state(RL_ST_STATUS_RESTART_HOLD | RL_ST_STATUS_RESTART_ST);
    rl_status_set_private_state(RL_PRIV_STATUS_STA_ADV);
    rl_status_reset_private_state(RL_PRIV_STATUS_STA_ADV_RDY);
    LAUNCH_REQ lreq;

    lreq.req_type = LAUNCH_REQ_STA;
    bk_wlan_sta_adv_param_2_sta(inNetworkInitParaAdv, &lreq.descr);

    rl_sta_adv_register_cache_station(&lreq);
    LT_WM(WIFI, "TESTING5 %u", millis());
#endif

    bk_wlan_sta_init_adv(inNetworkInitParaAdv);
    LT_WM(WIFI, "TESTING6 %u", millis());

    /*
     * Key buffer is not guaranteed to be null terminated: there is a separate
     * `key_len` field that stores the length. We have to copy the data to a
     * temporary stack buffer. Fortunately, `wpa_psk_request()` does not keep
     * the reference, as it's not needed: the end result is PSK that is either
     * derived from the `ssid` + `key` or is already available. `key` is just a
     * transient argument.
     */
    {
        char key_buffer[PMK_LEN + 1];

        /* Make copy with a guaranteed null terminator. */
        os_memcpy(key_buffer, g_sta_param_ptr->key, g_sta_param_ptr->key_len);
        key_buffer[g_sta_param_ptr->key_len + 1] = '\0';

        /*
         * let wpa_psk_cal thread to caculate psk.
         * XXX: If you know psk value, fill last two parameters of `wpa_psk_request()'.
         */
        wpa_psk_request(g_sta_param_ptr->ssid.array, g_sta_param_ptr->ssid.length,
                        key_buffer, NULL, 0);
    }

    supplicant_main_entry(inNetworkInitParaAdv->ap_info.ssid);

    net_wlan_add_netif(&g_sta_param_ptr->own_mac);
    LT_WM(WIFI, "TESTING7 %u", millis());


#else /* CFG_WPA_CTRL_IFACE */
    wlan_sta_config_t config;
    LT_WM(WIFI, "TESTING8 %u", millis());

#if (CFG_SOC_NAME == SOC_BK7231N)
    LT_WM(WIFI, "TESTING9 %u", millis());
    if (get_ate_mode_state()) {
        LT_WM(WIFI, "TESTING10 %u", millis());
        // cunliang20210407 set blk_standby_cfg with blk_txen_cfg like txevm, qunshan confirmed
        rwnx_cal_en_extra_txpa();
    }
#endif
    bk_wlan_sta_init_adv(inNetworkInitParaAdv);

    /*
     * Key buffer is not guaranteed to be null terminated: there is a separate
     * `key_len` field that stores the length. We have to copy the data to a
     * temporary stack buffer. Fortunately, `wpa_psk_request()` does not keep
     * the reference, as it's not needed: the end result is PSK that is either
     * derived from the `ssid` + `key` or is already available. `key` is just a
     * transient argument.
     */
    {
        char key_buffer[PMK_LEN + 1];

        /* Make copy with a guaranteed null terminator. */
        os_memcpy(key_buffer, g_sta_param_ptr->key, g_sta_param_ptr->key_len);
        key_buffer[g_sta_param_ptr->key_len + 1] = '\0';

        /*
         * let wpa_psk_cal thread to caculate psk.
         * XXX: If you know psk value, fill last two parameters of `wpa_psk_request()'.
         */
        wpa_psk_request(g_sta_param_ptr->ssid.array, g_sta_param_ptr->ssid.length,
                        key_buffer, NULL, 0);
    }

    /* disconnect previous connected network */
    wlan_sta_disconnect();

    /* start wpa_supplicant */
    wlan_sta_enable();

    /* set network parameters: ssid, passphase */
    wlan_sta_set((uint8_t*)inNetworkInitParaAdv->ap_info.ssid, os_strlen(inNetworkInitParaAdv->ap_info.ssid),
                 (uint8_t*)inNetworkInitParaAdv->key);

    LT_WM(WIFI, "TESTING11 %u", millis());
    /* set fast connect bssid */
    os_memset(&config, 0, sizeof(config));
    /*
     * This API has no way to communicate whether BSSID was supplied or not.
     * So we will use an assumption that no valid endpoint will ever use the
     * `00:00:00:00:00:00`. It's basically a MAC address and no valid device
     * should ever use the value consisting of all zeroes.
     *
     * We have just zeroed out the `config` struct so we can use those zeroes
     * for comparison with our input argument.
     */
    char hex_table[] = "0123456789abcdef";

    LT_WM(WIFI, "TESTING11 %u: config bssid=%c%c:%c%c:%c%c:%c%c:%c%c:%c%c"
            , millis()
            , hex_table[(config.u.bssid[0] >> 4) & 0xF]
            , hex_table[config.u.bssid[0] & 0xF]
            , hex_table[(config.u.bssid[1] >> 4) & 0xF]
            , hex_table[config.u.bssid[1] & 0xF]
            , hex_table[(config.u.bssid[2] >> 4) & 0xF]
            , hex_table[config.u.bssid[2] & 0xF]
            , hex_table[(config.u.bssid[3] >> 4) & 0xF]
            , hex_table[config.u.bssid[3] & 0xF]
            , hex_table[(config.u.bssid[4] >> 4) & 0xF]
            , hex_table[config.u.bssid[4] & 0xF]
            , hex_table[(config.u.bssid[5] >> 4) & 0xF]
            , hex_table[config.u.bssid[5] & 0xF]
            );

    LT_WM(WIFI, "TESTING12 %u: param bssid=%c%c:%c%c:%c%c:%c%c:%c%c:%c%c"
            , millis()
            , hex_table[(inNetworkInitParaAdv->ap_info.bssid[0] >> 4) & 0xF]
            , hex_table[inNetworkInitParaAdv->ap_info.bssid[0] & 0xF]
            , hex_table[(inNetworkInitParaAdv->ap_info.bssid[1] >> 4) & 0xF]
            , hex_table[inNetworkInitParaAdv->ap_info.bssid[1] & 0xF]
            , hex_table[(inNetworkInitParaAdv->ap_info.bssid[2] >> 4) & 0xF]
            , hex_table[inNetworkInitParaAdv->ap_info.bssid[2] & 0xF]
            , hex_table[(inNetworkInitParaAdv->ap_info.bssid[3] >> 4) & 0xF]
            , hex_table[inNetworkInitParaAdv->ap_info.bssid[3] & 0xF]
            , hex_table[(inNetworkInitParaAdv->ap_info.bssid[4] >> 4) & 0xF]
            , hex_table[inNetworkInitParaAdv->ap_info.bssid[4] & 0xF]
            , hex_table[(inNetworkInitParaAdv->ap_info.bssid[5] >> 4) & 0xF]
            , hex_table[inNetworkInitParaAdv->ap_info.bssid[5] & 0xF]
            );

    if (os_memcmp(config.u.bssid, inNetworkInitParaAdv->ap_info.bssid, ETH_ALEN)) {
        LT_WM(WIFI, "TESTING13 %u: doing bssid setup", millis());
        os_memcpy(config.u.bssid, inNetworkInitParaAdv->ap_info.bssid, ETH_ALEN);
        config.field = WLAN_STA_FIELD_BSSID;
        wpa_ctrl_request(WPA_CTRL_CMD_STA_SET, &config);
    }

    /* set fast connect freq */
    LT_WM(WIFI, "TESTING14 %u: param channel=%u", millis(), inNetworkInitParaAdv->ap_info.channel);
    os_memset(&config, 0, sizeof(config));
    config.u.channel = inNetworkInitParaAdv->ap_info.channel;
    config.field = WLAN_STA_FIELD_FREQ;
    wpa_ctrl_request(WPA_CTRL_CMD_STA_SET, &config);

    /* connect to AP */
    wlan_sta_connect(g_sta_param_ptr->fast_connect_set ? g_sta_param_ptr->fast_connect.chann : 0);

#endif
    ip_address_set(BK_STATION, inNetworkInitParaAdv->dhcp_mode,
                   inNetworkInitParaAdv->local_ip_addr,
                   inNetworkInitParaAdv->net_mask,
                   inNetworkInitParaAdv->gateway_ip_addr,
                   inNetworkInitParaAdv->dns_server_ip_addr);

    return 0;
}
