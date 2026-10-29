/********************************************************************
 * ramses_esp
 * ramses_ota.c
 *
 * (C) 2023 Peter Price
 *
 * Over The Air upgrade
 *
 */

#include <string.h>

static const char *TAG = "OTA";
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_crt_bundle.h"
#include "esp_ota_ops.h"

#include "ota_cmd.h"
#include "ramses_ota.h"

struct ota_data {
  BaseType_t coreID;

  char url[176];
  char version[16];
  char filename[32];
  uint8_t force;

  esp_https_ota_handle_t ota_handle;
};

struct ota_data *ota_ctxt(void) {
  static struct ota_data Ctxt = {
    .url = CONFIG_OTA_URL_SERVER,
	.version = CONFIG_OTA_VERSION,
	.filename = CONFIG_OTA_FILENAME,

	.ota_handle = NULL,
  };
  static struct ota_data *ctxt = NULL;

  if( !ctxt ) {
	ctxt = &Ctxt;
	// Runtime initialisation?
  }

  return ctxt;
}

esp_err_t _http_event_handler( esp_http_client_event_t *evt ) {
  switch (evt->event_id) {
  case HTTP_EVENT_ERROR:
    ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
    break;
  case HTTP_EVENT_ON_CONNECTED:
    ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
    break;
  case HTTP_EVENT_HEADER_SENT:
    ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
    break;
  case HTTP_EVENT_ON_STATUS_CODE:
    ESP_LOGD(TAG, "HTTP_EVENT_ON_STATUS_CODE");
    break;
  case HTTP_EVENT_ON_HEADER:
    ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
    break;
  case HTTP_EVENT_ON_HEADERS_COMPLETE:
    ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
    break;
  case HTTP_EVENT_ON_DATA:
    ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
    break;
  case HTTP_EVENT_ON_FINISH:
    ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
    break;
  case HTTP_EVENT_DISCONNECTED:
    ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
    break;
  case HTTP_EVENT_REDIRECT:
    ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
    break;
  }

  return ESP_OK;
}

static esp_err_t ota_begin( esp_https_ota_config_t  *config, struct ota_data *ctxt ) {
  esp_err_t err = esp_https_ota_begin( config, &ctxt->ota_handle );
  if( err!=ESP_OK ) {
    ESP_LOGE(TAG, "OTA Begin failed");
  }

  return err;
}

static esp_err_t ota_check_file_hdr( struct ota_data *ctxt ) {
  esp_app_desc_t app_info;
  esp_err_t err = esp_https_ota_get_img_desc( ctxt->ota_handle, &app_info );
  if( err != ESP_OK ) {
    ESP_LOGE(TAG, "esp_https_ota_read_img_desc failed");
  } else {
    esp_app_desc_t running_app_info;
    const esp_partition_t *running = esp_ota_get_running_partition();
    err = esp_ota_get_partition_description( running, &running_app_info );
	if( err==ESP_OK ) {
	  ESP_LOGI(TAG, "Running firmware version: %s", running_app_info.version);
	  printf("# OTA: Running firmware version: %s\n", running_app_info.version);
	}

	if( err==ESP_OK ) {
	  if( memcmp( app_info.version, running_app_info.version, sizeof(app_info.version) ) == 0 ) {
		if( ctxt->force ) {
          ESP_LOGW(TAG, "Current running version is the same as new. Update being forced.");
          printf("# OTA: Force update to %s\n",app_info.version);
		} else {
          ESP_LOGW(TAG, "Current running version is the same as new. We will not continue the update.");
          printf("# OTA: Target version is same as current version - no update\n");
	      err=ESP_FAIL;
		}
	  } else {
        ESP_LOGI(TAG, "Downloading firmware version %s",app_info.version);
        printf("# OTA: downloading firmware version %s\n",app_info.version);
	  }
	}
  }

  return err;
}

static esp_err_t ota_download_file( struct ota_data *ctxt ) {
  esp_err_t err;

  while (1) {
    err = esp_https_ota_perform( ctxt->ota_handle );
    if( err!=ESP_ERR_HTTPS_OTA_IN_PROGRESS ) {
      break;
    }
  }

  return err;
}

static esp_err_t ota_finish( struct ota_data *ctxt ) {
  esp_err_t err = ESP_FAIL;

  if( esp_https_ota_is_complete_data_received(ctxt->ota_handle) != true ) {
    // the OTA image was not completely received and user can customise the response to this situation.
    ESP_LOGE(TAG, "Complete data was not received.");
    printf("# OTA: complete data was not received\n");
  } else {
    err = esp_https_ota_finish( ctxt->ota_handle );
    if( err!=ESP_OK ) {
      if( err == ESP_ERR_OTA_VALIDATE_FAILED ) {
        ESP_LOGE(TAG, "Image validation failed, image is corrupted");
        printf("# OTA: Image corrupted\n");
      } else {
        printf("# OTA: upgrde failed\n");
        ESP_LOGE(TAG, "upgrade failed 0x%x", err);
      }
    }
  }
  return err;
}

static void Ota( void *param ) {
  esp_err_t err;
  struct ota_data *ctxt = param;

  char url[256];
  esp_http_client_config_t config = {
    .url = url,
    .crt_bundle_attach = esp_crt_bundle_attach,
    .event_handler = _http_event_handler,
    .keep_alive_enable = true,
	.buffer_size_tx = 4096,
  };
  esp_https_ota_config_t ota_config = {
    .http_config = &config,
  };

  ESP_LOGI( TAG, "Task Started");
  if( !strcmp( ctxt->version,"latest" ) ) {
    sprintf( url, "%s/%s/download/%s", ctxt->url,ctxt->version,ctxt->filename);
  } else {
	ctxt->force = 1;   // Always update with a specific version
    sprintf( url, "%s/download/%s/%s", ctxt->url,ctxt->version,ctxt->filename);
  }
  ESP_LOGI(TAG, "URL <%s>",url );

  err = ota_begin( &ota_config, ctxt );
  if( err==ESP_OK ) {
                      err = ota_check_file_hdr(ctxt);
    if( err==ESP_OK ) err = ota_download_file(ctxt);
    if( err==ESP_OK ) err = ota_finish( ctxt );
    if( err!=ESP_OK ) esp_https_ota_abort( ctxt->ota_handle );
  }

  if( err==ESP_OK ) {
    printf("# OTA: Upgrade complete. Rebooting ...\n");
    ESP_LOGI(TAG, "ESP_HTTPS_OTA upgrade successful. Rebooting ...");
    vTaskDelay( 1000 / portTICK_PERIOD_MS );
    esp_restart();
  } else {
    vTaskDelete(NULL);
  }
}

/********************************************************************
 * External commands
 */
void ota_set_url( const char * url ){
  struct ota_data *ctxt = ota_ctxt();
  strncpy( ctxt->url, url, sizeof(ctxt->url)-1 );
}

void ota_set_version( const char * version ){
  struct ota_data *ctxt = ota_ctxt();
  strncpy( ctxt->version, version, sizeof(ctxt->version)-1 );
}

void ota_set_filename( const char * filename ){
  struct ota_data *ctxt = ota_ctxt();
  strncpy( ctxt->filename, filename, sizeof(ctxt->filename)-1 );
}

void ota_start( uint8_t force ) {
  struct ota_data *ctxt = ota_ctxt();
  ctxt->force = force;
  xTaskCreatePinnedToCore( Ota, "OTA", 8*1024, ctxt, 5, NULL, ctxt->coreID );
}

/********************************************************************/

void ramses_ota_init( BaseType_t coreID ) {
  struct ota_data *ctxt = ota_ctxt();
  ctxt->coreID = coreID;

  esp_log_level_set(TAG, CONFIG_OTA_LOG_LEVEL );

  ota_register();
}
