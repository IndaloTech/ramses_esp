/********************************************************************
 * ramses_esp
 * frame.c
 *
 * (C) 2023 Peter Price
 *
 * Frame handler
 *
 * Detects the presence of RX messages in UART data
 * Extracts RX frames and removes manchester encoding of message data bytes
 * Acquires the RSSI value for RX messages
 *
 * Manages the transition to TX mode when required.
 *
 * Builds Raw frame adding manchester encoding of message data
 *
 */
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include <driver/uart.h>

static const char * TAG = "FRM";
#include "esp_log.h"
#include "esp_err.h"

#include "ramses_led.h"
#include "cc1101.h"
#include "uart.h"
#include "frame.h"
#include "message.h"

#include "ramses_debug.h"
#define DEBUG_FRAME(_i)   //do{if(_i)DEBUG1_ON;else DEBUG1_OFF;}while(0)

static uint64_t frm_time(void) {
  uint64_t ts;
  struct timeval tv;
  gettimeofday( &tv, NULL );
  ts = tv.tv_sec * 1000LL + (tv.tv_usec / 1000LL);
  return ts;
}

static uint64_t last_frm;
static uint64_t last_cal;

/***********************************************************************************
** Frame state machine
*/

enum frame_states {
  FRM_OFF,
  FRM_IDLE,
  FRM_RX,
  FRM_TX,
};

static struct frame_state {
  uint8_t state;
} frame;

static void frame_reset(void) {
  memset( &frame, 0, sizeof(frame) );
}

/*******************************************************
* Manchester Encoding
*
* The [Evo Message] is encoded in the bitstream with a
* Manchester encoding.  This is the only part of the
* complete packet encoded in this way so we cannot
* use the built in function of the CC1101
*
* While the bitstream is interpreted as a Big-endian stream
* the manchester codes inserted in the stream are little-endian
*
* The Manchester data here is designed to correspond with the
* Big-endian byte stream seen in the bitstream.
*
********
* NOTE *
********
* The manchester decode process converts the data from
*     8 bit little-endian to 4 bit big-endian
* The manchester encode process converts the data from
*     4 bit big-endian to 8 bit little-endian
*
* Since only a small subset of 8-bit values are actually allowed in
* the bitstream rogue values can be used to identify some errors in
* the bitstream.
*
*/

// Convert big-endian 4 bits to little-endian byte
static uint8_t const man_encode[16] = {
  0xAA, 0xA9, 0xA6, 0xA5,  0x9A, 0x99, 0x96, 0x95,
  0x6A, 0x69, 0x66, 0x65,  0x5A, 0x59, 0x56, 0x55
};
#define MAN_ENCODE(_i) man_encode[_i]

// Convert little-endian 4 bits to 2-bit big endian
static uint8_t const man_decode[16] = {
  0xF, 0xF, 0xF, 0xF, 0xF, 0x3, 0x2, 0xF,
  0xF, 0x1, 0x0, 0xF, 0xF, 0xF, 0xF, 0xF
};
#define MAN_DECODE(_i) man_decode[_i]

static inline int manchester_code_valid( uint8_t code ) {
 return ( MAN_DECODE( (code>>4)&0xF )!=0xF ) && ( MAN_DECODE( (code   )&0xF )!=0xF ) ;
}

static inline uint8_t manchester_decode( uint8_t byte ) {
  uint8_t decoded;

  decoded  = MAN_DECODE( ( byte    ) & 0xF );
  decoded |= MAN_DECODE( ( byte>>4 ) & 0xF )<<2;

  return decoded;
}

static inline uint8_t manchester_encode( uint8_t value ) {
  return MAN_ENCODE(value & 0xF )
  ;
}

/*******************************************************
** RAMSES Frame
** A frame consists of <training><header><message><trailer><training>
** <training>  is a pattern of alternating 0 and 1 bits <0x55>
**   <header>  is a fixed sequence of bytes that identify the packet as a RAMSES packet
**             a 16 bit synch word <FF><00> followed by a 3 byte magic pattern <33><55><53>
**  <message>  variable length RAMSES message.  Message data is Manchester encoded.
**  <trailer>  is a single byte that marks end of packet
**
** The <header> and <trailer> are made from bytes that are not valid Manchester code values
*/
static uint8_t ramses_synch[] = { 0xFF, 0x00 };
static uint8_t ramses_hdr[]   = { 0x33, 0x55, 0x53 };
static uint8_t ramses_tlr[]   = { 0x35 };

// New frame start pattern
static uint32_t syncWord;

/*******************************************************
* RX Frame processing
*/

enum frame_rx_states {
  FRM_RX_OFF,
  FRM_RX_IDLE,
  FRM_RX_SYNCH,
  FRM_RX_MESSAGE,
  FRM_RX_DONE,
  FRM_RX_ABORT
};

static struct rx_frame {
  enum frame_rx_states state;

  uint8_t nBytes;
  uint8_t nRaw;
  uint8_t *raw;

  uint32_t syncBuffer;

  uint8_t count;
  uint8_t msgErr;
  uint8_t msgByte;
} rxFrm;

static void frame_rx_reset(void) {
  memset( &rxFrm, 0, sizeof(rxFrm) );
}

void frame_rx_byte( uint8_t b ) {
  switch( rxFrm.state ) {
  case FRM_RX_OFF:
	break;

  case FRM_RX_IDLE:
  case FRM_RX_SYNCH: // wait for the <header>
    rxFrm.syncBuffer <<= 8;
    rxFrm.syncBuffer |= b;
    if( rxFrm.syncBuffer==syncWord ) {
      ESP_LOGI( TAG, "SYNCH" );
      rxFrm.raw = msg_rx_start();
      if( rxFrm.raw ) {
        rxFrm.nRaw = rxFrm.raw[0];
	    rxFrm.state  = FRM_RX_MESSAGE;
        DEBUG_FRAME(1);
        led_on(LED_RX);
	  }
	}
    break;

  case FRM_RX_MESSAGE:
    if( b == ramses_tlr[0] ) {
	  rxFrm.state = FRM_RX_DONE;
      ESP_LOGI( TAG, "DONE raw=%d msg=%d",rxFrm.nBytes, 1 );
    } else {
      ESP_LOGD( TAG, "raw[%d]=%02x",rxFrm.nBytes, b );
      rxFrm.raw[rxFrm.nBytes++] = b;

      if( !manchester_code_valid( b ) ) {
        rxFrm.state = FRM_RX_ABORT;
        rxFrm.msgErr = MSG_MANC_ERR;
        ESP_LOGE( TAG, "raw[%d]=%02x (MC)",rxFrm.nBytes-1, b );
      } else {
        rxFrm.msgByte <<= 4;
        rxFrm.msgByte |= manchester_decode( b );
        rxFrm.count = 1- rxFrm.count;

        if( !rxFrm.count ) {
          rxFrm.msgErr = msg_rx_byte( rxFrm.msgByte );
          if( rxFrm.msgErr != MSG_OK ) {
            ESP_LOGE( TAG, "raw[%d]=%02x (MSG)",rxFrm.nBytes-1, b );
            rxFrm.state = FRM_RX_ABORT;
          }
        }
      }

      // Protect raw data buffer
      if( rxFrm.nBytes >= rxFrm.nRaw ) {
        rxFrm.state = FRM_RX_ABORT;
        rxFrm.msgErr = MSG_OVERRUN_ERR;
      }
    }
    break;

  case FRM_RX_DONE:
  case FRM_RX_ABORT:
    break;
  }
}

static void frame_rx_done(void) {
  // Reset rxFrm as quickly as possible after collision can pick up new frame header
  uint8_t nBytes = rxFrm.nBytes;
  uint8_t msgErr = rxFrm.msgErr;
  uint8_t rssi;

  DEBUG_FRAME(0);
  led_off(LED_RX);

  frame_rx_reset();

  DEBUG_FRAME(1);
  led_on(LED_RX);

  // Now tell message about the end of frame
  rssi = cc_read_rssi();
  msg_rx_rssi( rssi );
  msg_rx_end(nBytes,msgErr);

  last_frm = frm_time();

  DEBUG_FRAME(0);
  led_off(LED_RX);
}

/***************************************************************************
****************************************************************************
** TX HW interface - cc1101 FIFO
****************************************************************************
****************************************************************************/

/*****************************************************************
* NOTE: The following shift_register structure is sensitive to
*       the endianness used by the MCU.  It may be necessary to
*       swap the order of .bits and .data
*
* The desired behaviour is that when .reg is left shifted the
* msb of .bits becomes the lsb of .data
*/
static union shift_register {
  uint16_t reg;
  struct {
#if __BYTE_ORDER__ ==__ORDER_LITTLE_ENDIAN__
    uint8_t bits;
    uint8_t data;
#endif
#if __BYTE_ORDER__ ==__ORDER_BIG_ENDIAN__
    uint8_t data;
    uint8_t bits;
#endif
  };
} tx;
static uint8_t txBits;   // Number of valid bits in shift register

static uint8_t swap4( uint8_t in ) {
  static uint8_t out[16] = {
    0x0, 0x8, 0x4, 0xC, 0x2, 0xA, 0x6, 0xE,
	0x1, 0x9, 0x5, 0xD, 0x3, 0xB, 0x7, 0xF
  };

  return out[ in & 0xF ];
}

static uint8_t swap8( uint8_t in ) {
  uint8_t out;

  out  = swap4( in ) << 4;
  out |= swap4( in>>4 );

  return out;
}

static uint8_t tx_data(void) {
   return cc_write_fifo( tx.data );
}

static inline void insert_p(void)  { tx.data <<= 1 ; tx.data |= 0x01; }
static inline void insert_s(void)  { tx.data <<= 1 ; }
static inline void insert_ps(void) { insert_p(); insert_s(); }
static inline void send( uint8_t n ) { tx.reg <<= n ; }

static uint8_t tx_byte( uint8_t byte ) { // convert byte to octets
  uint8_t space = 15;

  tx.bits = swap8(byte);

  // For each 4 bytes of data we send 5 octets of bitstream
  // so there is one case which generates two octets
  switch( txBits )
  {
  case 0: insert_ps(); send(6); space = tx_data(); send(2); txBits=2; break;
  case 2: insert_ps(); send(4); space = tx_data(); send(4); txBits=4; break;
  case 4: insert_ps(); send(2); space = tx_data(); send(6); // Fall through
  case 6: insert_ps();          space = tx_data();          txBits=8; break;
  case 8:              send(8); space = tx_data();          txBits=0; break;
  }

  return space;
}

static void tx_flush( void ) {
  // flush outstanding bits
  if( txBits ) {
    send(8-txBits);
    tx_data();
  }

  // Leave in SPACE condition
  tx.data = 0xFF;
  tx_data();
}

//-----------------------------------------------------------------
// TX FIFO

static QueueHandle_t tx_isr_queue;

static enum tx_fifo_state {
  TX_FIFO_FILL,
  TX_FIFO_WAIT
} tx_state;

static void tx_fifo_stop(void) {
  gpio_isr_handler_remove( CONFIG_CC_GDO0_GPIO );
  xQueueReset( tx_isr_queue );
}

static void tx_fifo_wait(void) {
  uint8_t data;
  tx_fifo_stop();
  frame_tx_byte( &data );
}

static uint8_t tx_fifo_send_block(void) {
  uint8_t done;

  uint8_t count;
  uint8_t block = 4;

  do {
    uint8_t data;
    done = frame_tx_byte( &data );
    count = tx_byte( data );
    block--;
  } while( block && !done && count>4 );

  return done;
}

static void tx_fifo_prime( void ) {
  // Not clear why but have to send a zero byte to start TX correctly
  cc_write_fifo( 0x00 );

  // Now send a BREAK condition
  cc_write_fifo( 0xFF );
  cc_write_fifo( 0x00 );
  cc_write_fifo( 0x00 );

  // So we can see an interrupt when it falls below threshold
  // send sufficient data to fill FIFO above threshold
  txBits = 0;
  while( !gpio_get_level( CONFIG_CC_GDO0_GPIO ) )
    tx_fifo_send_block();
}

static void tx_fifo_fill(void) {
  uint8_t done = tx_fifo_send_block();

  if( done ) {
    tx_flush();
    cc_fifo_end();
    tx_state = TX_FIFO_WAIT;

    // Switch to rising edge to detect FIFO empty
    gpio_set_intr_type( CONFIG_CC_GDO0_GPIO, GPIO_INTR_POSEDGE );
    return;
   }

  if( !gpio_get_level( CONFIG_CC_GDO0_GPIO ) ) {
    xQueueSend( tx_isr_queue, NULL, 0 );
    ESP_LOGW( TAG, "TX refill queued follow-up event" );
    printf("# FRAME: TX refill queued follow-up event\n");
  }
}

//---------------------------------------------------------------------------------

static void IRAM_ATTR GDO0_ISR(void *args) {
  gpio_intr_disable( CONFIG_CC_GDO0_GPIO );
  xQueueSendFromISR( tx_isr_queue, NULL, NULL );
}

static void tx_fifo_init(void) {
  gpio_reset_pin( CONFIG_CC_GDO0_GPIO );	// Disconnect GDO0 from UART TX
  gpio_set_direction( CONFIG_CC_GDO0_GPIO, GPIO_MODE_INPUT );
  gpio_pulldown_en( CONFIG_CC_GDO0_GPIO );
  gpio_pullup_dis( CONFIG_CC_GDO0_GPIO );

  tx_isr_queue = xQueueCreate( 32, 0 );
  gpio_install_isr_service(0);
}

static void tx_fifo_start(void) {
  xQueueReset( tx_isr_queue );

  gpio_intr_disable( CONFIG_CC_GDO0_GPIO );
  // Falling edge for FIFO low
  gpio_set_intr_type( CONFIG_CC_GDO0_GPIO, GPIO_INTR_NEGEDGE );
  gpio_isr_handler_add( CONFIG_CC_GDO0_GPIO, GDO0_ISR,  NULL );
  gpio_intr_enable( CONFIG_CC_GDO0_GPIO );

  tx_fifo_prime();
  tx_state = TX_FIFO_FILL;

  int gdo0 = gpio_get_level( CONFIG_CC_GDO0_GPIO );
  if( !gdo0 ) {
    xQueueSend( tx_isr_queue, NULL, 0 );
    printf("# FRAME: tx start: synthetic refill event queued\n");
  }
}

static void tx_fifo_work(void) {
  if( xQueueReceive( tx_isr_queue, NULL, 0 )){
    DEBUG_FRAME(0);
    led_off(LED_TX);
    switch(tx_state) {
      case TX_FIFO_FILL:  tx_fifo_fill();  break;
      case TX_FIFO_WAIT:  tx_fifo_wait();  break;
    }
    gpio_intr_enable( CONFIG_CC_GDO0_GPIO );
    DEBUG_FRAME(1);
    led_on(LED_TX);
  }
}

/***********************************************************************************
** TX FRAME processing
**
** We must provide the following data to the HW
**   <prefix><message><suffix>
**
**   <prefix>  = <preamble><sync word><header>
**   <message> = < manchester encoded pairs of bytes >
**   <suffix>  = <trailer><training>
**
*/

enum frame_tx_states {
  FRM_TX_OFF,
  FRM_TX_READY,
  FRM_TX_IDLE,
  FRM_TX_PREFIX,
  FRM_TX_MESSAGE,
  FRM_TX_SUFFIX,
  FRM_TX_DONE
};

static struct frame_tx {
  uint8_t state;

  uint8_t nBytes;
  uint8_t nRaw;
  uint8_t *raw;

  uint8_t count;
  uint8_t msgByte;
} txFrm;

static void frame_tx_reset(void) {
  memset( &txFrm, 0, sizeof(txFrm) );
}

static uint8_t tx_prefix[] = {
  0x55, 0x55, 0x55, 0x55, 0x55,   // Pre-amble
  0xFF, 0x00,                     // Sync Word
  0x33, 0x55, 0x53                // Header
};

static uint8_t tx_suffix[] = {
  0x35,                           // Trailer
  0x55,                           // Training
};

void frame_tx_start( uint8_t *raw, uint8_t nRaw ) {
  uint8_t i, done, byte;

  // Encode raw frame
  for( i=0 ; i<nRaw+1 ; i+=2 ) {
  	byte = msg_tx_byte(&done);
	if( done ) break;
	
	raw[ i   ] = manchester_encode( byte >> 4 );
	raw[ i+1 ] = manchester_encode( byte      );
  }

  txFrm.nBytes = i;
  txFrm.raw = raw;
  txFrm.nRaw = nRaw;
	
  txFrm.state = FRM_TX_READY;
}

uint8_t frame_tx_byte(uint8_t *byte) {
  uint8_t done = 0;

  switch( txFrm.state ) {
  case FRM_TX_IDLE:
    txFrm.state = FRM_TX_PREFIX;
    // Fall through

  case FRM_TX_PREFIX:
    if( txFrm.count < sizeof(tx_prefix) ) {
      (*byte) = tx_prefix[txFrm.count++];
      break;
    }
    txFrm.count = 0;
    txFrm.state = FRM_TX_MESSAGE;
    // Fall through

  case FRM_TX_MESSAGE:
    if( txFrm.count < txFrm.nBytes ) {
      (*byte) = txFrm.raw[ txFrm.count++ ];
	  break;
    } 

    msg_tx_end( txFrm.nBytes );

    txFrm.count = 0;
    txFrm.state = FRM_TX_SUFFIX;
    // Fall through

  case FRM_TX_SUFFIX:
    if( txFrm.count < sizeof(tx_suffix) ) {
      (*byte) = tx_suffix[txFrm.count++];
      if( txFrm.count == sizeof(tx_suffix) )
        done = 1;
      break;
    }
    last_frm = frm_time();

    txFrm.count = 0;
    txFrm.state = FRM_TX_DONE;
    // Fall through

  case FRM_TX_DONE:
  	done = 1;
    break;
  }

  return done;
}

static void frame_tx_done(void) {
  msg_tx_done();
  frame_tx_reset();
}

/***************************************************************************
** External interface
*/

static void frame_rx_enable(void) {
  cc_enter_rx_mode();

  frame.state = FRM_RX;
  rxFrm.state = FRM_RX_IDLE;

  uart_rx_enable();

  last_cal = frm_time();
}

static void frame_tx_enable(void) {
  uart_disable();
  cc_enter_tx_mode();

  frame.state = FRM_TX;
  txFrm.state = FRM_TX_IDLE;

  // make sure we switch back to RX after TX
  rxFrm.state = FRM_RX_OFF;

  tx_fifo_start();
}

void frame_disable(void) {
  uart_disable();
  cc_enter_idle_mode();

  frame.state = FRM_OFF;
}

void frame_init(void) {
  uint8_t i;

  esp_log_level_set(TAG, CONFIG_FRM_LOG_LEVEL );

  // When trying to detect the start of a new frame we'll just look for
  // that last 32 bits of the <header>
  for( i=0 ; i<sizeof(ramses_synch) ; i++ )
    syncWord = ( syncWord<<8 ) | ramses_synch[i];
  for( i=0 ; i<sizeof(ramses_hdr) ; i++ )
    syncWord = ( syncWord<<8 ) | ramses_hdr[i];

  frame_reset();

  cc_init();
  uart_init();
  tx_fifo_init();

  frame.state = FRM_IDLE;
}


void frame_work(void) {
  uart_work();

  switch( frame.state ) {
  case FRM_IDLE:
    if( rxFrm.state==FRM_RX_OFF ) {
      frame_rx_enable();
    }
    break;

  case FRM_RX:
    if( rxFrm.state==FRM_RX_DONE ) {
      frame_rx_done();
      rxFrm.state = FRM_RX_IDLE;    // Avoid switch to IDLE and back to RX mode
      break;
    }

    if( rxFrm.state==FRM_RX_ABORT ) {
      // Force a calibration after any error
      frame_rx_done();
      frame_rx_enable();
      break;
    }

    if( rxFrm.state<FRM_RX_MESSAGE ) { // RX not active
   	  uint64_t now = frm_time();
      if( ( now - last_frm ) > CONFIG_FRM_MIN_TX_DELAY ) {
        if( txFrm.state==FRM_TX_READY ) {
          led_on(LED_TX);
          frame_tx_enable();
          break;
        }

        if ( ( now - last_cal ) > CONFIG_FRM_MAX_CAL_INTERVAL*1000 ) {
          // Force a recalibration if we haven't done one for a while
          frame_rx_enable();
          break;
        }
      }
    }

    if( rxFrm.state==FRM_RX_OFF ) {
      frame_rx_enable();
      break;
    }
    break;

  case FRM_TX:
    if( txFrm.state>=FRM_TX_DONE ) {
      led_off(LED_TX);
      frame_tx_done();
      frame_rx_enable();
      break;
    }

    tx_fifo_work();
    break;
  }

}
