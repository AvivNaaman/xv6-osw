#include "pouch.h"

#define POUCHFILE_IMPORT_TOKEN "IMPORT"
#define POUCHFILE_RUN_TOKEN "RUN"

/**
 * defines a single command in a pouchfile.
 */
struct pouchfile_command {
  char* command;
  struct pouchfile_command* next;
};

/**
 * defines a parsed pouchfile to be built.
 */
struct pouchfile {
  char* image_name;
  struct pouchfile_command* commands_list_head;
};

pouch_status pouch_build(const char* file_name, const char* tag);
/*
 *   Get all avaliable images
 *   @input: none
 *   @output: prints all avaliable images
 */
pouch_status pouch_print_images();

/*
 *   Check if image exists in images list by it's name
 *   @input: image_name
 *   @output: none
 */
pouch_status image_exists(const char* const image_name);

/*
 *   Prepare image name to path:
 *   - Create a path in cgroup fs for corresponding cname
 *   @input: image_name
 *   @output: image_path
 */
pouch_status image_name_to_path(const char* image_name, char* image_path);
