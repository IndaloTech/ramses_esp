/********************************************************************
 * ramses_esp
 * cc1101.h
 *
 * (C) 2023 Peter Price
 *
 * Hardware interface to TI CC1101 radio chip
 *
 */
#ifndef _CC1101_H_
#define _CC1101_H_

#include <stdint.h>
#include <stdbool.h>

#define CC_MAX_PARAM 5
extern uint8_t cc_param( uint8_t reg, uint8_t nReg, uint8_t *param );
extern void cc_param_read( uint8_t reg, uint8_t nReg, uint8_t *param );

extern uint8_t cc_read_rssi(void);

extern bool cc_enter_idle_mode(void);
extern bool cc_enter_rx_mode(void);
// Prepare an empty FIFO in IDLE; call cc_start_tx only after priming it.
extern bool cc_enter_tx_mode(void);
extern bool cc_start_tx(void);
extern bool cc_tx_status( uint8_t *bytes, bool *underflow );

extern uint8_t cc_write_fifo(uint8_t b);
extern void cc_fifo_end(void);

extern void cc_init(void);
extern void cc_work(void);

#endif
