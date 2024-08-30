#include "image.h"

#include "fcntl.h"
#include "fs.h"
#include "lib/user.h"
#include "param.h"
#include "pouch.h"
#include "stat.h"
#include "util.h"

#define LINE_BUFFER_SIZE 1024
/*
 *   Pouchfile functions
 */
static pouch_status next_line(const int pouchfile_fd, char** const line,
                              int* const count) {
  pouch_status exit_code = SUCCESS_CODE;
  *count = 0;
  *line = (char*)malloc(sizeof(char) * LINE_BUFFER_SIZE);
  if (*line == NULL) {
    return ERROR_OUT_OF_MEMORY_CODE;
  }

  int allocated_line_length = LINE_BUFFER_SIZE;
  char current = 0;
  int index = 0;

  int read_ret = 0;
  bool started_read = false;
  while ((read_ret = read(pouchfile_fd, &current, 1)) > 0) {
    /* Skip initial whitespace */
    if (!started_read) {
      if (isspace(current)) {
        continue;
      }
      started_read = true;
    }

    if (current == '\n' || current == '\0') {
      break;
    }

    (*line)[index] = current;
    ++index;

    if (index == allocated_line_length) {
      const int last_line_length = allocated_line_length;
      allocated_line_length += LINE_BUFFER_SIZE;
      char* new_line = (char*)malloc(sizeof(char) * allocated_line_length);
      if (new_line == NULL) {
        exit_code = ERROR_OUT_OF_MEMORY_CODE;
        goto exit_error;
      }

      memmove(new_line, *line, last_line_length);
      free(*line);
      *line = new_line;
    }
  }

  if (read_ret == 0) {
    exit_code = END_OF_FILE_CODE;
  }
  (*line)[index] = '\0';
  *count = index;
  return exit_code;

exit_error:
  free(*line);
  *line = NULL;
  return ERROR_CODE;
}

static pouch_status pouchfile_init(struct pouchfile** const pouchfile,
                                   const char* const image_name) {
  *pouchfile = NULL;

  if (image_name == NULL) {
    printf(stderr, "Empty image name!\n");
    goto error_image_name;
  }

  int image_name_length = strlen(image_name);
  if (image_name_length == 0) {
    printf(stderr, "Empty image name!\n");
    goto error_image_name;
  }

  *pouchfile = (struct pouchfile*)malloc(sizeof(struct pouchfile));
  if (*pouchfile == NULL) {
    printf(stderr, "Pouchfile struct alloc failed!\n");
    return ERROR_OUT_OF_MEMORY_CODE;
  }

  (*pouchfile)->image_name =
      (char*)malloc(sizeof(char) * (image_name_length + 1));
  if ((*pouchfile)->image_name == NULL) {
    printf(stderr, "Image name alloc failed!\n");
    goto error_image_name;
  }

  strcpy((*pouchfile)->image_name, image_name);
  (*pouchfile)->commands_list_head = NULL;
  return SUCCESS_CODE;

error_image_name:
  free(pouchfile);
  return ERROR_INVALID_IMAGE_NAME_CODE;
}

static void pouchfile_destroy(struct pouchfile** const pouchfile) {
  free((*pouchfile)->image_name);
  struct pouchfile_command* current_command = (*pouchfile)->commands_list_head;
  while (current_command != NULL) {
    struct pouchfile_command* const to_free = current_command;
    current_command = current_command->next;
    free(to_free->command);
    free(to_free);
  }

  *pouchfile = NULL;
}

static pouch_status pouchfile_add_command(struct pouchfile* const pouchfile,
                                          const char* const command_string) {
  const int new_command_length = strlen(command_string);
  if (new_command_length == 0) {
    return SUCCESS_CODE;
  }

  struct pouchfile_command* const new_command =
      (struct pouchfile_command*)malloc(sizeof(struct pouchfile_command));
  if (new_command == NULL) {
    return ERROR_CODE;
  }

  char* const new_command_string =
      (char*)malloc(sizeof(char) * (new_command_length + 1));
  if (new_command_string == NULL) {
    goto error_new_command;
  }

  strcpy(new_command_string, command_string);
  new_command->command = new_command_string;
  new_command->next = NULL;
  if (pouchfile->commands_list_head == NULL) {
    pouchfile->commands_list_head = new_command;
  } else {
    struct pouchfile_command* current = pouchfile->commands_list_head;
    while (current->next != NULL) {
      current = current->next;
    }

    current->next = new_command;
  }

  return SUCCESS_CODE;

error_new_command:
  free(new_command);
  return ERROR_CODE;
}

static char* pouchfile_skip_cmd(char* line, const char* pouchfile_token) {
  char* const end_of_original_line = line + strlen(line);

  char* next_token = strtok_r(line, " \t\n\r\f", NULL);
  if (next_token == NULL) {
    return NULL;
  }
  if (strcmp(next_token, pouchfile_token) == 0) {
    char* to_return = next_token + strlen(next_token);
    // Skip whitespace after token, if there are some.
    while (to_return < end_of_original_line && isspace(*to_return)) {
      ++to_return;
    }
    if (to_return < end_of_original_line)
      return to_return + 1;
    else
      return to_return;
  }

  return NULL;
}

static pouch_status pouch_pouchfile_parse(const char* const pouchfile_path,
                                          struct pouchfile** const pouchfile) {
  int exit_code = SUCCESS_CODE;
  const int pouchfile_fd = open(pouchfile_path, O_RDONLY);
  if (pouchfile_fd < 0) {
    printf(stderr, "Failed to open pouchfile %s\n", pouchfile_path);
    return ERROR_CODE;
  }

  char* import_line = NULL;
  int import_line_length = 0;
  enum POUCH_INTERNAL_STATUS_CODES read_line_code = SUCCESS_CODE;

  /* Extract import, skip empty lines, break on end-of-file. */
  while (read_line_code == SUCCESS_CODE) {
    if ((read_line_code = next_line(pouchfile_fd, &import_line,
                                    &import_line_length)) == ERROR_CODE) {
      printf(stderr, "Failed to read import line from Pouchfile\n");
      goto image_line_error;
    }
    if (import_line_length > 0) {
      break;
    }
  }

  if (import_line_length == 0) {
    printf(stderr, "No import line found in Pouchfile\n");
    exit_code = ERROR_CODE;
    goto pouchfile_creation_error;
  }

  char* const image_name_start =
      pouchfile_skip_cmd(import_line, POUCHFILE_IMPORT_TOKEN);
  if (image_name_start == NULL) {
    printf(stderr,
           "Failed to find import directive in first line of Pouchfile: %s\n",
           import_line);
    exit_code = ERROR_CODE;
    goto pouchfile_creation_error;
  }
  if (*image_name_start == '\0' ||
      import_line + import_line_length < image_name_start) {
    printf(stderr,
           "Failed to find image name for import directive in first line of "
           "Pouchfile: %s\n",
           import_line);
    exit_code = ERROR_CODE;
    goto pouchfile_creation_error;
  }

  if (pouchfile_init(pouchfile, image_name_start) == ERROR_CODE) {
    printf(stderr, "Failed to init pouchfile struct\n");
    exit_code = ERROR_CODE;
    goto pouchfile_creation_error;
  }

  free(import_line);
  import_line = NULL;

  char* run_command_line = NULL;
  int run_command_line_length = 0;
  int extract_line_status = SUCCESS_CODE;
  while (extract_line_status == SUCCESS_CODE) {
    if ((extract_line_status = next_line(pouchfile_fd, &run_command_line,
                                         &run_command_line_length)) ==
        ERROR_CODE) {
      printf(stderr, "Failed to extract run line from Pouchfile\n");
      exit_code = ERROR_CODE;
      goto pouch_commands_error;
    }

    if (run_command_line_length == 0) {
      goto skip_line;
    }

    char* const run_command_start =
        pouchfile_skip_cmd(run_command_line, POUCHFILE_RUN_TOKEN);
    if (run_command_start == NULL) {
      printf(stderr,
             "Failed to find run directive in first line of Pouchfile\n");
      exit_code = ERROR_CODE;
      goto pouch_commands_add_error;
    }
    if (*run_command_start == '\0') {
      printf(stderr,
             "Failed to find run argument in first line of Pouchfile\n");
      exit_code = ERROR_CODE;
      goto pouch_commands_error;
    }

    if (pouchfile_add_command(*pouchfile, run_command_start) == ERROR_CODE) {
      exit_code = ERROR_CODE;
      goto pouch_commands_error;
    }

  skip_line:
    free(run_command_line);
  }
  run_command_line = NULL;

pouch_commands_add_error:
  free(run_command_line);
pouch_commands_error:
  if (exit_code != SUCCESS_CODE) {
    pouchfile_destroy(pouchfile);
  }
pouchfile_creation_error:
  free(import_line);
image_line_error:
  close(pouchfile_fd);
  return exit_code;
}

pouch_status pouch_build(const char* file_name, const char* tag) {
  if (!tag) {
    tag = "default";  // todo: getcwd?
  }
  if (!file_name) {
    file_name = "Pouchfile";
  }
  printf(stderr, "Building pouch image from \"%s\" to tag \"%s\"...\n",
         file_name, tag);
  struct pouchfile* pouchfile = NULL;
  if (pouch_pouchfile_parse(file_name, &pouchfile) == ERROR_CODE) {
    printf(stderr, "Error parsing Pouchfile %s\n", file_name);
    return ERROR_CODE;
  }
  // Future: Implement image construction!
  (void)pouchfile;

  pouchfile_destroy(&pouchfile);
  printf(stderr, "Built image to tag \"%s\".\n", tag);
  return SUCCESS_CODE;
}

pouch_status pouch_print_images() {
  char buf[MAX_PATH_LENGTH], *p;
  char dir[MAX_PATH_LENGTH];
  int fd;
  struct dirent de;
  struct stat st;

  if ((fd = open(IMAGE_DIR, 0)) < 0) {
    printf(stderr, "Cannot access the images dir, make sure the path %s exists\n",
           IMAGE_DIR);
    return ERROR_IMAGE_DIR_INVALID_CODE;
  }

  if (fstat(fd, &st) < 0) {
    printf(stderr, "cannot stat the images dir, make sure the path %s exists \n",
           IMAGE_DIR);
    close(fd);
    return ERROR_IMAGE_NOT_FOUND_CODE;
  }

  bool printed_first = false;
  if (st.type != T_DIR) {
    printf(stderr, "%s should be a directory\n", IMAGE_DIR);
    return ERROR_IMAGE_INVALID_CODE;
  }

  strcpy(buf, IMAGE_DIR);
  p = buf + strlen(buf);
  *p++ = '/';
  while (read(fd, &de, sizeof(de)) == sizeof(de)) {
    if (de.inum == 0) continue;
    memmove(p, de.name, DIRSIZ);
    p[DIRSIZ] = 0;
    if (stat(buf, &st) < 0) {
      printf(stdout, "Cannot stat %s\n", buf);
      continue;
    }
    strcpy(dir, fmtname(buf));
    if (strncmp(dir, ".", 1) != 0) {
      if (!printed_first) {
        printf(stdout, "Pouch images available:\n");
        printed_first = true;
      }
      printf(stdout, "%s\n", dir);
    }
  }

  if (!printed_first) {
    printf(stdout, "No images available\n");
  }

  close(fd);
  return SUCCESS_CODE;
}

pouch_status image_exists(const char* const image_name) {
  char buf[MAX_PATH_LENGTH];
  struct stat st;

  image_name_to_path(image_name, buf);
  if (stat(buf, &st) < 0) {
    return ERROR_IMAGE_NOT_FOUND_CODE;
  }
  return SUCCESS_CODE;
}

pouch_status image_name_to_path(const char* image_name, char* image_path) {
  if (strlen(image_name) + strlen(IMAGE_DIR) > MAX_PATH_LENGTH) {
    perror("Image name is too long");
    return ERROR_IMAGE_NAME_TOO_LONG_CODE;
  }
  strcpy(image_path, IMAGE_DIR);
  strcat(image_path, image_name);
  return SUCCESS_CODE;
}
