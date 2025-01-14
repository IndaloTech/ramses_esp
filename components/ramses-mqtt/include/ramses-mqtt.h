/********************************************************************
 * ramses_esp
 * ramses-mqtt.h
 *
 * (C) 2023 Peter Price
 *
 * MQTT Client
 *
 */
#ifndef _RAMSES_MQTT_H_
#define _RAMSES_MQTT_H_

typedef void * MQTT_HNDL;

extern char const *NET_get_mqtt_broker(void);
extern char const *NET_get_mqtt_user(void);
extern char const *NET_get_mqtt_password(void);

extern MQTT_HNDL ramses_mqtt_init( BaseType_t coreID );
extern void MQTT_publish_rx( char const *ts, char const *msg );

#endif // _RAMSES_MQTT_H_
