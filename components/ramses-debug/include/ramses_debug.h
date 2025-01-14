/********************************************************************
 * ramses_esp
 * ramses_debug.h
 *
 * (C) 2023 Peter Price
 *
 * Simple GPIO debug
 */

#ifndef _RAMSES_DEBUG_H_
#define _RAMSES_DEBUG_H_

#include "sdkconfig.h"
#include "driver/gpio.h"

#define DEBUG_PIN(_p) ( 1ULL << (_p) )
#define DEBUG_ON(_p)  gpio_set_level( _p, 1 )
#define DEBUG_OFF(_p) gpio_set_level( _p, 0 )

#if CONFIG_DEBUG_PIN1
#define DEBUG1	    DEBUG_PIN(CONFIG_DEBUG_PIN1)
#define DEBUG1_ON   do{ DEBUG_ON(CONFIG_DEBUG_PIN1);  } while(0)
#define DEBUG1_OFF  do{ DEBUG_OFF(CONFIG_DEBUG_PIN1); } while(0)
#else
#define DEBUG1	    0
#define DEBUG1_ON   do{}while(0)
#define DEBUG1_OFF  do{}while(0)
#endif

#if CONFIG_DEBUG_PIN2
#define DEBUG2	    DEBUG_PIN(CONFIG_DEBUG_PIN2)
#define DEBUG2_ON   do{ DEBUG_ON(CONFIG_DEBUG_PIN2);  } while(0)
#define DEBUG2_OFF  do{ DEBUG_OFF(CONFIG_DEBUG_PIN2); } while(0)
#else
#define DEBUG2      0
#define DEBUG2_ON   do{}while(0)
#define DEBUG2_OFF  do{}while(0)
#endif

#if CONFIG_DEBUG_PIN3
#define DEBUG3	    DEBUG_PIN(CONFIG_DEBUG_PIN3)
#define DEBUG3_ON   do{ DEBUG_ON(CONFIG_DEBUG_PIN3);  } while(0)
#define DEBUG3_OFF  do{ DEBUG_OFF(CONFIG_DEBUG_PIN3); } while(0)
#else
#define DEBUG3	    0
#define DEBUG3_ON   do{}while(0)
#define DEBUG3_OFF  do{}while(0)
#endif

#if CONFIG_DEBUG_PIN4
#define DEBUG4	    DEBUG_PIN(CONFIG_DEBUG_PIN3)
#define DEBUG4_ON   do{ DEBUG_ON(CONFIG_DEBUG_PIN4);  } while(0)
#define DEBUG4_OFF  do{ DEBUG_OFF(CONFIG_DEBUG_PIN4); } while(0)
#else
#define DEBUG4	    0
#define DEBUG4_ON   do{}while(0)
#define DEBUG4_OFF  do{}while(0)
#endif

#if CONFIG_DEBUG_PIN5
#define DEBUG5	    DEBUG_PIN(CONFIG_DEBUG_PIN5)
#define DEBUG5_ON   do{ DEBUG_ON(CONFIG_DEBUG_PIN5);  } while(0)
#define DEBUG5_OFF  do{ DEBUG_OFF(CONFIG_DEBUG_PIN5); } while(0)
#else
#define DEBUG5	    0
#define DEBUG5_ON   do{}while(0)
#define DEBUG5_OFF  do{}while(0)
#endif

#if CONFIG_DEBUG_PIN6
#define DEBUG6	    DEBUG_PIN(CONFIG_DEBUG_PIN6)
#define DEBUG6_ON   do{ DEBUG_ON(CONFIG_DEBUG_PIN6);  } while(0)
#define DEBUG6_OFF  do{ DEBUG_OFF(CONFIG_DEBUG_PIN6); } while(0)
#else
#define DEBUG6	    0
#define DEBUG6_ON   do{}while(0)
#define DEBUG6_OFF  do{}while(0)
#endif

#define DEBUG_MASK ( DEBUG1 | DEBUG2 | DEBUG3 | DEBUG4 | DEBUG5 | DEBUG6 )

extern void ramses_debug_init(void);

#endif // _RAMSES_DEBUG_H_
