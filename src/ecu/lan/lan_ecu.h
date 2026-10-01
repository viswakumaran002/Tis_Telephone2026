/**
 * @file lan_ecu.h
 * @author Triove innovation solutions 
 * @name viswakumaran
 * @version 0.1
 * @date 2026-10-1
 * 
 * @copyright Copyright (c) 2026
 */

#ifndef LAN_ECU_H
#define LAN_ECU_H

#include <stdbool.h>
#include <stdint.h>

/*=============>  Standard Return Status Codes <==============*/

typedef enum
{
ECU_LAN_STATUS_OK = 0,
ECU_LAN_STATUS_NOT_INITIALIZED,
ECU_LAN_STATUS_INVALID_PARAM,
ECU_LAN_STATUS_DEVICE_NOT_FOUND,
ECU_LAN_STATUS_IOCTL_ERROR,
ECU_LAN_STATUS_CARRIER_READ_ERROR,
ECU_LAN_STATUS_PARSE_ERROR
}ecu_lan_statustype;

ecu_lan_statustype ecu_lan_init(void);
void ecu_lan_deinit(void);

ecu_lan_statustype ecu_lan_getcarrierstate(const char *ifname, bool *is_connected);
ecu_lan_statustype ecu_lan_setadminstate(const char *ifname, bool up);
ecu_lan_statustype ecu_lan_getadminstate(const char *ifname, bool *is_up);

ecu_lan_statustype ecu_lan_getmacaddress(const char *ifname, uint8_t mac_out[6]);
ecu_lan_statustype ecu_lan_setmacaddress(const char *ifname, const uint8_t mac_in[6]);

ecu_lan_statustype ecu_lan_getmtu(const char *ifname, uint32_t *mtu_out);
ecu_lan_statustype ecu_lan_setmtu(const char *ifname, uint32_t mtu_out);

ecu_lan_statustype ecu_lan_getipv4address(const char *ifname, char *ip_buf, size_t buf_len);
ecu_lan_statustype ecu_lan_setipv4address(const char *ifname, const char *ip_str);

ecu_lan_statustype ecu_lan_getsubnetmask(const char *ifname,char *mask_buf, size_t buf_len);
ecu_lan_statustype ecu_lan_setsubnetmask(const char *ifname,char *mask_str);

#endif 