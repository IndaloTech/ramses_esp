/********************************************************************
 * ramses_esp
 * cc1101_param.c
 *
 * (C) 2023 Peter Price
 *
 * Parameter management for cc1101
 *
 */
#include <string.h>

#include "cc1101_param.h"

static inline uint8_t cc1101_cfg_default( uint8_t *cfg ) {
  // CC1101 default register settings
  static const uint8_t CC_DEFAULT_CFG[CC_PARAM_MAX] = {
    0x0D,  //  CC_IOCFG2 	  GDO2- RX data
    0x2E,  //  CC_IOCFG1 	  GDO1- not used
    0x2E,  //  CC_IOCFG0	  GDO0- TX data
    0x07,  //  CC_FIFOTHR   default
    0xD3,  //  CC_SYNC1     default
    0x91,  //  CC_SYNC0     default
    0xFF,  //  CC_PKTLEN	  default
    0x04,  //  CC_PKTCTRL1  default
    0x31,  //  CC_PKTCTRL0  Asynchornous Serial, TX on GDO0, RX on GDOx
    0x00,  //  CC_ADDR      default
    0x00,  //  CC_CHANNR    default
    0x0f,  //  CC_FSCTRL1   default
    0x00,  //  CC_FSCTRL0   default
    0x21,  //  CC_FREQ2     /
    0x65,  //  CC_FREQ1     / 868.3 MHz
    0x6A,  //  CC_FREQ0     /
    0x6A,  //  CC_MDMCFG4   //
    0x83,  //  CC_MDMCFG3   // DRATE_M=131 data rate=38,383.4838867Hz
    0x10,  //  CC_MDMCFG2   GFSK, No Sync Word
    0x22,  //  CC_MDMCFG1   FEC_EN=0, NUM_PREAMBLE=4, CHANSPC_E=2
    0xF8,  //  CC_MDMCFG0   Channel spacing 199.951 KHz
    0x50,  //  CC_DEVIATN
    0x07,  //  CC_MCSM2     default
    0x10,  //  CC_MCSM1     CCA if RSSI below threshold
    0x08,  //  CC_MCSM0     No Auto-calibrate, Power on timeout 149-155 uS
    0x16,  //  CC_FOCCFG    default
    0x6c,  //  CC_BSCFG     default
    0x43,  //  CC_AGCCTRL2
    0x40,  //  CC_AGCCTRL1  default
    0x91,  //  CC_AGCCTRL0  default
    0x87,  //  CC_WOREVT1   default
    0x6B,  //  CC_WOREVT0   default
    0xF8,  //  CC_WORCTRL   default
    0x56,  //  CC_FREND1    default
    0x10,  //  CC_FREND0    default
    0xE9,  //  CC_FSCAL3
    0x21,  //  CC_FSCAL2
    0x00,  //  CC_FSCAL1
    0x1f,  //  CC_FSCAL0
    0x41,  //  CC_RCCTRL1   default
    0x00,  //  CC_RCCTRL0   default
    0x59,  //  CC_FSTEST    default
    0x7F,  //  CC_PTEST     default
    0x3F,  //  CC_AGCTEST   default
    0x81,  //  CC_TEST2
    0x35,  //  CC_TEST1
    0x09,  //  CC_TEST0
  };
  uint8_t len=0;

  if( cfg ) {
    len = sizeof(CC_DEFAULT_CFG);
    memcpy( cfg, CC_DEFAULT_CFG, len );
  }

  return len;
}

static inline uint8_t cc1101_pa_default( uint8_t *paTable ) {
  // CC1101 default Power Ramp settings
  static const uint8_t CC_DEFAULT_PA[CC_PA_MAX] = {
    0xC3,0,0,0,0,0,0,0
  };
  uint8_t len=0;

  if( paTable ) {
    len = sizeof(CC_DEFAULT_PA);
    memcpy( paTable, CC_DEFAULT_PA, len );
  }

  return len;
}

uint8_t cc_cfg_get( uint8_t param, uint8_t *buff, uint8_t nParam ) {
  uint8_t len=0;

  if( param < CC_PARAM_MAX ) {
    uint8_t cfg[CC_PARAM_MAX];
    len = cc1101_cfg_default( cfg );

    if( len && len>param )
      len = len - param;
    else
      len = 0;

    if( nParam > len )
      nParam = len;

    len = nParam;
    memcpy( buff, cfg+param, len );
  }

  return len;
}

uint8_t cc_pa_get( uint8_t *paTable ) {
  uint8_t len=0;

  if( paTable ) {
    uint8_t palen = cc1101_pa_default( paTable );

    for( len=0 ; len<palen ; len++ ) {
      if( paTable[len]==0x00 || paTable[len]==0xFF )
	    break;
    }
  }

  return len;
}
