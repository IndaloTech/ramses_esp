/********************************************************************
 * ramses_esp
 * cc1101.c
 *
 * (C) 2023 Peter Price
 *
 * Hardware interface to TI CC1101 radio chip
 *
 */
#include <driver/spi_master.h>
#include <driver/gpio.h>
#include <string.h>
#include "esp_timer.h"
#include "esp_rom_sys.h"

#define TAG "CC1101"
#include "esp_log.h"

#include "cc1101_param.h"
#include "cc1101.h"

/************************************************************
 * CC1101 SPI device configuration
 */

#if CONFIG_CC_SPI2_HOST
#define CC_HOST_ID SPI2_HOST
#elif CONFIG_CC_SPI3_HOST
#define CC_HOST_ID SPI3_HOST
#endif

static spi_bus_config_t const cc_buscfg = {
  .sclk_io_num = CONFIG_CC_SCK_GPIO, // set SPI CLK pin
  .mosi_io_num = CONFIG_CC_MOSI_GPIO, // set SPI MOSI pin
  .miso_io_num = CONFIG_CC_MISO_GPIO, // set SPI MISO pin
  .quadwp_io_num = -1,
  .quadhd_io_num = -1
};

static spi_device_interface_config_t const cc_devcfg = {
  .clock_speed_hz = 1000000, // SPI clock is 1 MHz!
  .queue_size = 7,
  .mode = 0, // SPI mode 0
  .spics_io_num = CONFIG_CC_CSN_GPIO,
  .flags = SPI_DEVICE_NO_DUMMY
};

static spi_device_handle_t cc_handle;
static bool cc_io_ok = true;

static void cc_spi_init() {
  esp_err_t ret;

  ret = spi_bus_initialize( CC_HOST_ID, &cc_buscfg, SPI_DMA_CH_AUTO );
  ESP_LOGI( TAG, "spi_bus_initialize=%d", ret );
  assert( ret==ESP_OK );

  ret = spi_bus_add_device( CC_HOST_ID, &cc_devcfg, &cc_handle );
  ESP_LOGI( TAG, "spi_bus_add_device=%d", ret );
  assert(ret==ESP_OK);
}

static bool spi_write_byte(uint8_t *status, uint8_t* Dataout, size_t DataLength )
{
  // The status destination is one byte even for a two-byte register write.
  // Receive the whole transfer inline, then copy only the chip status byte.
  spi_transaction_t transaction = {
    .flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA,
    .length = DataLength * 8,
  };
  assert( DataLength > 0 && DataLength <= sizeof(transaction.tx_data) );
  memcpy( transaction.tx_data, Dataout, DataLength );
  esp_err_t err = spi_device_transmit( cc_handle, &transaction );
  if( err != ESP_OK ) {
    cc_io_ok = false;
    *status = 0xFF;
    ESP_LOGE( TAG, "SPI write failed: %s", esp_err_to_name(err) );
    return false;
  }
  *status = transaction.rx_data[0];
  return true;
}

static bool spi_read_byte(uint8_t* Datain, uint8_t* Dataout, size_t DataLength )
{
  spi_transaction_t transaction = {
    .flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA,
    .length = DataLength * 8,
  };
  assert( DataLength > 0 && DataLength <= sizeof(transaction.tx_data) );
  memcpy( transaction.tx_data, Dataout, DataLength );
  esp_err_t err = spi_device_transmit( cc_handle, &transaction );
  if( err != ESP_OK ) {
    cc_io_ok = false;
    memset( Datain, 0xFF, DataLength );
    ESP_LOGE( TAG, "SPI read failed: %s", esp_err_to_name(err) );
    return false;
  }
  memcpy( Datain, transaction.rx_data, DataLength );
  return true;
}

/************************************************************
 * CC1101 SPI device basic actions
 */
#define delayMicroseconds(us) esp_rom_delay_us(us)
#define LOW  0
#define HIGH 1

#define cc_assert()    gpio_set_level( CONFIG_CC_CSN_GPIO, LOW  )
#define cc_deassert()  gpio_set_level( CONFIG_CC_CSN_GPIO, HIGH )

static uint8_t cc_read( uint8_t addr ) {
  uint8_t out[2] = { addr | CC_READ, 0 };
  uint8_t data[2] ;

  spi_read_byte( data, out, 2 );

  return data[1];
}

static uint8_t cc_write( uint8_t addr, uint8_t b ) {
  uint8_t result;

  uint8_t out[2] = { addr, b };
  spi_write_byte( &result, out, 2 );

  return result;
}

uint8_t cc_strobe( uint8_t cmd )
{
  uint8_t result;

  spi_write_byte( &result, &cmd, 1 );

  return result;
}

uint8_t cc_write_fifo(uint8_t b) {
  uint8_t result = cc_write( CC_FIFO, b );
  if( (result & CC_CHIP_RDY) || CC_STATE(result) == CC_STATE_TX_UNDERFLOW )
    cc_io_ok = false;
  return result & 0x0F;	// TX fifo space
}

/************************************************************
 * CC1101 Mode control
 */

static bool cc_wait_state( uint8_t expected ) {
  int64_t deadline = esp_timer_get_time() + 20000;
  uint8_t status;
  do {
    status = cc_strobe( CC_SNOP );
    if( !cc_io_ok ) return false;
    if( !(status & CC_CHIP_RDY) && CC_STATE(status) == expected ) return true;
    esp_rom_delay_us( 20 );
  } while( esp_timer_get_time() < deadline );
  ESP_LOGE( TAG, "Mode timeout: expected 0x%02x, status 0x%02x", expected, status );
  return false;
}

bool cc_enter_idle_mode(void) {
  cc_io_ok = true; // Allow a new recovery attempt after a failed transfer.
  cc_strobe( CC_SIDLE );
  return cc_wait_state( CC_STATE_IDLE );
}

bool cc_enter_rx_mode(void) {
  if( !cc_enter_idle_mode() ) return false;
  // Clear both FIFOs, including a previous TX-underflow condition.
  cc_strobe( CC_SFTX );
  cc_strobe( CC_SFRX );
  cc_write( CC_IOCFG0, 0x2E );      // GDO0 not needed
  cc_write( CC_PKTCTRL0, 0x32 );    // Asynchronous, infinite packet
  cc_strobe( CC_SRX );
  return cc_wait_state( CC_STATE_RX );
}

bool cc_enter_tx_mode(void) {
  if( !cc_enter_idle_mode() ) return false;
  cc_write( CC_PKTCTRL0, 0x02 );    // FIFO mode, infinite packet
  cc_write( CC_IOCFG0, 0x02 );      // Falling edge, TX FIFO low
  cc_strobe( CC_SFTX );
  // Stay IDLE until the frame layer has primed the FIFO.
  return cc_io_ok;
}

bool cc_start_tx(void) {
  if( !cc_io_ok ) return false;
  cc_strobe( CC_STX );
  return cc_wait_state( CC_STATE_TX );
}

bool cc_tx_status( uint8_t *bytes, bool *underflow ) {
  uint8_t status = cc_read( CC_TXBYTES );
  // TXBYTES is updated asynchronously. Require two matching reads.
  for( unsigned i=0; i<4; i++ ) {
    uint8_t next = cc_read( CC_TXBYTES );
    if( !cc_io_ok ) return false;
    if( next == status ) {
      *bytes = status & 0x7F;
      *underflow = (status & 0x80) != 0;
      return true;
    }
    status = next;
  }
  return false;
}

void cc_fifo_end(void) {
  cc_write( CC_IOCFG0, 0x05 ); 		// Rising edge, TX Fifo empty
}

uint8_t cc_read_rssi(void) {
  // CC1101 Section 17.3
  int8_t rssi = (int8_t )cc_read( CC_RSSI );
  rssi = rssi/2 - 74;  // answer in range -138 to -10

  return (uint8_t)( -rssi ); // returns 10 to 138
}

/************************************************************
 * CC1101 initialisation
 */
static void cc_spi_reset(void) {
  gpio_reset_pin( CONFIG_CC_CSN_GPIO );
  gpio_set_direction( CONFIG_CC_CSN_GPIO, GPIO_MODE_OUTPUT );
  gpio_set_level( CONFIG_CC_CSN_GPIO, 1 );

  cc_deassert();
  delayMicroseconds(1);

  cc_assert();
  delayMicroseconds(10);

  cc_deassert();
  delayMicroseconds(41);
}

void cc_init(void) {
  uint8_t param[CC_PARAM_MAX];
  uint8_t i,len;

  esp_log_level_set(TAG, CONFIG_CC_LOG_LEVEL );

  cc_spi_reset();
  cc_spi_init();

  cc_strobe(CC_SRES);
  //cc_strobe(CC_SCAL);

  len = cc_cfg_get( 0, param, sizeof(param) );
  for ( i=0 ; i<len ; i++ )
    cc_write( i, param[i] );

  cc_write( CC_FIFOTHR, ( param[CC_FIFOTHR]&0xF0 )+14 );	  // TX Fifo Threshold 5

  len = cc_pa_get( param );
  for ( i=0 ; i<len ; i++ )
    cc_write( CC_PATABLE, param[i]);

  cc_enter_rx_mode();
}

