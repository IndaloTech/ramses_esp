/********************************************************************
 * ramses_esp
 * ramses_wifi.h
 *
 * (C) 2023 Peter Price
 *
 * WiFi Station
 *
 */
#ifndef _RAMSES_WIFI_H_
#define _RAMSES_WIFI_H_

#include "freertos/FreeRTOS.h"

typedef void * WIFI_HNDL;

extern void wifi_set_ssid( char *ssid );
extern void wifi_set_password( char *password );
extern void wifi_restart( void );
extern void wifi_status( void );

extern bool wifi_is_connected(void);

extern WIFI_HNDL ramses_wifi_init( BaseType_t coreID );

#endif // _RAMSES_WIFI_H_
