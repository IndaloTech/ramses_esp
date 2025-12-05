/********************************************************************
 * ramses_esp
 * ramses_wifi.c
 *
 * (C) 2023 Peter Price
 *
 * WiFi Station
 *
 */
#include <string.h>

static const char *TAG = "WIFI";
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"

#include "wifi_cmd.h"
#include "ramses_wifi.h"

/* Map configuration to internal constants */
#if CONFIG_WPA3_SAE_PWE_HUNT_AND_PECK
#define SAE_MODE WPA3_SAE_PWE_HUNT_AND_PECK
#define H2E_IDENTIFIER ""
#elif CONFIG_WPA3_SAE_PWE_HASH_TO_ELEMENT
#define SAE_MODE WPA3_SAE_PWE_HASH_TO_ELEMENT
#define H2E_IDENTIFIER CONFIG_WIFI_PW_ID
#elif CONFIG_WPA3_SAE_PWE_BOTH
#define SAE_MODE WPA3_SAE_PWE_BOTH
#define H2E_IDENTIFIER CONFIG_WIFI_PW_ID
#endif

#if CONFIG_WIFI_AUTH_OPEN
#define SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_WIFI_AUTH_WEP
#define SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_WIFI_AUTH_WPA_PSK
#define SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_WIFI_AUTH_WPA2_PSK
#define SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_WIFI_AUTH_WPA_WPA2_PSK
#define SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_WIFI_AUTH_WPA3_PSK
#define SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_WIFI_AUTH_WPA2_WPA3_PSK
#define SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_WIFI_AUTH_WAPI_PSK
#define SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif

#define WIFI_STATE_LIST \
  WIFI_STATE( WIFI_IDLE,	      "Idle" ) \
  WIFI_STATE( WIFI_CREATED,       "Created" ) \
  WIFI_STATE( WIFI_STARTED,       "Started" ) \
  WIFI_STATE( WIFI_FAILED,        "Failed" ) \
  WIFI_STATE( WIFI_CONNECTED,     "Connected" ) \
  WIFI_STATE( WIFI_DISCONNECTED,  "Disconnected" ) \
  WIFI_STATE( WIFI_RESTART,       "Restart" ) \


#define WIFI_STATE( _e, _t ) _e,
enum wifi_state {
  WIFI_STATE_LIST
  WIFI_STATE_MAX
};
#undef WIFI_STATE

char const *wifi_state_text( enum wifi_state state ) {
  static char const * const state_text[WIFI_STATE_MAX] = {
    #define WIFI_STATE( _e,_t ) _t,
    WIFI_STATE_LIST
    #undef WIFI_STATE
  };

  char const *text = "Unknown";
  if( state<WIFI_STATE_MAX )
    text = state_text[state];

  return text;
}

struct wifi_data {
  BaseType_t coreID;
  TaskHandle_t   task;
  QueueHandle_t  queue;

  enum wifi_state state;
  int retry_num;

  /* FreeRTOS event group to signal when we are connected*/
  EventGroupHandle_t event_group;
  #define WIFI_CONNECTED_BIT BIT0
  #define WIFI_FAIL_BIT      BIT1

  esp_event_handler_instance_t any_id;
  esp_event_handler_instance_t got_ip;

  wifi_config_t station_config;
  bool restart;
};


static struct wifi_data *wifi_ctxt( void ) {
  static struct wifi_data wifi = {
    .state = WIFI_IDLE,
    .station_config = {
      .sta = {
        .ssid = CONFIG_WIFI_SSID,
        .password = CONFIG_WIFI_PASSWORD,
        /* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (pasword len => 8).
         * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
         * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
         * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
         */
        .threshold.authmode = SCAN_AUTH_MODE_THRESHOLD,
        .sae_pwe_h2e = SAE_MODE,
        .sae_h2e_identifier = H2E_IDENTIFIER,
      },
    },
    .restart = false,
  };

  static struct wifi_data *ctxt = NULL;
  if( !ctxt ){
    ctxt = &wifi;
    // Runtime Initialisation?
  }

  return ctxt;
}

/********************************************************************
 * utilities
 */
bool wifi_is_connected(void) {
  struct wifi_data *ctxt = wifi_ctxt();
  return( ctxt && ctxt->state==WIFI_CONNECTED );
}

/********************************************************************
 * Task events
 */
#define SSID_LEN     32
#define PASSWORD_LEN 64
struct wifi_msg {
  union wifi_msg_param {
    char ssid[ SSID_LEN ];
    char password[ PASSWORD_LEN ];
  } param;
  void (* msgFunc)( struct wifi_data *ctxt, union wifi_msg_param *param );
};

static BaseType_t wifi_msg_post( struct wifi_msg * msg ) {
  BaseType_t res = pdFALSE;

  struct wifi_data *ctxt = wifi_ctxt();
  if( ctxt && ctxt->queue )
    res = xQueueSend( ctxt->queue, msg, portTICK_PERIOD_MS );

  return res;
}

/********************************************************************/

static void _wifi_set_ssid( struct wifi_data *ctxt, union wifi_msg_param *param ) {
  memcpy( ctxt->station_config.sta.ssid, param->ssid, sizeof(ctxt->station_config.sta.ssid) );
}

void wifi_set_ssid( char *ssid ) {
  struct wifi_msg msg = {
	.msgFunc = _wifi_set_ssid,
  } ;
  strncpy( msg.param.ssid, ssid, sizeof(msg.param.ssid) );
  wifi_msg_post( &msg );
}

/********************************************************************/

static void _wifi_set_password( struct wifi_data *ctxt, union wifi_msg_param *param ) {
  memcpy( ctxt->station_config.sta.password, param->password, sizeof(ctxt->station_config.sta.password) );
}

void wifi_set_password( char *password ) {
  struct wifi_msg msg = {
	.msgFunc = _wifi_set_password,
  } ;
  strncpy( msg.param.password, password, sizeof(msg.param.password) );
  wifi_msg_post( &msg );
}

/********************************************************************/
static void _wifi_restart( struct wifi_data *ctxt, union wifi_msg_param *param ) {
  ctxt->restart = true;
}

void wifi_restart( void ) {
  struct wifi_msg msg = {
	.msgFunc = _wifi_restart,
  } ;

  wifi_msg_post( &msg );
}

/********************************************************************/
/*
static void _wifi_status( struct wifi_data *ctxt, union wifi_msg_param *param ) {

}

void wifi_status( void ) {
  struct wifi_msg msg = {
	.msgFunc = _wifi_status,
  } ;

  wifi_msg_post( &msg );
}
*/
/********************************************************************/

static void wifi_event_handler( void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  struct wifi_data *ctxt = arg;

  if( event_base==WIFI_EVENT && event_id==WIFI_EVENT_STA_START ) {
     esp_wifi_connect();
  } else if( event_base==WIFI_EVENT && event_id==WIFI_EVENT_STA_DISCONNECTED ) {
    xEventGroupSetBits( ctxt->event_group, WIFI_FAIL_BIT );
  } else if( event_base==IP_EVENT && event_id==IP_EVENT_STA_GOT_IP ) {
    ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
    ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    ctxt->retry_num = 0;
    xEventGroupSetBits( ctxt->event_group, WIFI_CONNECTED_BIT );
  }
}

static void wifi_check_station( struct wifi_data *ctxt ) {
  wifi_config_t config;
  union wifi_msg_param param;

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  cfg.wifi_task_core_id = ctxt->coreID;
  ESP_ERROR_CHECK( esp_wifi_init(&cfg) );

  ESP_ERROR_CHECK( esp_wifi_get_config(ESP_IF_WIFI_STA, &config ) );
  if( config.sta.ssid[0]!='\0' ) {
	memcpy( param.ssid, config.sta.ssid, sizeof(param.ssid) );
	_wifi_set_ssid( ctxt, &param );
    ESP_LOGI( TAG, "SSID retrived from NVS");
  }
  if( config.sta.password[0]!='\0' ) {
	memcpy( param.password, config.sta.password, sizeof(param.password) );
	_wifi_set_password( ctxt, &param );
    ESP_LOGI( TAG, "PASSWORD retrieved from NVS");
  }
}

static void wifi_create( struct wifi_data *ctxt ) {
  ctxt->event_group = xEventGroupCreate();
  esp_netif_create_default_wifi_sta();

  ESP_ERROR_CHECK( esp_event_handler_instance_register(WIFI_EVENT,ESP_EVENT_ANY_ID,  &wifi_event_handler,ctxt, &ctxt->any_id) );
  ESP_ERROR_CHECK( esp_event_handler_instance_register(IP_EVENT,IP_EVENT_STA_GOT_IP, &wifi_event_handler,ctxt, &ctxt->got_ip) );
}

static void wifi_start( struct wifi_data *ctxt ) {
  ctxt->retry_num = 0;

  ESP_ERROR_CHECK( esp_wifi_set_mode( WIFI_MODE_STA ) );
  ESP_ERROR_CHECK( esp_wifi_set_config( WIFI_IF_STA, &ctxt->station_config ) );
  ESP_ERROR_CHECK( esp_wifi_start() );
}

static void wifi_state_machine( struct wifi_data *ctxt ) {
  EventBits_t bits = 0;
  if( ctxt->state >= WIFI_CREATED )
    bits = xEventGroupGetBits( ctxt->event_group );

  switch( ctxt->state ){
  case WIFI_IDLE:
    if( ctxt->station_config.sta.ssid[0] != '\0' ) {
      wifi_create(ctxt);
      ctxt->state = WIFI_CREATED;
      ESP_LOGI( TAG, "WiFi created");
	}
    break;

  case WIFI_CREATED:
    printf("# Attempting to connect to SSID:%s\n", (char *)ctxt->station_config.sta.ssid );
    wifi_start( ctxt );
    ctxt->state = WIFI_STARTED;
    ESP_LOGI( TAG, "WiFi started");
    break;

  case WIFI_STARTED:
    if( bits & WIFI_CONNECTED_BIT ) {
      ESP_LOGI( TAG, "connected to ap SSID:%s", (char *)ctxt->station_config.sta.ssid );
      printf("# Connected to SSID:%s\n", (char *)ctxt->station_config.sta.ssid );
      ctxt->state = WIFI_CONNECTED;
      xEventGroupClearBits( ctxt->event_group, WIFI_CONNECTED_BIT );
    } else if( bits & WIFI_FAIL_BIT ) {
      if( ctxt->retry_num < CONFIG_WIFI_MAXIMUM_RETRY ) {
        esp_wifi_connect();
        ctxt->retry_num++;
        ESP_LOGI( TAG, "retry to connect to the AP" );
      } else {
        ESP_LOGI( TAG, "Failed to connect to SSID:%s",(char *)ctxt->station_config.sta.ssid );
        printf("# Failed to connect to SSID:%s\n", (char *)ctxt->station_config.sta.ssid );
        ctxt->state = WIFI_FAILED;
      }
      xEventGroupClearBits( ctxt->event_group, WIFI_FAIL_BIT );
    }
    break;

  case WIFI_FAILED:
    ctxt->state = WIFI_RESTART;
    break;

  case WIFI_CONNECTED:
    if( bits & WIFI_FAIL_BIT ) {
      ctxt->state = WIFI_DISCONNECTED;
    } else if( ctxt->restart ) {
      ctxt->state = WIFI_RESTART;
    }
    break;

  case WIFI_DISCONNECTED:
    ESP_LOGI( TAG ,"disconnected from SSID:%s", (char *)ctxt->station_config.sta.ssid );
    printf("# Disconnected from SSID:%s\n", (char *)ctxt->station_config.sta.ssid );
    ctxt->state = WIFI_RESTART;
    break;

  case WIFI_RESTART:
    esp_wifi_stop();
    ctxt->restart = false;
    ctxt->state = WIFI_CREATED;
    break;

  default:
    break;
  };
}

static void Wifi( void *param ) {
  struct wifi_data *ctxt = param;

  ESP_LOGI( TAG, "Task Started");

  ctxt->queue = xQueueCreate( 10, sizeof( struct wifi_msg ) );

  wifi_check_station(ctxt);

  do {
	wifi_state_machine( ctxt );
    struct wifi_msg msg;
    BaseType_t res = xQueueReceive( ctxt->queue, &msg, portTICK_PERIOD_MS);
    if( res ){
      if( msg.msgFunc )
        ( msg.msgFunc )( ctxt, &msg.param );
    }
  } while(1);
}

WIFI_HNDL ramses_wifi_init( BaseType_t coreID ) {
  struct wifi_data *ctxt= wifi_ctxt();
  ctxt->coreID = coreID;

  esp_log_level_set(TAG, CONFIG_WIFI_LOG_LEVEL );

  wifi_register();

  xTaskCreatePinnedToCore( Wifi, "WiFi", 4096, ctxt, 10, &ctxt->task, ctxt->coreID );

  return ctxt;
}
