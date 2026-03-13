/********************************************************************
 * ramses_esp
 * ramses-mqtt.c
 *
 * (C) 2023 Peter Price
 *
 * MQTT Client
 *
 */
#include <string.h>

static const char *TAG = "MQTT";
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_app_desc.h"

#include "mqtt_client.h"
#include "cJSON.h"

#include "cmd.h"
#include "device.h"
#include "gateway.h"
#include "ramses_wifi.h"
#include "ramses-mqtt.h"

/****************************************************
 * States
 */
#define MQTT_STATE_LIST \
  MQTT_STATE( MQTT_IDLE,	      "Idle" ) \
  MQTT_STATE( MQTT_WAIT,          "Wait" ) \
  MQTT_STATE( MQTT_START,         "Start" ) \
  MQTT_STATE( MQTT_STARTING,      "Starting" ) \
  MQTT_STATE( MQTT_CONNECTED,     "Connected" ) \
  MQTT_STATE( MQTT_ACTIVE,        "Active" ) \

#define MQTT_STATE( _e, _t ) _e,
enum mqtt_state {
  MQTT_STATE_LIST
  MQTT_STATE_MAX
};
#undef MQTT_STATE

static char const *mqtt_state_text( enum mqtt_state state ) {
  static char const * const state_text[MQTT_STATE_MAX] = {
    #define MQTT_STATE( _e,_t ) _t,
    MQTT_STATE_LIST
    #undef MQTT_STATE
  };

  char const *text = "Unknown";
  if( state<MQTT_STATE_MAX )
    text = state_text[state];

  return text;
}

struct mqtt_data {
  BaseType_t coreID;
  TaskHandle_t task;

  enum mqtt_state state;
  char root[20];
  char topic[32];

  esp_mqtt_client_config_t cfg;
  esp_mqtt_client_handle_t client;

  uint8_t info;
};

static struct mqtt_data *mqtt_ctxt( void ) {
  static struct mqtt_data mqtt = {
    .state = MQTT_IDLE,
	.root = CONFIG_MQTT_ROOT,
    .cfg = {
      .session = {
    	.last_will = {
          .msg = "offline",
          .topic = mqtt.topic,
          .qos = 1,
          .retain = 1,
    	},
		.keepalive=20,
      },
    },
	.info = 0,
  };

  static struct mqtt_data *ctxt = NULL;
  if( !ctxt ){
    ctxt = &mqtt;
    // Runtime Initialisation?
  }

  return ctxt;
}

static void log_error_if_nonzero( const char *message, int error_code ) {
  if (error_code != 0) {
    ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code );
  }
}

static bool mqtt_broker_configured( struct mqtt_data *ctxt ) {
  bool res;

  // Most recent config
  char const * uri = NET_get_mqtt_broker();
  char const * user = NET_get_mqtt_user();
  char const * password = NET_get_mqtt_password();

  if( uri ) ESP_LOGI( TAG, "broker=<%s>",uri);
  if( user ) ESP_LOGI( TAG, "user=<%s>",user);
  if( password ) ESP_LOGI( TAG, "password=<%s>",password);

  res = uri  && uri[0] != '\0';
  if( res ) {  // Have URI, are we trying to authenticate
	if( user || password ) {
	  res = user && password; // Need both
	  if( res ) res = user[0] != '\0';
	  if( res ) res = password[0] != '\0';
	}
  }

  if( res ) {
    ctxt->cfg.broker.address.uri = uri;
    ctxt->cfg.credentials.username = user;
    ctxt->cfg.credentials.authentication.password = password;
  }

  return res;
}

static void mqtt_set_state( struct mqtt_data *ctxt, enum mqtt_state newState ) {
  if( ctxt ) {
    if( ctxt->state != newState ) {
      ESP_LOGI( TAG, "state %s->%s ", mqtt_state_text(ctxt->state), mqtt_state_text(newState) );
      ctxt->state = newState;
    }
  }
}

/****************************************************
 * Information topics
 */

typedef void (*info_field)( struct mqtt_data *ctxt, char *topic );

enum info_topic {
  INFO_FW,
  INFO_VER,
  // last
  INFO_MAX
};

static void publish_firmware( struct mqtt_data *ctxt, char *topic ) {
  const esp_app_desc_t *app = esp_app_get_description();
  strcat( topic, "/firmware" );
  esp_mqtt_client_publish( ctxt->client,topic, app->project_name, 0, 1, 1);
}

static void publish_version( struct mqtt_data *ctxt, char *topic ) {
  const esp_app_desc_t *app = esp_app_get_description();
  strcat( topic, "/version" );
  esp_mqtt_client_publish( ctxt->client,topic, app->version, 0, 1, 1);
}

static void mqtt_publish_info( struct mqtt_data *ctxt ) {
  static info_field const publish[INFO_MAX] = {
    [INFO_FW]  = publish_firmware,
	[INFO_VER] = publish_version,
  };
  if( ctxt->info < INFO_MAX ) {
    char topic[64];
    sprintf( topic, "%s/info", ctxt->topic );
    publish[ctxt->info]( ctxt, topic );
    ctxt->info++;
  }
}

/*******************************************************************************
 * RX message
 */
static void mqtt_publish_rx( struct mqtt_data *ctxt, char const *ts, char const *msg ) {
  cJSON *json = cJSON_CreateObject();
  char *rx;
  char topic[64];

  char *end = strstr( msg,"\r\n");
  if( end ) end[0] = '\0';

  cJSON_AddStringToObject( json, "msg", msg );
  cJSON_AddStringToObject( json, "ts",  ts );

  rx = cJSON_Print( json );
  sprintf( topic, "%s/rx", ctxt->topic );
  esp_mqtt_client_publish( ctxt->client,topic, rx, 0, 1, 0 );

  cJSON_Delete( json );
}

void MQTT_publish_rx( char const *ts, char const *msg ) {
  struct mqtt_data *ctxt= mqtt_ctxt();
  if( ctxt->state == MQTT_ACTIVE )
    mqtt_publish_rx( ctxt, ts, msg );
}

/*******************************************************************************
 * TX message
 */
static void mqtt_subscribe_tx( struct mqtt_data *ctxt ) {
  char topic[64];
  sprintf( topic, "%s/tx", ctxt->topic );
  esp_mqtt_client_subscribe( ctxt->client, topic, 0);
}

static void mqtt_process_tx( struct mqtt_data *ctxt, char const *data, int dataLen ) {
  cJSON *json, *msg;

  ESP_LOGI( TAG, "TX:<%.*s> %s", dataLen,data,esp_log_system_timestamp() );

  json = cJSON_Parse( data );
  msg = cJSON_GetObjectItem( json, "msg" );

  ESP_LOGD( TAG, "<%s>", msg->valuestring );
  gateway_tx( msg->valuestring );

  cJSON_Delete( json );
}

/*******************************************************************************
 * General command
 */

static void mqtt_publish_cmd( struct mqtt_data *ctxt ) {
  char topic[64];
  sprintf( topic, "%s/cmd/cmd",ctxt->topic );
  esp_mqtt_client_publish( ctxt->client,topic, NULL, 0, 0, 0);
}


static void mqtt_subscribe_cmd( struct mqtt_data *ctxt ) {
  char topic[64];
  sprintf( topic, "%s/cmd/cmd", ctxt->topic );
  esp_mqtt_client_subscribe( ctxt->client, topic, 2 );
}

static void mqtt_publish_cmd_result( struct mqtt_data *ctxt, char const *cmd, esp_err_t err, int retVal ) {
  char topic[192],data[128];

  sprintf( topic, "%s/cmd/result",ctxt->topic );
  sprintf( data, "{ \"cmd\":\"%s\", \"err\":\"%s\", \"return\":%d} ",cmd, esp_err_to_name(err),retVal );
  esp_mqtt_client_publish( ctxt->client,topic, data, 0, 0, 0 );

//  mqtt_publish_cmd( ctxt ); // Clear CMD so we don't execute ita again
}

static void mqtt_publish_cmd_result_str( struct mqtt_data *ctxt, char const *cmd, esp_err_t err, char const *retVal ) {
  char topic[192],data[128];

  sprintf( topic, "%s/cmd/result",ctxt->topic );
  sprintf( data, "{ \"cmd\":\"%s\", \"err\":\"%s\", \"return\":\"%s\"} ",cmd, esp_err_to_name(err),retVal ? retVal : "" );
  esp_mqtt_client_publish( ctxt->client,topic, data, 0, 0, 0 );
}

static void mqtt_process_cmd( struct mqtt_data *ctxt, char const *data, int dataLen ) {
  if( dataLen>0 ) {
    char cmdline[128];
    sprintf( cmdline,"%.*s", dataLen,data );	// Make sure cmdline contains trailing '\0'

    // MQTT clients use !V as a gateway handshake and expect an evofw3-style string.
    if( !strcmp( cmdline, "!V" ) ) {
      const esp_app_desc_t *app = esp_app_get_description();
      char version[32];

      sprintf( version, "# evofw3 %s", app->version );
      mqtt_publish_cmd_result_str( ctxt, cmdline, ESP_OK, version );
      return;
    }

    int retVal = 0;
    esp_err_t err = cmd_run( cmdline, &retVal );

    mqtt_publish_cmd_result( ctxt, cmdline, err, retVal );
  }
}

/*******************************************************************************
 * Process subscribed topics
 */

enum topics {
  TOPIC_TX,
  TOPIC_CMD,
  TOPIC_MAX
};

static void mqtt_process_data( struct mqtt_data *ctxt, esp_mqtt_event_handle_t event ) {
  static struct topic {
	char const *topic;
	void (*func)( struct mqtt_data *ctxt, char const *data, int dataLen );
  } const topic_list[TOPIC_MAX+1] = {
    [TOPIC_TX]  = { "tx",      mqtt_process_tx },
    [TOPIC_CMD] = { "cmd",     mqtt_process_cmd },
    // List terminator
    [TOPIC_MAX] = { NULL,NULL },
  };
  struct topic const *topic = topic_list;

  size_t offset = strlen( ctxt->topic );
  char const *subtopic = event->topic + offset + 1;

  while( topic->topic != NULL ) {
    if( !strncmp( subtopic, topic->topic, strlen(topic->topic) ) ) {
      if( topic->func )
    	( topic->func )( ctxt, event->data, event->data_len );
      break;
    }
    topic++;
  }

  if( topic->topic == NULL )
    ESP_LOGI(TAG, "Unexpected event %.*s",event->topic_len-offset-1, subtopic );

}

static void mqtt_event_handler( void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data )
{
  struct mqtt_data *ctxt = handler_args;

  ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
  esp_mqtt_event_handle_t event = event_data;
//  esp_mqtt_client_handle_t client = event->client;
  switch ((esp_mqtt_event_id_t)event_id) {
  case MQTT_EVENT_CONNECTED:
    ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
    mqtt_set_state( ctxt, MQTT_CONNECTED );
    break;

  case MQTT_EVENT_DISCONNECTED:
    ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
    printf("# MQTT: Disonnected\n");
    esp_restart();
    break;

  case MQTT_EVENT_SUBSCRIBED:
    ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
    break;

  case MQTT_EVENT_UNSUBSCRIBED:
    ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
    break;

  case MQTT_EVENT_PUBLISHED:
    ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d %s", event->msg_id,esp_log_system_timestamp());
    break;

  case MQTT_EVENT_DATA:
    mqtt_process_data(ctxt, event);
	break;

  case MQTT_EVENT_ERROR:
    ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
    if( event->error_handle->error_type==MQTT_ERROR_TYPE_TCP_TRANSPORT ) {
      log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
      log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
      log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
      ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
    }
    break;

  default:
    ESP_LOGI(TAG, "Other event id:%d", event->event_id);
    break;
  }
}

static void mqtt_state_machine( struct mqtt_data *ctxt ) {

  switch( ctxt->state ){
  case MQTT_IDLE:
    if( mqtt_broker_configured( ctxt ) ) {
    	mqtt_set_state( ctxt,MQTT_WAIT );
    }
    break;

  case MQTT_WAIT:
	if( wifi_is_connected() )
      mqtt_set_state( ctxt,MQTT_START );
	break;

  case MQTT_START:
	char const *dev = device();
	if( dev[0] ) {
      sprintf( ctxt->topic,"%s/%s",ctxt->root,dev );
      ctxt->client = esp_mqtt_client_init( &ctxt->cfg );
      ESP_LOGI( TAG, "Connecting to %s",ctxt->cfg.broker.address.uri );
      printf("# MQTT: Connecting to %s\n",ctxt->cfg.broker.address.uri );
      esp_mqtt_client_register_event( ctxt->client, ESP_EVENT_ANY_ID, mqtt_event_handler, ctxt );
      esp_mqtt_client_start( ctxt->client );
      mqtt_set_state( ctxt,MQTT_STARTING );
	}
    break;

  case MQTT_STARTING:
	break;

  case MQTT_CONNECTED:
    printf("# MQTT: Connected\n");
    esp_mqtt_client_publish( ctxt->client, ctxt->topic, "online", 0, 1, 1);
    mqtt_subscribe_tx( ctxt );
    mqtt_publish_cmd( ctxt );  // Clear old CMD
    mqtt_subscribe_cmd( ctxt );
	mqtt_set_state( ctxt,MQTT_ACTIVE );
    break;

  case MQTT_ACTIVE:
    if( ctxt->info<INFO_MAX )
      mqtt_publish_info( ctxt );
    break;

  default:
    break;
  };
}

static void Mqtt( void *param ) {
  struct mqtt_data *ctxt = param;

  ESP_LOGI( TAG, "Task Started");

  do {
	mqtt_state_machine( ctxt );
    vTaskDelay( 50/portTICK_PERIOD_MS );
  } while(1);
}

MQTT_HNDL ramses_mqtt_init( BaseType_t coreID ) {
  struct mqtt_data *ctxt= mqtt_ctxt();
  ctxt->coreID = coreID;

  esp_log_level_set(TAG, CONFIG_MQTT_LOG_LEVEL );

  xTaskCreatePinnedToCore( Mqtt, "MQTT", 4096, ctxt, 10, &ctxt->task, ctxt->coreID );

  return ctxt;
}
