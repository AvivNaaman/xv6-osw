/**
 * Pouch configuration file management utilities
 */
#include "container.h"
#include "pouch.h"

#define CONFIG_KEY_PPID "PPID:"
#define CONFIG_KEY_NAME "NAME:"
#define CONFIG_KEY_IMAGE "IMAGE:"

typedef struct container_config {
  char container_name[CNTNAMESIZE];
  char tty_name[CNTNAMESIZE];
  int pid;
  char image_name[CNTNAMESIZE];
} container_config;

pouch_status init_pouch_conf();

/*
 *   Write to pconf
 *   - pconf is a file that holds container name that is currently attached to a
 * tty
 *   - pconf name is similar to tty name, having knows naming structure to open
 * it
 *   - used for 'printing all containers list 'pouch list all' command
 *   @input: container_name, cgcname
 */
pouch_status write_to_pconf(const char* const ttyname, const char* const cname);

/*
 *   Remove from pconf
 *   - when container is deleted, tty is detatched, need to remove it's name
 * from pconf
 *   @input: ttyname
 *   @output: none
 */
pouch_status remove_from_pconf(char* ttyname);
/*
 *   Read from pconf
 *   - get container name from provided tty name
 *   @input: ttyname
 *   @output: cname
 */
pouch_status container_name_by_tty(const char* const ttyname,
                                   char* const cname);

/*
 *   Read from conf:
 *   - Reading container information from container's object
 *   @input: container_name
 *   @output: tty_name, pid
 */
pouch_status read_from_cconf(const char* const container_name,
                             container_config* conf);

/*
 *   Write to conf:
 *   - Writing container information to container's object
 *   @input: container_name, tty_name, pid
 *   @output: none
 */
pouch_status write_to_cconf(const container_config* conf);
