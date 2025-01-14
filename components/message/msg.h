/********************************************************************
 * ramses_esp
 * msg.h
 *
 * (C) 2025 Peter Price
 *
 * RAMSES message private
 *
 */
#ifndef _MSG_H_
#define _MSG_H_

#include <string.h>

#define MAX_RAW 162
#define MAX_PAYLOAD 64
#define MAX_ADDR 3
struct message {
  struct message *next;
  struct message *prev;

  uint8_t state;
  uint8_t count;

  uint8_t fields;  // Fields specified in header
  uint8_t rxFields;  // Fields actually received
  uint8_t error;

  uint8_t addr[MAX_ADDR][3];
  uint8_t param[2];

  uint8_t opcode[2];
  uint8_t len;

  uint8_t csum;
  uint8_t rssi;

  uint8_t nPayload;
  uint8_t payload[MAX_PAYLOAD];

  uint8_t nBytes;
  uint8_t raw[MAX_RAW];

#define MSG_TIMESTAMP 36
  char timestamp[MSG_TIMESTAMP];
};

/********************************************************
** Message status
********************************************************/
enum message_state {
  S_START,
  S_HEADER,
  S_ADDR0,
  S_ADDR1,
  S_ADDR2,
  S_PARAM0,
  S_PARAM1,
  S_OPCODE,
  S_LEN,
  S_PAYLOAD,
  S_CHECKSUM,
  S_TRAILER,
  S_COMPLETE,
  S_ERROR
};

#define _MSG_TYPE_LIST \
  _MSG_TYPE( F_RQ,      "RQ" ) \
  _MSG_TYPE( F_I,       "I"  ) \
  _MSG_TYPE( F_W,       "W"  ) \
  _MSG_TYPE( F_RP,      "RP" ) \

#define _MSG_TYPE(_e,_t) _e,
enum msg_type { _MSG_TYPE_LIST MSG_TYPE_MAX };
#undef _MSG_TYPE
#define F_MASK   0x03

#define F_ADDR0  0x10
#define F_ADDR1  0x20
#define F_ADDR2  0x40

#define F_PARAM0 0x04
#define F_PARAM1 0x08
#define F_RSSI   0x80

// Only used for received fields
#define F_OPCODE 0x01
#define F_LEN    0x02

#define F_OPTION ( F_ADDR0 + F_ADDR1 + F_ADDR2 + F_PARAM0 + F_PARAM1 )
#define F_MAND   ( F_OPCODE + F_LEN )

#define HDR_T_MASK 0x30
#define HDR_T_SHIFT   4
#define HDR_A_MASK 0x0C
#define HDR_A_SHIFT   2
#define HDR_PARAM0 0x02
#define HDR_PARAM1 0x01


extern char *msg_timestamp( char *timestamp, int len );

extern uint8_t msg_decode_header( uint8_t header );
extern uint8_t msg_encode_header( uint8_t flags );

extern void msg_reset( struct message *msg );

extern void msg_set_fields( struct message *msg, uint8_t  fields );
extern void msg_get_fields( struct message *msg, uint8_t *fields );

extern void msg_set_address( struct message *msg, uint8_t i, uint32_t  addr );
extern void msg_get_address( struct message *msg, uint8_t i, uint32_t *addr );

extern void msg_set_opcode( struct message *msg, uint16_t  opcode );
extern void msg_get_opcode( struct message *msg, uint16_t *opcode );

extern void msg_set_payload( struct message *msg, uint8_t  len, uint8_t  *payload );
extern void msg_get_payload( struct message *msg, uint8_t *len, uint8_t **payload );

extern uint8_t msg_checksum( struct message *msg );

extern uint8_t msg_isValid( struct message *msg );
extern uint8_t msg_isTx( struct message *msg );

extern struct message *msg_create( enum msg_type type, uint32_t addr[MAX_ADDR], uint16_t opcode, uint8_t len, uint8_t *payload );

#endif // _MSG_H_
