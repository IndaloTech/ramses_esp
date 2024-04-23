/********************************************************************
 * ramses_esp
 * ramses_network.c
 *
 * (C) 2023 Peter Price
 *
 * Network
 *
 */
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"

#include "esp_err.h"
#include "esp_netif.h"
#include "nvs.h"

#include "ramses_ota.h"
#include "ramses_sntp.h"
#include "ramses_wifi.h"

// NVS identifiers
#define NETWORK_NAMESPACE "network"

#define MQTT_BROKER   "mqtt_broker"
#define MQTT_USER     "mqtt_user"
#define MQTT_PASSWORD "mqtt_password"

#define SNTP_SERVER "sntp_server"
#define TIMEZONE    "timezone"

#include "network_cmd.h"
#include "ramses_network.h"

/********************************************************************************
 * Network context
 */

struct network_data {
  nvs_handle_t nvs;

  size_t mqtt_broker_len;
  char *mqtt_broker;
  size_t mqtt_user_len;
  char *mqtt_user;
  size_t mqtt_password_len;
  char *mqtt_password;

  size_t sntp_server_len;
  char *sntp_server;
  size_t timezone_len;
  char * timezone;
};

static struct network_data *network_ctxt( void ) {
  static struct network_data data;
  static struct network_data * ctxt = NULL;
  if( !ctxt ) {
	ctxt = &data;
	// Runtime initialisation ?
  }

  return ctxt;
}

/********************************************************************************
 * MQTT broker uri
 */

static void net_get_mqtt_broker( struct network_data *ctxt ) {
  esp_err_t err  = nvs_get_str( ctxt->nvs, MQTT_BROKER, NULL, &ctxt->mqtt_broker_len );
  if( err==ESP_OK ) {
    if( ctxt->mqtt_broker_len!=0 ) {
      ctxt->mqtt_broker = malloc( ctxt->mqtt_broker_len );	// length includes '\0' terminator
      if( ctxt->mqtt_broker ) {
        nvs_get_str( ctxt->nvs, MQTT_BROKER, ctxt->mqtt_broker, &ctxt->mqtt_broker_len );
      }
    }
  }
}

static void net_set_mqtt_broker( struct network_data *ctxt, char *new_broker ) {
  if( !ctxt->mqtt_broker || strcmp( ctxt->mqtt_broker, new_broker ) ) {
    size_t len = strlen( new_broker );
    if( len >= ctxt->mqtt_broker_len ) {
      if( ctxt->mqtt_broker ) free( ctxt->mqtt_broker );
      ctxt->mqtt_broker_len = len+1; // Need space for '\0' terminator
      ctxt->mqtt_broker = malloc( ctxt->mqtt_broker_len );
    }

    if( len>0 && ctxt->mqtt_broker ) {
        strcpy( ctxt->mqtt_broker, new_broker );
        nvs_set_str( ctxt->nvs, MQTT_BROKER, ctxt->mqtt_broker );
    }
  }
}

void NET_set_mqtt_broker( char *new_broker ) {
  struct network_data *ctxt = network_ctxt();
  if( ctxt && new_broker )
    net_set_mqtt_broker( ctxt, new_broker );
}

char const *NET_get_mqtt_broker(void){
  struct network_data *ctxt = network_ctxt();
  return ctxt->mqtt_broker;
}

/********************************************************************************
 * MQTT user
 */

static void net_get_mqtt_user( struct network_data *ctxt ) {
  esp_err_t err  = nvs_get_str( ctxt->nvs, MQTT_USER, NULL, &ctxt->mqtt_user_len );
  if( err==ESP_OK ) {
    if( ctxt->mqtt_user_len!=0 ) {
      ctxt->mqtt_user = malloc( ctxt->mqtt_user_len );	// length includes '\0' terminator
      if( ctxt->mqtt_user ) {
        nvs_get_str( ctxt->nvs, MQTT_USER, ctxt->mqtt_user, &ctxt->mqtt_user_len );
      }
    }
  }
}

static void net_set_mqtt_user( struct network_data *ctxt, char *new_user ) {
  if( !ctxt->mqtt_user || strcmp( ctxt->mqtt_user, new_user ) ) {
    size_t len = strlen( new_user );
    if( len >= ctxt->mqtt_user_len ) {
      if( ctxt->mqtt_user ) free( ctxt->mqtt_user );
      ctxt->mqtt_user_len = len+1; // Need space for '\0' terminator
      ctxt->mqtt_user = malloc( ctxt->mqtt_user_len );
    }

    if( len>0 && ctxt->mqtt_user ) {
        strcpy( ctxt->mqtt_user, new_user );
        nvs_set_str( ctxt->nvs, MQTT_USER, ctxt->mqtt_user );
    }
  }
}

void NET_set_mqtt_user( char *new_user ) {
  struct network_data *ctxt = network_ctxt();
  if( ctxt && new_user )
    net_set_mqtt_user( ctxt, new_user );
}

char const *NET_get_mqtt_user(void){
  struct network_data *ctxt = network_ctxt();
  return ctxt->mqtt_user;
}

/********************************************************************************
 * MQTT password
 */

static void net_get_mqtt_password( struct network_data *ctxt ) {
  esp_err_t err  = nvs_get_str( ctxt->nvs, MQTT_PASSWORD, NULL, &ctxt->mqtt_password_len );
  if( err==ESP_OK ) {
    if( ctxt->mqtt_password_len!=0 ) {
      ctxt->mqtt_password = malloc( ctxt->mqtt_password_len );	// length includes '\0' terminator
      if( ctxt->mqtt_password ) {
        nvs_get_str( ctxt->nvs, MQTT_PASSWORD, ctxt->mqtt_password, &ctxt->mqtt_password_len );
      }
    }
  }
}

static void net_set_mqtt_password( struct network_data *ctxt, char *new_password ) {
  if( !ctxt->mqtt_password || strcmp( ctxt->mqtt_password, new_password ) ) {
    size_t len = strlen( new_password );
    if( len >= ctxt->mqtt_password_len ) {
      if( ctxt->mqtt_password ) free( ctxt->mqtt_password );
      ctxt->mqtt_password_len = len+1; // Need space for '\0' terminator
      ctxt->mqtt_password = malloc( ctxt->mqtt_password_len );
    }

    if( len>0 && ctxt->mqtt_password ) {
        strcpy( ctxt->mqtt_password, new_password );
        nvs_set_str( ctxt->nvs, MQTT_PASSWORD, ctxt->mqtt_password );
    }
  }
}

void NET_set_mqtt_password( char *new_password ) {
  struct network_data *ctxt = network_ctxt();
  if( ctxt && new_password )
    net_set_mqtt_password( ctxt, new_password );
}

char const *NET_get_mqtt_password(void){
  struct network_data *ctxt = network_ctxt();
  return ctxt->mqtt_password;
}

/********************************************************************************
 * MQTT broker params
 */
static void net_get_mqtt_params( struct network_data *ctxt ) {
  net_get_mqtt_broker( ctxt );
  net_get_mqtt_user( ctxt );
  net_get_mqtt_password( ctxt );
}

/********************************************************************************
 * SNTP server uri
 */

static void net_get_sntp_server( struct network_data *ctxt ) {
  esp_err_t err  = nvs_get_str( ctxt->nvs, SNTP_SERVER, NULL, &ctxt->sntp_server_len );
  if( err==ESP_OK ) {
    if( ctxt->sntp_server_len!=0 ) {
      ctxt->sntp_server = malloc( ctxt->sntp_server_len );	// length includes '\0' terminator
      if( ctxt->sntp_server ) {
        nvs_get_str( ctxt->nvs, SNTP_SERVER, ctxt->sntp_server, &ctxt->sntp_server_len );
      }
    }
  }
}

static void net_set_sntp_server( struct network_data *ctxt, char *new_server ) {
  if( !ctxt->sntp_server || strcmp( ctxt->sntp_server, new_server ) ) {
    size_t len = strlen( new_server );
    if( len >= ctxt->sntp_server_len ) {
      if( ctxt->sntp_server ) free( ctxt->sntp_server );
      ctxt->sntp_server_len = len+1; // Need space for '\0' terminator
      ctxt->sntp_server = malloc( ctxt->sntp_server_len );
    }

    if( len > 0 ) {
      if( ctxt->sntp_server ) {
        strcpy( ctxt->sntp_server, new_server );
        nvs_set_str( ctxt->nvs, SNTP_SERVER, ctxt->sntp_server );
      }
    }
  }
}

void NET_set_sntp_server( char *new_server ) {
  struct network_data *ctxt = network_ctxt();
  if( ctxt && new_server )
    net_set_sntp_server( ctxt, new_server );
}

/********************************************************************************
 * Timezones
 */

static struct tz {
	char const *abbrev;
	char const *tz_str;
} const tz_list[] = {
	// List of timezones where ramses_esp is likely to be used
  { "GMT", "GMT0BST-1,M3.5.0/01,M10.5.0/02"},
  { "CET", "CET-1CEST-2,M3.5.0/02,M10.5.0/03"},
  { "EET", "EET-2EEST-3,M3.5.0/03,M10.5.0/04"},
  { "WET", "WET0WEST-1,M3.5.0/01,M10.5.0/02"},
  { "IST", "IST-1GMT0,M10.5.0/02,M3.5.0/01"},
  { NULL, NULL }
};

static void net_timezone( char const *tz ) {
  setenv("TZ", tz,1);
}

static char const *find_timezone( char const *zone ) {
  struct tz const *pTz = tz_list;
  do {
	if( !strcmp( pTz->abbrev, zone ) ) {
      zone = pTz->tz_str;
      break;
	}
    pTz++;
  } while( pTz->abbrev != NULL );

  // If we didn't find a matching abbreviation return the original string
  return zone;
}

static void net_set_timezone( struct network_data *ctxt, char *new_timezone ) {
  char const *new_tz = find_timezone( new_timezone );
  if( !ctxt->timezone || strcmp( ctxt->timezone, new_tz ) ) {
    size_t len = strlen( new_tz );
    if( len >= ctxt->timezone_len ) {
      if( ctxt->timezone ) free( ctxt->timezone );
      ctxt->timezone_len = len+1; // Need space for '\0' terminator
      ctxt->timezone = malloc( ctxt->timezone_len );
    }

    if( len > 0 ) {
      if( ctxt->timezone ) {
        strcpy( ctxt->timezone, new_tz );
        nvs_set_str( ctxt->nvs, TIMEZONE, ctxt->timezone );
        net_timezone( ctxt->timezone );
      }
    }
  }
}

static void net_get_timezone( struct network_data *ctxt ) {
  esp_err_t err  = nvs_get_str( ctxt->nvs, TIMEZONE, NULL, &ctxt->timezone_len );
  if( err==ESP_OK ) {
    if( ctxt->timezone_len!=0 ) {
      ctxt->timezone = malloc( ctxt->timezone_len );	// length includes '\0' terminator
      if( ctxt->timezone ) {
        nvs_get_str( ctxt->nvs, TIMEZONE, ctxt->timezone, &ctxt->timezone_len );
        net_timezone( ctxt->timezone );
      }
    }
  }
}

void NET_set_timezone( char *new_timezone ) {
  struct network_data *ctxt = network_ctxt();
  if( ctxt && new_timezone )
    net_set_timezone( ctxt, new_timezone );
}

void NET_show_timezones( void) {
  struct tz const *pTz = tz_list;

  printf("Supported timezones (For other zones specify full string)\n");
  do {
	printf("%-4s %s\n",pTz->abbrev, pTz->tz_str );
    pTz++;
  } while( pTz->abbrev != NULL );


}

/********************************************************************************
 * Initialisation
 */
void ramses_network_init( BaseType_t coreID ) {
  struct network_data *ctxt = network_ctxt();

  network_register();

  ESP_ERROR_CHECK( esp_netif_init() );

  esp_err_t err = nvs_open( NETWORK_NAMESPACE, NVS_READWRITE, &ctxt->nvs );
  if( err==ESP_OK ) {
    net_get_mqtt_params( ctxt );
    net_get_sntp_server( ctxt );
    net_get_timezone( ctxt );
  }

  ramses_sntp_init( coreID, ctxt->sntp_server );
  ramses_ota_init( coreID );
  ramses_wifi_init( coreID );
}
