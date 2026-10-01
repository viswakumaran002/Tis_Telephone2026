/**
 * @file lan_ecu.c
 * @author Triove innovation solutions 
 * @name viswakumaran
 * @version 0.1
 * @date 2026-10-1
 * 
 * @copyright Copyright (c) 2026
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <arpa/inet.h>

#include "lan_ecu.h"

static int  ecu_lan_ctrl_sock   = -1;
static bool ecu_lan_initialized = false;

/**
 * @brief  Initializes the ECU network subsystem control socket.
 * @param  None
 * @return ECU_LAN_STATUS_OK on success, ECU_LAN_STATUS_IOCTL_ERROR on socket creation failure.
 */
ecu_lan_statustype ecu_lan_init(void)
{
    if(ecu_lan_initialized) return ECU_LAN_STATUS_OK;

    /* Open a standard DGRAM socket strictly used for kernel SIOC ioctl operations */
    ecu_lan_ctrl_sock = socket(AF_INET,SOCK_DGRAM,0);

    if(ecu_lan_ctrl_sock < 0) return ECU_LAN_STATUS_IOCTL_ERROR;

    ecu_lan_initialized = true;
    return ECU_LAN_STATUS_OK;
}

/**
 * @brief  Closes the kernel control socket and resets driver state.
 * @param  None
 * @return None (void)
 */
void ecu_lan_deinit(void)
{
    if(ecu_lan_ctrl_sock >= 0)
    {
        close(ecu_lan_ctrl_sock);
        ecu_lan_ctrl_sock = -1;
    }
    ecu_lan_initialized = false;
}

/**
 * @brief  Reads physical RJ45 Ethernet link/carrier status from sysfs.
 * @param  ifname       [in]  Interface name (e.g., "lan1", "br-lan").
 * @param  is_connected [out] Pointer to bool; true if physical link up, false otherwise.
 * @return ECU_LAN_STATUS_OK on success, ECU_LAN_STATUS_CARRIER_READ_ERROR if file read fails.
 */
ecu_lan_statustype ecu_lan_getcarrierstate(const char *ifname, bool *is_connected)
{
    if(!ecu_lan_initialized) return ECU_LAN_STATUS_NOT_INITIALIZED;

    if(!ifname || !is_connected) return ECU_LAN_STATUS_INVALID_PARAM;

    char path[64];
    snprintf(path,sizeof(path), "/sys/class/net/%s/carrier",ifname);

    int fd = open(path,O_RDONLY);
    if(fd < 0)
    {
        *is_connected = false;
        return ECU_LAN_STATUS_CARRIER_READ_ERROR;
    }

    char buf[4] = {0};
    ssize_t bytes_read = read(fd,buf,sizeof(buf)-1);
    close(fd);

    if(bytes_read <= 0)
    {
        *is_connected = false;
        return ECU_LAN_STATUS_CARRIER_READ_ERROR;
    }

    /* '1' indicates PHY lock/link pulse established, '0' indicates cable disconnected */
    *is_connected = (buf[0] == '1');
    return ECU_LAN_STATUS_OK;
}

/**
 * @brief  Administratively enables (UP) or disables (DOWN) the network interface.
 * @param  ifname [in] Interface name (e.g., "lan1").
 * @param  up     [in] true to bring interface UP, false to bring DOWN.
 * @return ECU_LAN_STATUS_OK on success, ECU_LAN_STATUS_DEVICE_NOT_FOUND or ECU_LAN_STATUS_IOCTL_ERROR on failure.
 */
ecu_lan_statustype ecu_lan_setadminstate(const char *ifname, bool up)
{
    if(!ecu_lan_initialized) return ECU_LAN_STATUS_NOT_INITIALIZED;

    if(!ifname) return ECU_LAN_STATUS_INVALID_PARAM;
    
    struct ifreq ifr;                                                                               // setting network interface to unix in operating system
    memset(&ifr,0,sizeof(ifr));
    strncpy(ifr.ifr_name,ifname,IFNAMSIZ - 1);

    /* Step 1: Read existing interface flags */
    if(ioctl(ecu_lan_ctrl_sock,SIOCGIFFLAGS,&ifr) < 0) return ECU_LAN_STATUS_DEVICE_NOT_FOUND;

    /* Step 2: Set or clear IFF_UP flag */
    if(up) ifr.ifr_flags |= (IFF_UP | IFF_RUNNING);  // set bit
    else ifr.ifr_flags &= ~IFF_UP;                   // clear bit

    if(ioctl(ecu_lan_ctrl_sock, SIOCSIFFLAGS, &ifr) < 0) return ECU_LAN_STATUS_IOCTL_ERROR;

    return ECU_LAN_STATUS_OK;
}

/**
 * @brief  Reads whether the interface is administratively configured UP or DOWN.
 * @param  ifname [in]  Interface name (e.g., "lan1").
 * @param  is_up  [out] Pointer to bool; true if UP flag is set, false if DOWN.
 * @return ECU_LAN_STATUS_OK on success, ECU_LAN_STATUS_DEVICE_NOT_FOUND on failure.
 */
ecu_lan_statustype ecu_lan_getadminstate(const char *ifname, bool *is_up) 
{
    if (!ecu_lan_initialized) 
    {
        return ECU_LAN_STATUS_NOT_INITIALIZED;
    }
    if (!ifname || !is_up) 
    {
        return ECU_LAN_STATUS_INVALID_PARAM;
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);

    if (ioctl(ecu_lan_ctrl_sock, SIOCGIFFLAGS, &ifr) < 0) 
    {
        return ECU_LAN_STATUS_DEVICE_NOT_FOUND;
    }

    *is_up = (ifr.ifr_flags & IFF_UP) != 0;
    return ECU_LAN_STATUS_OK;
}

/**
 * @brief  Reads the 6-byte hardware MAC address from the network controller.
 * @param  ifname  [in]  Interface name (e.g., "lan1").
 * @param  mac_out [out] Buffer of at least 6 bytes to store the hardware MAC address.
 * @return ECU_LAN_STATUS_OK on success, ECU_LAN_STATUS_IOCTL_ERROR on failure.
 */
ecu_lan_statustype ecu_lan_getmacaddress(const char *ifname, uint8_t mac_out[6])
{
  if(!ecu_lan_initialized) return ECU_LAN_STATUS_NOT_INITIALIZED;
  if(!ifname || !mac_out)  return ECU_LAN_STATUS_INVALID_PARAM;

  struct ifreq ifr;
  memset(&ifr,0,sizeof(ifr));
  strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);

  if(ioctl(ecu_lan_ctrl_sock,SIOCGIFHWADDR,&ifr) < 0) return ECU_LAN_STATUS_IOCTL_ERROR;
  memcpy(mac_out,ifr.ifr_hwaddr.sa_data,6);
  return ECU_LAN_STATUS_OK;
}

/**
 * @brief  Programs a new hardware MAC address into the network controller.
 * @param  ifname [in] Interface name (e.g., "lan1"). Interface should be DOWN first.
 * @param  mac_in [in] 6-byte array containing the new MAC address to set.
 * @return ECU_LAN_STATUS_OK on success, ECU_LAN_STATUS_IOCTL_ERROR on failure.
 */
ecu_lan_statustype ecu_lan_setmacaddress(const char *ifname,const uint8_t mac_in[6])
{
    if(!ecu_lan_initialized) return ECU_LAN_STATUS_NOT_INITIALIZED;
    if(!ifname || !mac_in)     return ECU_LAN_STATUS_INVALID_PARAM;

    struct ifreq ifr;
    memset(&ifr,0,sizeof(ifr));
    strncpy(ifr.ifr_name,ifname,IFNAMSIZ-1);

    /* ARPHRD_ETHER specifies standard Ethernet hardware type */
    ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;
    memcpy(ifr.ifr_hwaddr.sa_data,mac_in,6);

    if(ioctl(ecu_lan_ctrl_sock,SIOCSIFHWADDR,&ifr)< 0) return ECU_LAN_STATUS_IOCTL_ERROR;

    return ECU_LAN_STATUS_OK;
}

/**
 * @brief  Queries the Maximum Transmission Unit (MTU) packet size.
 * @param  ifname  [in]  Interface name (e.g., "lan1", "br-lan").
 * @param  mtu_out [out] Pointer to uint32_t to store the retrieved MTU value.
 * @return ECU_LAN_STATUS_OK on success, ECU_LAN_STATUS_IOCTL_ERROR on failure.
 */
ecu_lan_statustype ecu_lan_getmtu(const char *ifname,uint32_t *mtu_out)
{
    if(!ecu_lan_initialized) return ECU_LAN_STATUS_NOT_INITIALIZED;
    if(!ifname || !mtu_out)  return ECU_LAN_STATUS_INVALID_PARAM;

    struct ifreq ifr;
    memset(&ifr,0,sizeof(ifr));
    strncpy(ifr.ifr_name,ifname,IFNAMSIZ-1);

    if(ioctl(ecu_lan_ctrl_sock,SIOCGIFMTU,&ifr) < 0) return ECU_LAN_STATUS_IOCTL_ERROR;

    *mtu_out = (uint32_t)ifr.ifr_mtu;
    return ECU_LAN_STATUS_OK;
}

/**
 * @brief  Sets the Maximum Transmission Unit (MTU) packet size on the interface.
 * @param  ifname  [in] Interface name (e.g., "lan1").
 * @param  mtu_val [in] New MTU size in bytes (minimum 68 for IPv4).
 * @return ECU_LAN_STATUS_OK on success, ECU_LAN_STATUS_INVALID_PARAM or ECU_LAN_STATUS_IOCTL_ERROR on failure.
 */
ecu_lan_statustype ecu_lan_setmtu(const char *ifname , uint32_t mtu_val)
{
  if(! ecu_lan_initialized) return ECU_LAN_STATUS_NOT_INITIALIZED;
  if(!ifname || !mtu_val)    return ECU_LAN_STATUS_INVALID_PARAM;

  struct ifreq ifr;
  memset(&ifr,0,sizeof(ifr));
  strncpy(ifr.ifr_name,ifname,IFNAMSIZ - 1);
  ifr.ifr_mtu = (int)mtu_val;

  if(ioctl(ecu_lan_ctrl_sock,SIOCSIFMTU,&ifr) < 0) return ECU_LAN_STATUS_IOCTL_ERROR;
  
  return ECU_LAN_STATUS_OK;
}

/* ========================================================================= */
/* IPv4 Address Operations                                                   */
/* Reference: SIOCGIFADDR / SIOCSIFADDR in man 7 netdevice                   */
/* ========================================================================= */

/**
 * @brief  Retrieves the active IPv4 address assigned to the interface as a string.
 * @param  ifname  [in]  Interface name (e.g., "br-lan").
 * @param  ip_buf  [out] Destination character buffer (minimum 16 bytes for INET_ADDRSTRLEN).
 * @param  buf_len [in]  Length of ip_buf in bytes.
 * @return ECU_LAN_STATUS_OK on success, ECU_LAN_STATUS_IOCTL_ERROR or ECU_LAN_STATUS_PARSE_ERROR on failure.
 */
ecu_lan_statustype ecu_lan_getipv4address(const char *ifname,char *ip_buf, size_t buf_len)
{
    if(!ecu_lan_initialized) return ECU_LAN_STATUS_NOT_INITIALIZED;
    if(!ifname || !ip_buf || buf_len < INET_ADDRSTRLEN) return ECU_LAN_STATUS_INVALID_PARAM;

    struct ifreq ifr;
    memset(&ifr,0,sizeof(ifr));
    strncpy(ifr.ifr_name,ifname,IFNAMSIZ -1 );
    ifr.ifr_addr.sa_family = AF_INET;

    if(ioctl(ecu_lan_ctrl_sock,SIOCGIFADDR,&ifr) < 0) return ECU_LAN_STATUS_IOCTL_ERROR;

    struct sockaddr_in *ip_addr = (struct sockaddr_in *)&ifr.ifr_addr;
    if(!inet_ntop(AF_INET,&ip_addr->sin_addr,ip_buf,buf_len)) return ECU_LAN_STATUS_PARSE_ERROR;

    return ECU_LAN_STATUS_OK;
}
/**
 * @brief  Configures a static IPv4 address on the target network interface.
 * @param  ifname [in] Interface name (e.g., "br-lan").
 * @param  ip_str [in] Dotted-decimal IPv4 string (e.g., "192.168.1.1").
 * @return ECU_LAN_STATUS_OK on success, ECU_LAN_STATUS_INVALID_PARAM or ECU_LAN_STATUS_IOCTL_ERROR on failure.
 */
ecu_lan_statustype ecu_lan_setipv4address(const char *ifname,const char *ip_str)
{
    if(!ecu_lan_initialized) return ECU_LAN_STATUS_NOT_INITIALIZED;
    if(!ifname || !ip_str)    return ECU_LAN_STATUS_INVALID_PARAM;

    struct ifreq ifr;
    memset(&ifr,0,sizeof(ifr));
    strncpy(ifr.ifr_name,ifname,IFNAMSIZ - 1);

    struct sockaddr_in *ip_addr = (struct sockaddr_in *)&ifr.ifr_addr;
    ip_addr->sin_family = AF_INET;

    if(inet_pton(AF_INET,ip_str,&ip_addr->sin_addr) <= 0) return ECU_LAN_STATUS_INVALID_PARAM;

    if(ioctl(ecu_lan_ctrl_sock,SIOCSIFADDR,&ifr) < 0) return ECU_LAN_STATUS_IOCTL_ERROR;

    return ECU_LAN_STATUS_OK;
}
/* ========================================================================= */
/* Subnet Mask Operations                                                    */
/* Reference: SIOCGIFNETMASK / SIOCSIFNETMASK in man 7 netdevice             */
/* ========================================================================= */

/**
 * @brief  Retrieves the active IPv4 subnet mask configured on the interface.
 * @param  ifname   [in]  Interface name (e.g., "br-lan").
 * @param  mask_buf [out] Destination character buffer (minimum 16 bytes for INET_ADDRSTRLEN).
 * @param  buf_len  [in]  Length of mask_buf in bytes.
 * @return ECU_LAN_STATUS_OK on success, ECU_LAN_STATUS_IOCTL_ERROR or ECU_LAN_STATUS_PARSE_ERROR on failure.
 */
ecu_lan_statustype ecu_lan_getsubnetmask(const char *ifname,char *mask_buf,size_t buf_len)
{
    if(!ecu_lan_initialized) return ECU_LAN_STATUS_NOT_INITIALIZED;
    if(!ifname || !mask_buf || buf_len < INET_ADDRSTRLEN) return ECU_LAN_STATUS_INVALID_PARAM;

    struct ifreq ifr;
    memset(&ifr,0,sizeof(ifr));
    strncpy(ifr.ifr_name,ifname,IFNAMSIZ - 1);
    ifr.ifr_netmask.sa_family = AF_INET;

    if(ioctl(ecu_lan_ctrl_sock,SIOCGIFNETMASK,&ifr) < 0) return ECU_LAN_STATUS_IOCTL_ERROR;

    struct sockaddr_in *netmask = (struct sockaddr_in *)&ifr.ifr_netmask;
    if(!inet_ntop(AF_INET,&netmask->sin_addr,mask_buf,buf_len)) return ECU_LAN_STATUS_PARSE_ERROR;

    return ECU_LAN_STATUS_OK;
}
/**
 * @brief  Configures the IPv4 subnet mask on the target network interface.
 * @param  ifname   [in] Interface name (e.g., "br-lan").
 * @param  mask_str [in] Dotted-decimal subnet mask string (e.g., "255.255.255.0").
 * @return ECU_LAN_STATUS_OK on success, ECU_LAN_STATUS_INVALID_PARAM or ECU_LAN_STATUS_IOCTL_ERROR on failure.
 */
ecu_lan_statustype ecu_lan_setsubnetmask(const char *ifname,char *mask_str);
{
    if(!ecu_lan_initialized) return ECU_LAN_STATUS_NOT_INITIALIZED;
    if(!ifname || !mask_str) return ECU_LAN_STATUS_INVALID_PARAM;

    struct ifreq ifr;
    memset(&ifr,0,sizeof(ifr));
    strncpy(ifr.ifr_name,ifname,IFNAMSIZ - 1);

    struct sockaddr_in *netmask = (struct sockaddr_in *)&ifr.ifr_netmask;
    netmask->sin_family = AF_INET;

    if(inet_pton(AF_INET,mask_str,&netmask->sin_addr) <= 0) return ECU_LAN_STATUS_INVALID_PARAM;
    if(ioctl(ecu_lan_ctrl_sock,SIOCSIFNETMASK,&ifr) < 0) return ECU_LAN_STATUS_IOCTL_ERROR;

    return ECU_LAN_STATUS_OK;
}