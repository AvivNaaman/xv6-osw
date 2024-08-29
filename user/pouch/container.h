#include "pouch.h"

#define CNTNAMESIZE (100)

/*
 *   Pouch fork:
 *   - Starting new container and execute shell inside, waiting for container to
 * exit
 *   @input: container_name,root_dir
 *   @output: none
 *   @return: 0 - OK, <0 - FAILURE
 */
pouch_status pouch_container_start(const char* const container_name,
                                   const char* const image_name);

/*
 *   Pouch stop:
 *   - Stopping container
 *   @input: container_name
 *   @output: none
 *   @return: 0 - OK, <0 - FAILURE
 */
pouch_status pouch_container_stop(const char* const container_name);

pouch_status pouch_container_connect(const char* const container_name);
pouch_status pouch_container_disconnect(const char* const container_name);

/*
 *   Print given container information
 *   - show all started containers and their state
 *   @input: container_name,tty_name,pid
 *   @output: none
 *   @return: 0 - OK, <0 - FAILURE
 */
pouch_status print_cinfo(const char* const container_name);

/*
 *   Print cotainers list
 *   - show all started containers and their state
 *   @input: none
 *   @output: none
 *   @return: 0 - OK, <0 - FAILURE
 */
pouch_status print_clist();

/*
 *   Get connected container name
 *   @input: none
 *   @output: cname
 *   @return: 0 - OK, <0 - FAILURE
 */
pouch_status get_connected_cname(char* const cname);

/*
 *   Limit pouch cgroup:
 *   - Limits given state object for given container name and limit
 *   @input: container_name, cgroup_state_obj, limitation
 *   @output: none
 *   @return: 0 - OK, <0 - FAILURE
 */
pouch_status pouch_limit_cgroup(const char* const container_name,
                                const char* const cgroup_state_obj,
                                const char* const limitation);
