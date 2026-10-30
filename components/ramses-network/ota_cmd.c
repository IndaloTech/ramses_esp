/********************************************************************
 * ramses_esp
 * ota_cmd.c
 *
 * (C) 2023 Peter Price
 *
 * OTA Commands
 *
 */
#include "cmd.h"
#include "ota_cmd.h"
#include "ramses_ota.h"

/*********************************************************
 * ORL command
 */
static int ota_cmd_url( int argc, char **argv ) {

  if( argc>1 )
    ota_set_url( argv[1] );

  return 0;
}

/*********************************************************
 * Version command
 */
static int ota_cmd_version( int argc, char **argv ) {

  if( argc>1 )
    ota_set_version( argv[1] );

  return 0;
}

/*********************************************************
 * filename command
 */
static int ota_cmd_filename( int argc, char **argv ) {

  if( argc>1 )
	ota_set_filename( argv[1] );

  return 0;
}

/*********************************************************
 * Start command
 */
static int ota_cmd_start( int argc, char **argv ) {

  ota_start(0);

  return 0;
}

static int ota_cmd_force( int argc, char **argv ) {

  ota_start(1);

  return 0;
}

/*********************************************************
 * check command
 */
static int ota_cmd_check( int argc, char **argv ) {

  ota_check();

  return 0;
}

/*********************************************************
 * Top Level command
 */
static esp_console_cmd_t const ota_cmds[] = {
  {
    .command = "url",
    .help = "Set OTA <url>",
    .hint = NULL,
    .func = ota_cmd_url,
  },
  {
    .command = "version",
    .help = "Set OTA <version>",
    .hint = NULL,
    .func = ota_cmd_version,
  },
  {
    .command = "filename",
    .help = "Set OTA <filename>",
    .hint = NULL,
    .func = ota_cmd_filename,
  },
  {
    .command = "start",
    .help = "Start OTA",
    .hint = NULL,
    .func = ota_cmd_start,
  },
  {
    .command = "force",
    .help = "Force OTA",
    .hint = NULL,
    .func = ota_cmd_force,
  },
  {
    .command = "check",
    .help = "Get latest version",
    .hint = NULL,
    .func = ota_cmd_check,
  },
  // List termination
  { NULL_COMMAND }
};

static int ota_cmd( int argc, char **argv ) {
  return cmd_menu( argc, argv, ota_cmds, argv[0] );
}

void ota_register(void) {
  const esp_console_cmd_t ota[] = {
    {
      .command = "ota",
      .help = "OTA commands, enter 'ota' for list",
	  .hint = NULL,
	  .func = &ota_cmd,
    },
	{ NULL_COMMAND }
  };

  cmd_menu_register( ota );
}
