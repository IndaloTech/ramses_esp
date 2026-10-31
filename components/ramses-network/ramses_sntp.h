/********************************************************************
 * ramses_esp
 * ramses_sntp.h
 *
 * (C) 2023 Peter Price
 *
 * SNTP Client
 *
 */
#ifndef _RAMSES_SNTP_H_
#define _RAMSES_SNTP_H_

extern bool ramses_sntp_set_server( char *server );
extern void ramses_sntp_init( BaseType_t coreID, char *server );

#endif // _RAMSES_SNTP_H_
