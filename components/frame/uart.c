/********************************************************************
 * ramses_esp
 * uart.c
 *
 * (C) 2023 Peter Price
 *
 * Radio UART interface between frame and cc1101
 * Provides an RX interface using UART data
 * Provides a TX interface using the cc1101 TX fifo
 *
 */
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include <driver/uart.h>
#include <hal/uart_hal.h>

static const char * TAG = "UART";
#include "esp_log.h"
#include "esp_err.h"

#include "uart.h"
#include "frame.h"

#include "ramses_debug.h"
#define DEBUG_UART(_i)    do{if(_i)DEBUG2_ON;else DEBUG2_OFF;}while(0)
#define DEBUG_DATA(_i)    do{if(_i)DEBUG3_ON;else DEBUG3_OFF;}while(0)

static uart_port_t const uart_num = UART_NUM_1;
static QueueHandle_t uartQ;

/*******************************************************
* HW UART interface to cc1101
*/

/****************************************************************************/
enum uart_mode {
  uart_off,
  uart_rx,
  uart_tx
};

static enum uart_mode uartMode = uart_off;
static bool uart_rx_on = false;


/*******************************************************
* UART off
*/

static void uart_work_off(void) {
}

/*******************************************************
* UART RX
*/

static void uart_rx_flush(void) {
  uart_disable_rx_intr(uart_num);
  uart_flush_input(uart_num);
  xQueueReset(uartQ);
}

//---------------------------------------------------------------------------------

static void rx_start(void) {
  uart_rx_flush();
  uart_enable_rx_intr(uart_num);
  uart_rx_on = true;
}

//---------------------------------------------------------------------------------

static void rx_stop(void) {
  uart_disable_rx_intr(uart_num);
  uart_rx_on = false;
}

//---------------------------------------------------------------------------------

static void uart_work_rx(void) {
  uart_event_t event = {0};
  // Events are wake-up hints, not an accounting of buffered bytes: the ISR can
  // drop a notification when its queue is full. Always try a read, even when
  // there was no event, so the driver's full-buffer stash can be recovered.
  bool have_event = xQueueReceive( uartQ, &event, pdMS_TO_TICKS(10) ) == pdTRUE;
  for( unsigned count=0; have_event && count<32; count++ ) {
    if( event.type == UART_FIFO_OVF || event.type == UART_BUFFER_FULL ||
        event.type == UART_FRAME_ERR || event.type == UART_PARITY_ERR ) {
      ESP_LOGW( TAG, "RX stream error %d; flushing and resynchronizing", event.type );
      frame_rx_error();
      uart_rx_flush();
      uart_enable_rx_intr(uart_num);
      return;
    }
    if( count + 1 < 32 )
      have_event = xQueueReceive( uartQ, &event, 0 ) == pdTRUE;
  }

  DEBUG_UART(1);
  // Bound each pass so continuous noise cannot starve the frame deadlines.
  for( unsigned chunk=0; chunk<4; chunk++ ) {
    uint8_t data[128];
    int n = uart_read_bytes( uart_num, data, sizeof(data), 0 );
    if( n <= 0 ) break;
    for( int i=0; i<n; i++ ) {
      DEBUG_DATA(1);
      frame_rx_byte( data[i] );
      DEBUG_DATA(0);
    }
  }
  DEBUG_UART(0);
}

/*******************************************************
* UART TX
*/

static void tx_start(void) {
  uart_rx_flush();
}

//---------------------------------------------------------------------------------

static void tx_stop(void) {
}

//---------------------------------------------------------------------------------

static void uart_work_tx(void) {
  uart_rx_flush();
}

/***************************************************************************
****************************************************************************
** External interface
****************************************************************************
****************************************************************************/

void uart_rx_enable(void) {
  tx_stop();
  rx_start();

  uartMode = uart_rx;
}

void uart_tx_enable(void) {
  rx_stop();
  tx_start();

  uartMode = uart_tx;
}

void uart_disable(void) {
  rx_stop();
  tx_stop();

  uartMode = uart_off;
}

void uart_work(void)
{
  switch( uartMode ) {
  case uart_off:  uart_work_off();  break;
  case uart_rx:   uart_work_rx();   break;
  case uart_tx:   uart_work_tx();   break;
  }
}


void uart_init(void)
{
  static uart_config_t const uart_config = {
    .baud_rate = RADIO_BAUDRATE,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .source_clk = UART_SCLK_DEFAULT
  };

  static uart_intr_config_t const uintr_cfg = {
    .intr_enable_mask = UART_INTR_RXFIFO_FULL | UART_INTR_RXFIFO_TOUT |
                        UART_INTR_RXFIFO_OVF | UART_INTR_FRAM_ERR | UART_INTR_PARITY_ERR,
    .rx_timeout_thresh = 2,
    .rxfifo_full_thresh = 16,
  };

  esp_log_level_set(TAG, CONFIG_UART_LOG_LEVEL );

  ESP_ERROR_CHECK( uart_set_pin( uart_num, CONFIG_CC_GDO0_GPIO, CONFIG_CC_GDO2_GPIO, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) );
  ESP_ERROR_CHECK( uart_param_config( uart_num, &uart_config ) );

  //Install UART driver, and get the queue.
  ESP_ERROR_CHECK( uart_driver_install( uart_num, 2048,0 , 32,&uartQ, 0 ) );

  ESP_ERROR_CHECK( uart_intr_config( uart_num, &uintr_cfg ) );

  ESP_LOGI( TAG, "initialised" );
}
