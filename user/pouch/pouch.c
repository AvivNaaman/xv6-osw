#include "pouch.h"

#include "configs.h"
#include "container.h"
#include "fcntl.h"
#include "fs.h"
#include "image.h"
#include "lib/mutex.h"
#include "lib/user.h"
#include "ns_types.h"
#include "param.h"
#include "stat.h"

/*
 * Command line options
 */
#define POUCH_CMD_ARG_IMAGES "images"
#define POUCH_CMD_ARG_BUILD "build"

/*
 *   Init pouch cgroup:
 *   - Creates root cgroup dir if not exists and mounts cgroups fs
 *   @input: none
 *   @output: none
 *   @return: 0 - OK, <0 - FAILURE
 */
static pouch_status init_pouch_cgroup() {
  int cgroup_fd = -1;
  // check if cgoup filesystem already created
  if ((cgroup_fd = open("/cgroup", O_RDWR)) < 0) {
    if (mkdir("/cgroup") != 0) {
      printf(1, "Pouch: Failed to create root cgroup.\n");
      return MOUNT_CGROUP_FAILED_ERROR_CODE;
    }
    if (mount(0, "/cgroup", "cgroup") != 0) {
      printf(1, "Pouch: Failed to mount cgroup fs.\n");
      return MOUNT_CGROUP_FAILED_ERROR_CODE;
    }
  } else {
    if (close(cgroup_fd) < 0) return ERROR_CODE;
  }

  return SUCCESS_CODE;
}

/*
 *   Pouch cmd:
 *   - Pouch operation based on command type
 *   @input: container_name,image_name,pouch_file,p_cmd
 *   @output: none
 *   @return: 0 - OK, != 0 - FAILURE
 */
static pouch_status pouch_cmd(char* container_name, char* image_name,
                              enum p_cmd cmd) {
  switch (cmd) {
    case START:
      return pouch_container_start(container_name, image_name);
    case LIST:
      return print_clist();
    case IMAGES:
      return pouch_print_images();
    case INFO:
      return print_cinfo(container_name);
    case DESTROY:
      return pouch_container_stop(container_name);
    case CONNECT:
      return pouch_container_connect(container_name);
    case DISCONNECT:
      return pouch_container_disconnect(container_name);
    default:
      printf(stderr, "Pouch: Unknown command %d\n", cmd);
      return ERROR_CODE;
  }
}

static pouch_status print_help_inside_cnt() {
  int retval = 0;
  retval = printf(stderr, "\nPouch commands inside containers:\n");
  retval |= printf(stderr, "       pouch disconnect \n");
  retval |= printf(stderr,
                   "          : disconnect a currently connected container\n");
  retval |= printf(stderr, "       pouch info\n");
  retval |= printf(
      stderr,
      "          : query information about currently connected container\n");
  return retval;
}

void print_pouch_build_help() {
  printf(
      stderr,
      "       pouch build [--file filename=Pouchfile] [--tag Tag=default]\n");
  printf(
      stderr,
      "          : build a new pouch image using the specified parameters\n");
  printf(stderr,
         "          - {--file} : The pouch file name to use for building the "
         "container.\n");
  printf(stderr, "          - {--tag} : The tag to use for the output image\n");
}

void print_help_outside_cnt() {
  printf(stderr, "\nPouch commands outside containers:\n");
  printf(stderr, "       pouch start {name} {image}\n");
  printf(stderr, "          : starts a new container\n");
  printf(stderr, "          - {name} : container name\n");
  printf(stderr, "          - {image} : image name\n");
  printf(stderr, "       pouch connect {name}\n");
  printf(stderr, "          : connect already started container\n");
  printf(stderr, "          - {name} : container name\n");
  printf(stderr, "       pouch destroy {name}\n");
  printf(stderr, "          : destroy a container\n");
  printf(stderr, "          - {name} : container name\n");
  printf(stderr, "       pouch info {name}\n");
  printf(stderr, "          : query information about a container\n");
  printf(stderr, "          - {name} : container name\n");
  printf(stderr, "       pouch list all\n");
  printf(stderr, "          : displays state of all created containers\n");
  printf(stderr, "      \ncontainers cgroups:\n");
  printf(stderr, "       pouch cgroup {cname} {state-object} [value]\n");
  printf(stderr, "          : limit given cgroup state-object\n");
  printf(stderr, "          - {name} : container name\n");
  printf(stderr,
         "          - {state-object} : cgroups state-object. Refer spec.\n");
  printf(stderr,
         "          - [value] : argument for the state-object, multiple values "
         "delimited by ','\n");
  printf(stderr, "      \npouch images:\n");
  printf(stderr, "       pouch images\n");
  printf(stderr, "          : list pouch images in the system.\n");
  print_pouch_build_help();
}

static pouch_status pouch_build_parse_args(const int argc, char* argv[],
                                           char** const file_name,
                                           char** const tag) {
  char** options = &argv[2];
  /* Parse build options: --file, --tag */
  while (options < argv + argc) {
    if (strcmp(*options, "--file") == 0) {
      if (options + 1 >= argv + argc) {
        printf(stderr, "Error: Expected file name after --file\n");
        goto error;
      }
      if (*file_name) {
        printf(stderr, "Error: Specified more than one --file argument.\n");
        goto error;
      }
      *file_name = *(++options);
    } else if (strcmp(*options, "--tag") == 0) {
      if (options + 1 >= argv + argc) {
        printf(stderr, "Error: Expected tag name after --tag\n");
        goto error;
      }
      if (*tag) {
        printf(stderr, "Error: Specified more than one --tag argument.\n");
        goto error;
      }
      *tag = *(++options);
    } else {
      printf(stderr, "Error: Unexpected argument %s!\n", *options);
      goto error;
    }
    ++options;
  }
  return SUCCESS_CODE;

error:
  return ERROR_CODE;
}

int main(int argc, char* argv[]) {
  enum p_cmd cmd = START;
  char container_name[CNTNAMESIZE];
  char image_name[CNTNAMESIZE];

  // get parent pid
  int ppid = getppid();

  // check if any argument is --help and print help message accordingly:
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--help") == 0) {
      if (ppid == 1)
        print_help_inside_cnt();
      else
        print_help_outside_cnt();
      exit(0);
    }
  }

  if (argc >= 4) {
    int arg_len = strlen(argv[3]);
    if (arg_len > CNTNAMESIZE || arg_len == 0) {
      printf(stderr, "Error: Image name invalid, must be 1-%d chars, got %d.\n",
             CNTNAMESIZE, arg_len);
      exit(1);
    }
    strcpy(image_name, argv[3]);
  }
  if (argc >= 3) {
    int arg_len = strlen(argv[2]);
    if (arg_len > CNTNAMESIZE || arg_len == 0) {
      printf(stderr,
             "Error: Container name invalid, must be 1-%d chars, got %d.\n",
             CNTNAMESIZE, arg_len);
      exit(1);
    }
    strcpy(container_name, argv[2]);
  } else if (argc == 2) {
    if (strcmp(argv[1], POUCH_CMD_ARG_IMAGES) != 0 &&
        strcmp(argv[1], POUCH_CMD_ARG_BUILD) != 0) {
      if (ppid == 1 && get_connected_cname(container_name) < 0) {
        print_help_inside_cnt();
        exit(1);
      } else if (ppid != 1) {
        print_help_outside_cnt();
        exit(0);
      }
    }
  } else {
    if (ppid == 1)
      print_help_inside_cnt();
    else
      print_help_outside_cnt();
    exit(0);
  }
  // get command type
  if (argc >= 2) {
    if ((strcmp(argv[1], "start")) == 0 && argc == 4) {
      cmd = START;
    } else if ((strcmp(argv[1], "connect")) == 0) {
      cmd = CONNECT;
    } else if ((strcmp(argv[1], "disconnect")) == 0) {
      cmd = DISCONNECT;
      if (ppid != 1) {
        printf(1, "Pouch: no container is connected\n");
        exit(1);
      }
    } else if ((strcmp(argv[1], "destroy")) == 0) {
      cmd = DESTROY;
    } else if ((strcmp(argv[1], "cgroup")) == 0 && argc == 5) {
      cmd = LIMIT;
    } else if ((strcmp(argv[1], "info")) == 0) {
      cmd = INFO;
    } else if ((strcmp(argv[1], "list")) == 0 &&
               (strcmp(argv[2], "all")) == 0) {
      cmd = LIST;
    } else if ((strcmp(argv[1], POUCH_CMD_ARG_IMAGES)) == 0) {
      cmd = IMAGES;
    } else if ((strcmp(argv[1], POUCH_CMD_ARG_BUILD)) == 0) {
      cmd = BUILD;
    } else {
      if (ppid == 1)
        print_help_inside_cnt();
      else
        print_help_outside_cnt();
      exit(1);
    }

    if (init_pouch_cgroup() < 0) {
      printf(1, "Pouch: cgroup operation failed.\n");
      exit(1);
    }

    if (init_pouch_conf() < 0) {
      printf(1, "Pouch: operation failed.\n");
      exit(1);
    }

    // Inside the container the are only few commands permitted, disable others.
    if (ppid == 1 && cmd != LIMIT && cmd != DISCONNECT /* && cmd != LIST*/
        && cmd != INFO && cmd != IMAGES && cmd != BUILD) {
      if (cmd == START) {
        printf(1, "Nesting containers is not supported.\n");
        goto error_exit;
      } else if (cmd == CONNECT) {
        printf(1, "Nesting containers is not supported.\n");
        goto error_exit;
      } else if (cmd == DESTROY) {
        printf(1, "Container can't be destroyed while connected.\n");
        goto error_exit;
      } else if (cmd == LIST) {
        if (print_help_inside_cnt() < 0) {
          goto error_exit;
        }
      }
    } else {
      // command execution
      if (cmd == LIMIT && argc == 5) {
        if (pouch_limit_cgroup(container_name, argv[3], argv[4]) < 0) {
          goto error_exit;
        }
      } else if (cmd == BUILD) {
        char* pouch_file_name = NULL;
        char* image_tag = NULL;
        if (pouch_build_parse_args(argc, argv, &pouch_file_name, &image_tag) !=
            SUCCESS_CODE) {
          printf(stderr, "\n");
          print_pouch_build_help();
          goto error_exit;
        }
        if (pouch_build(pouch_file_name, image_tag) != SUCCESS_CODE) {
          goto error_exit;
        }
      } else if (pouch_cmd(container_name, image_name, cmd) < 0) {
        printf(1, "Pouch: operation failed.\n");
        goto error_exit;
      }
    }
  }
  exit(0);
error_exit:
  exit(1);
}
