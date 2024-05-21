/********************************************************************
 * ramses_esp
 * radio.c
 *
 * (C) 2023 Peter Price
 *
 * Radio interface
 *
 */
static const char *TAG = "RADIO";
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "frame.h"
#include "message.h"

#include "radio.h"

struct radio_data {
  BaseType_t coreID;
  TaskHandle_t task;
};

/*************************************************************************
 * TASK
 */

void Radio_Task(void *arg)
{
  ESP_LOGI( TAG, "Task Started");

  frame_init();
  msg_init();

  while(1){
    frame_work();
    msg_work();
  }
}


/*************************************************************************
 * External API
 */

static struct radio_data *radio_context( void ) {
  static struct radio_data radio;

  struct radio_data *ctxt = NULL;
  if( !ctxt ){
    ctxt = &radio;
    // Initialisation?
  }

  return ctxt;
}

void Radio_init( BaseType_t coreID ) {
  struct radio_data *ctxt = radio_context();

  ctxt->coreID = coreID;

  xTaskCreatePinnedToCore( Radio_Task,  "Radio",  4096, ctxt, 20, &ctxt->task, ctxt->coreID );
}
