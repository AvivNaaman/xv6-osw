#include "configs.h"

#include "container.h"
#include "fcntl.h"
#include "lib/user.h"
#include "param.h"

pouch_status init_pouch_conf() {
  // create a file for eacth tty that will hold cname inside currently connected
  // to it
  int i;
  int ttyc_fd;
  pouch_status status = SUCCESS_CODE;

  // Not including the console tty
  for (i = 0; i < (MAX_TTY - 1); i++) {
    char ttyc[] = "tty.cX";
    ttyc[5] = '0' + i;
    // check if cname ttys already created
    if (open(ttyc, O_RDWR) > 0) continue;
    if ((ttyc_fd = open(ttyc, O_CREATE | O_RDWR)) < 0) {
      printf(stderr, "cannot open %s fd\n", ttyc);
      status = TTY_OPEN_ERROR_CODE;
      goto end;
    }
    if (close(ttyc_fd) < 0) {
      printf(stderr, "cannot close %s fd\n", ttyc);
      status = TTY_CLOSE_ERROR_CODE;
      goto end;
    }
  }
end:
  return status;
}

pouch_status write_to_pconf(const char* const ttyname,
                            const char* const cname) {
  char ttyc[] = "tty.cX";
  int ttyc_fd;
  ttyc[5] = ttyname[4];
  if ((ttyc_fd = open(ttyc, O_CREATE | O_WRONLY)) < 0) {
    printf(stderr, "cannot open %s fd\n", ttyc);
    return TTY_OPEN_ERROR_CODE;
  }
  printf(ttyc_fd, "%s", cname);
  close(ttyc_fd);
  return SUCCESS_CODE;
}

pouch_status remove_from_pconf(char* ttyname) {
  char ttyc[] = "tty.cX";
  int ttyc_fd;
  ttyc[5] = ttyname[4];
  pouch_status status = SUCCESS_CODE;
  if ((ttyc_fd = open(ttyc, O_RDWR)) < 0) {
    printf(stderr, "cannot open %s fd\n", ttyc);
    status = TTY_OPEN_ERROR_CODE;
    goto end;
  }
  if (unlink(ttyc) < 0) {
    printf(stderr, "cannot unlink %s\n", ttyc);
    status = ERROR_CODE;
    goto end;
  }
  if ((ttyc_fd = open(ttyc, O_CREATE | O_RDWR)) < 0) {
    printf(stderr, "cannot open %s fd\n", ttyc);
    status = TTY_OPEN_ERROR_CODE;
  }

end:
  close(ttyc_fd);
  return status;
}

pouch_status container_name_by_tty(const char* const ttyname,
                                   char* const cname) {
  char ttyc[] = "/tty.cX";
  int ttyc_fd;
  ttyc[6] = ttyname[4];
  if ((ttyc_fd = open(ttyc, O_RDWR)) < 0) {
    printf(stderr, "cannot open %s fd\n", ttyc);
    return TTY_OPEN_ERROR_CODE;
  }
  read(ttyc_fd, cname, CNTNAMESIZE);
  close(ttyc_fd);
  return SUCCESS_CODE;
}

pouch_status read_from_cconf(const char* const container_name,
                             container_config* const conf) {
  char pid_str[sizeof("PPID:") + 10];
  int cont_fd = -1;

  char container_file[CNTNAMESIZE + sizeof("/")];
  strcpy(container_file, "/");
  strcat(container_file, container_name);
  cont_fd = open(container_file, 0);
  pouch_status status = SUCCESS_CODE;
  if (cont_fd < 0) {
    printf(stderr, "There is no container: %s in a started stage\n",
           container_name);
    status = FAILED_TO_OPEN_CCONF_ERROR_CODE;
    goto error;
  }

  if (read(cont_fd, conf->tty_name, sizeof("/ttyX")) < sizeof("/ttyX")) {
    printf(stderr, "CONT TTY NOT FOUND\n");
    status = INVALID_CCONF_ERROR_CODE;
    goto error;
  }

  conf->tty_name[sizeof("/ttyX") - 1] = 0;

  int i = 0;
  char c;
  while (read(cont_fd, &c, 1) > 0) {
    if (c == '\n') {
      pid_str[i] = 0;
      break;
    }
    pid_str[i++] = c;
  }
  pid_str[i] = '\0';

  // make sure it begins with PPID:
  if (strncmp(pid_str, "PPID:", sizeof("PPID:") - 1) != 0) {
    printf(stderr, "CONT PID NOT FOUND\n");
    status = INVALID_CCONF_ERROR_CODE;
    goto error;
  }

  // parse the pid
  conf->pid = atoi(pid_str + sizeof("PPID:"));

  // skip container name "NAME: container_name":
  i = 0;
  while (read(cont_fd, &c, 1) > 0) {
    if (c == '\n') {
      break;
    }
  }

  // read image name
  i = 0;
  while (read(cont_fd, &c, 1) > 0) {
    if (c == '\n' || i == CNTNAMESIZE - 1) {
      conf->image_name[i] = 0;
      break;
    }
    conf->image_name[i++] = c;
  }
  conf->image_name[i] = '\0';

  close(cont_fd);
  return SUCCESS_CODE;

error:
  if (cont_fd >= 0) close(cont_fd);
  return status;
}

pouch_status write_to_cconf(const container_config* conf) {
  if (conf == NULL) {
    printf(stderr, "conf is NULL\n");
    return INVALID_CCONF_TO_WRITE_ERROR_CODE;
  }
  if (strlen(conf->container_name) == 0) {
    printf(stderr, "container_name is empty\n");
    return INVALID_CCONF_TO_WRITE_ERROR_CODE;
  }
  if (strlen(conf->tty_name) == 0) {
    printf(stderr, "tty_name is empty\n");
    return INVALID_CCONF_TO_WRITE_ERROR_CODE;
  }
  if (conf->pid <= 0) {
    printf(stderr, "pid is %d <= 0!\n", conf->pid);
    return INVALID_CCONF_TO_WRITE_ERROR_CODE;
  }

  int cont_fd = open(conf->container_name, O_CREATE | O_RDWR);
  if (cont_fd < 0) {
    printf(stderr, "cannot open %s\n", conf->container_name);
    return FAILED_TO_OPEN_CCONF_ERROR_CODE;
  }
  printf(cont_fd, "%s\n%s %d\n%s %s\n%s %s\n", conf->tty_name, CONFIG_KEY_PPID,
         conf->pid, CONFIG_KEY_NAME, conf->container_name, CONFIG_KEY_IMAGE,
         conf->image_name);
  close(cont_fd);
  return SUCCESS_CODE;
}
