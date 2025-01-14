/********************************************************************
 * ramses_esp
 * msg.c
 *
 * (C) 2023 Peter Price
 *
 * RAMSES message handling
 * General message utilities
 *
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "message.h"
#include "msg.h"

char *msg_timestamp( char *timestamp, int len ) {
  struct timeval tv;
  struct tm *nowtm;
  ssize_t written = -1;

  gettimeofday(&tv, NULL);
  nowtm = localtime(&tv.tv_sec);

  written = (ssize_t)strftime( timestamp,len, "%Y-%m-%dT%H:%M:%S", nowtm);
  if (( written>0 ) && ( (size_t)(written+7)<len ))
    written += snprintf( timestamp+written, len-(size_t)written, ".%06ld", tv.tv_usec);
  if (( written>0 ) && ( (size_t)(written+6)<len )) {
	char tz[8];
	strftime( tz,8, "%z", nowtm );
	// strftime does not generate ISO 8601 timezone - there's no ':'
	written += sprintf( timestamp+written,"%c%c%c:%c%c", tz[0],tz[1],tz[2],tz[3],tz[4] );
  }

  return timestamp;
}

char const *msg_get_ts( struct message const  *msg ){ return msg->timestamp; }

/********************************************************
** Message Header
********************************************************/

static const uint8_t address_flags[4] = {
  F_ADDR0+F_ADDR1+F_ADDR2 ,
                  F_ADDR2 ,
  F_ADDR0+        F_ADDR2 ,
  F_ADDR0+F_ADDR1
};

#define HDR_T_MASK 0x30
#define HDR_T_SHIFT   4
#define HDR_A_MASK 0x0C
#define HDR_A_SHIFT   2
#define HDR_PARAM0 0x02
#define HDR_PARAM1 0x01

uint8_t msg_decode_header( uint8_t header ) {
  uint8_t fields;

  fields = ( header & HDR_T_MASK ) >> HDR_T_SHIFT;   // Message type
  fields |= address_flags[ ( header & HDR_A_MASK ) >> HDR_A_SHIFT ];
  if( header & HDR_PARAM0 ) fields |= F_PARAM0;
  if( header & HDR_PARAM1 ) fields |= F_PARAM1;

  return fields;
}

uint8_t msg_encode_header( uint8_t fields ) {
  uint8_t i;

  uint8_t header = 0xFF;
  uint8_t addresses = fields & ( F_ADDR0+F_ADDR1+F_ADDR2 );

  for( i=0 ; i<sizeof(address_flags) ; i++ ) {
    if( addresses==address_flags[i] ) {
      header = i << HDR_A_SHIFT;
      header |= ( fields & F_MASK ) << HDR_T_SHIFT;  // Message type
      if( fields & F_PARAM0 ) header |= HDR_PARAM0;
      if( fields & F_PARAM1 ) header |= HDR_PARAM1;
      break;
    }
  }

  return header;
}

/********************************************************
** Address conversion
********************************************************/
void msg_encode_address( uint8_t *addr, uint8_t class, uint32_t id ) {
  if( addr!=NULL ) {
    addr[0] = ( ( class<< 2 ) & 0xFC ) | ( ( id >> 16 ) & 0x03 );
    addr[1] =                            ( ( id >>  8 ) & 0xFF );
    addr[2] =                            ( ( id       ) & 0xFF );
  }
}

void msg_decode_address( uint8_t *addr, uint8_t *class, uint32_t *id ) {
  if( addr!=NULL ) {
    uint8_t Class =         ( addr[0] & 0xFC ) >>  2 ;
    uint32_t Id = (uint32_t)( addr[0] & 0x03 ) << 16
                | (uint32_t)( addr[1]        ) <<  8
                | (uint32_t)( addr[2]        )       ;

    if( class ) (*class) = Class;
    if( id    ) (*id   ) = Id;
  }
}

void msg_make_addr( uint32_t *addr, uint8_t class, uint32_t id ) {
  if( addr!=NULL ) {
	uint8_t a[3];
    msg_encode_address( a, class, id );
    (*addr) = ( a[0] << 16 )
            | ( a[1] <<  8 )
            | ( a[2] <<  0 ) ;
  }
}
/********************************************************
** Message fields
********************************************************/

void msg_reset( struct message *msg ) {
  if( msg != NULL ) {
    memset( msg, 0, sizeof(*msg) );
  }
}

/***********************************************************/

void msg_set_fields( struct message *msg, uint8_t  fields ) {
  if( msg != NULL ) {
	msg->fields = fields;
  }
}

void msg_get_fields( struct message *msg, uint8_t *fields ) {
  uint8_t res = 0;

  if( msg!=NULL )
	res = msg->fields;

  if( fields!=NULL )
	(*fields) = res;
}

/***********************************************************/

void msg_set_address( struct message *msg, uint8_t i, uint32_t  addr ) {
  if( msg!=NULL && i<MAX_ADDR ) {
	uint8_t *a = msg->addr[i];
    a[0] = ( addr >> 16 ) & 0xFF ;
    a[1] = ( addr >>  8 ) & 0xFF ;
    a[2] = ( addr >>  0 ) & 0xFF ;
  }
}

void msg_get_address( struct message *msg, uint8_t i, uint32_t *addr ) {
  if( msg!=NULL && i<MAX_ADDR ) {
	uint8_t *a = msg->addr[i];
    if( addr ) {
	  *addr = ( a[0] << 16 )
            | ( a[1] <<  8 )
            | ( a[2] <<  0 );
    }
  }
}

/***********************************************************/

void msg_set_opcode( struct message *msg, uint16_t  opcode ) {
  if( msg!=NULL ) {
	msg->opcode[0] = ( opcode >> 8 ) & 0xFF ;
	msg->opcode[1] = ( opcode >> 0 ) & 0xFF ;
  }
}

void msg_get_opcode( struct message *msg, uint16_t *opcode ) {
  if( msg!=NULL ) {
    if( opcode!=NULL ) {
      *opcode = (uint16_t)( msg->opcode[0] << 8 )
              | (uint16_t)( msg->opcode[0] << 0 );
    }
  }
}

/***********************************************************/

void msg_set_payload( struct message *msg, uint8_t  len, uint8_t  *payload ) {
  if( msg!=NULL ) {
    if( payload!=NULL ){
      uint8_t i;
      if( len>MAX_PAYLOAD ) len = MAX_PAYLOAD;
      for ( i=0 ; i<len ; i++ )
        msg->payload[i] = payload[i];
      msg->nPayload = len;
    }
  }
}

void msg_get_payload( struct message *msg, uint8_t *len, uint8_t **payload ) {
  if( msg!=NULL ) {
    if( payload!=NULL  ) (*payload) = msg->payload ;
    if( len!=NULL ) (*len) = msg->nPayload;
  }
}

/********************************************************
** Message Checksum
********************************************************/
uint8_t msg_checksum( struct message *msg ) {
  uint8_t csum;
  uint8_t i,j;

  // Missing fields will be zero so we can just add them to checksum without testing presence
                                                  { csum  = msg_encode_header(msg->fields); }
  for( i=0 ; i<3 ; i++ ) { for( j=0 ; j<3 ; j++ ) { csum += msg->addr[i][j]; }       }
  for( i=0 ; i<2 ; i++ )                          { csum += msg->param[i];           }
  for( i=0 ; i<2 ; i++ )                          { csum += msg->opcode[i];          }
                                                  { csum += msg->len;                }
  for( i=0 ; i<msg->nPayload ; i++ )              { csum += msg->payload[i];         }

  return -csum;
}

/************************************************************************************
**
** Message status functions
**/
uint8_t msg_isValid( struct message *msg ) {
  uint8_t isValid = 0;
  
  if( msg ) {
	isValid = ( msg->error==MSG_OK );
  }
  
  return isValid;
}

uint8_t msg_isTx( struct message *msg ) {
  uint8_t isTx = 0;
  
  if( msg ) {
	isTx = msg_isValid( msg ) && ( msg->rssi==0 );
  }

  return isTx;
}

/************************************************************************************
**
** Create a message
**/
struct message *msg_create( enum msg_type type, uint32_t addr[MAX_ADDR], uint16_t opcode, uint8_t len, uint8_t *payload ) {
  struct message *msg = msg_alloc();
  if( msg ) {
	uint8_t fields  = type ;
	uint8_t i;
	for( i=0 ; i<MAX_ADDR ; i++ ) {
	  if( addr[i] != 0 ) {
        msg_set_address( msg, i, addr[i] );
        fields |= F_ADDR0<<i ;
	  }
	}
	msg_set_opcode( msg, opcode );
    msg_set_payload( msg, len, payload );
  }

  return msg;
}
