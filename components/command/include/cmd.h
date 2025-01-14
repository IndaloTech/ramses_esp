/********************************************************************
 * ramses_esp
 * cmd.h
 *
 * (C) 2024 Peter Price
 *
 * General command handling
 *
 */
#ifndef _CMD_H_
#define _CMD_H_

#include <string.h>
#include <stdio.h>

#include "esp_err.h"
#include "esp_console.h"

#define NULL_COMMAND .command = NULL, .help = NULL,  .hint = NULL,  .func = NULL

extern int cmd_menu( int argc, char **argv, esp_console_cmd_t const *cmds, char const *menu );
extern void cmd_menu_register( esp_console_cmd_t const *cmd );

extern esp_err_t cmd_run( char const *cmdline, int *cmd_ret );

struct cmd_data;
extern struct cmd_data *cmd_init(void);
extern void cmd_work( struct cmd_data *data );

#endif // _CMD_H_
