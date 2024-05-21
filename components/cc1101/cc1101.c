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
  if ( DataLength > 0 ) {
    spi_transaction_t SPITransaction = {
      .length = DataLength * 8,
	  .tx_buffer = Dataout,
	  .rx_buffer = status
	};

	spi_device_transmit( cc_handle, &SPITransaction );
  }

  return true;
}

static bool spi_read_byte(uint8_t* Datain, uint8_t* Dataout, size_t DataLength )
{
  if ( DataLength > 0 ) {
	spi_transaction_t SPITransaction = {
	  .length = DataLength * 8,
	  .tx_buffer = Dataout,
	  .rx_buffer = Datain
	};

	spi_device_transmit( cc_handle, &SPITransaction );
  }

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
  return result & 0x0F;	// TX fifo space
}

/************************************************************
 * CC1101 Mode control
 */

void cc_enter_idle_mode(void) {
  while ( CC_STATE( cc_strobe( CC_SIDLE ) ) != CC_STATE_IDLE );
}

void cc_enter_rx_mode(void) {
//  while ( CC_STATE( cc_strobe( CC_SIDLE ) ) != CC_STATE_IDLE ){}

  cc_write( CC_IOCFG0, 0x09 );      // Show Clear Channel Assessment
  cc_write( CC_PKTCTRL0, 0x32 );	// Asynchronous, infinite packet

//  cc_strobe( CC_SFRX );
  while ( CC_STATE( cc_strobe( CC_SRX ) ) != CC_STATE_RX ){}
}

void cc_enter_tx_mode(void) {
//  while ( CC_STATE( cc_strobe( CC_SIDLE ) ) != CC_STATE_IDLE ){}

  cc_write( CC_PKTCTRL0, 0x02 );    // Fifo mode, infinite packet
  cc_write( CC_IOCFG0, 0x02 );      // Falling edge, TX Fifo low

//  cc_strobe( CC_SFTX );
  while ( CC_STATE( cc_strobe( CC_STX ) ) != CC_STATE_TX ){}
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

  cc_enter_idle_mode();
  cc_enter_rx_mode();
}

