/********************************************************************
 * ramses_esp
 * ramses_led.h
 *
 * (C) 2023 Peter Price
 *
 * LED control
 *
 */

#ifndef _RAMSES_LED_H_
#define _RAMSES_LED_H_

enum LED_ID {
  LED_RX,
  LED_TX,
  LED_MAX
};

void ramses_led_init(void);
void led_on( enum LED_ID led );
void led_off( enum LED_ID led);

#endif // _RAMSES_LED_H_
