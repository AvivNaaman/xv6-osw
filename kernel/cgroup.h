#ifndef XV6_CGROUP_H
#define XV6_CGROUP_H

#include "defs.h"
#include "fs/vfs_file.h"
#include "param.h"
#include "proc.h"

/* Max length of string representation of descendants number. (the value is
 * a number of at most two digits + null terminator) */
#define MAX_DECS_SIZE 3

/* Max length of string representation of depth number. (the value is a
 * number of at most two digits + null terminator) */
#define MAX_DEPTH_SIZE 3

/* Max length allowed for controller names. */
#define MAX_CONTROLLER_NAME_LENGTH 16

/* major:minor format which takes at most 17 bytes (8 bytes for each uint value)
 */
#define DEVICE_NAME 17

typedef enum { CG_FILE, CG_DIR } cg_file_type;

/* cgroup's io device statistics structure, here we got all the relevant fields
    from the cgroup perspective and also the dev_stat structure which describes
    what status fields every IO device should have in the system.

    Note: this is a wrapper to the dev_stat so we can add more cgroup specific
    parameters and info which is not really relevant to the driver itself (like
   dev_name)
*/
typedef struct cgroup_io_device_statistics_s {
  char dev_name[DEVICE_NAME];
  uint major;
  uint minor;
  struct dev_stat device_stats;
} cgroup_io_device_statistics_t;

/*
 * Control group, contains up to NPROC processes.
 */
struct cgroup {
  /* Path of the cgroup directory. */
  char cgroup_dir_path[MAX_PATH_LENGTH];

  /* Reference count. */
  int ref_count;

  /* Array of all processes in the cgroup. */
  struct proc* proc[NPROC];
  /* Number of processes in the cgroup subtree
   * (including processes in this cgroup). */
  int num_of_procs;

  /* The parent cgroup. */
  struct cgroup* parent;

  /* Is 1 if cpu controller may be enabled, otherwise 0. */
  char cpu_controller_avalible;
  /* Is 1 if cpu controller is enabled, otherwise 0. */
  char cpu_controller_enabled;

  /* Is 1 if pid controller may be enabled, otherwise 0. */
  char pid_controller_avalible;
  /* Is 1 if pid controller is enabled, otherwise 0. */
  char pid_controller_enabled;

  /* Is 1 if cpu set controller may be enabled, otherwise 0. */
  char set_controller_avalible;
  /* Is 1 if cpu set controller is enabled, otherwise 0. */
  char set_controller_enabled;

  /* Is 1 if memory controller may be enabled, otherwise 0. */
  char mem_controller_avalible;
  /* Is 1 if memory controller is enabled, otherwise 0. */
  char mem_controller_enabled;

  /* Is 1 if subtree has at least one process in it, otherise 0. */
  char populated;

  /* Number of maximum descendant cgroups allowed in subtree. */
  unsigned int max_descendants_value;

  /* Number of maximum depth allowed in subtree. */
  unsigned int max_depth_value;

  /* Current depth of the cgroup. */
  unsigned int depth;

  /* Current number of descendant cgroups. */
  unsigned int nr_descendants;

  /* Current number of dying descendant cgroups. */
  unsigned int nr_dying_descendants;

  /* The maximum number of processes that are allowed in the cgroup.
   * Used by pid controller. */
  int max_num_of_procs;

  /* Which cpu id to use for cpu set controller. */
  uchar cpu_to_use;

  /* Indicates whether cgroup is frozen. */
  int is_frozen;

  /* The current amount of memory used by the group. */
  unsigned int current_mem;
  /* The current amount of memory used by the group in pages. */
  unsigned int current_page;
  /* Amount of cached filesystem data that was modified
   * but not yet written back to disk. */
  unsigned int mem_stat_file_dirty;
  /* Total number of cached filesystem data that was modified
   * and written back to disk. */
  unsigned int mem_stat_file_dirty_aggregated;
  /* Number of page faults incurred when the kernel does not needs to read
   * the data from disk. */
  unsigned int mem_stat_pgfault;
  /* Number of page faults incurred and the kernel actually needs to read
   * the data from disk. */
  unsigned int mem_stat_pgmajfault;

  /* The maximum memory allowed for a group to use.*/
  unsigned int max_mem;
  /* Amount of memory that protected for this cgroup.*/
  unsigned int min_mem;
  /* How meny pages of memory we need to protect for this group (e.g. min_mem -
   * current_page).*/
  unsigned int protected_mem;

  unsigned long long cpu_time;
  unsigned int cpu_period_time;
  unsigned int cpu_percent;
  unsigned int cpu_account_period;
  unsigned int cpu_time_limit;
  unsigned int cpu_account_frame;
  unsigned int cpu_nr_periods;
  unsigned int cpu_nr_throttled;
  unsigned int cpu_throttled_usec;
  char cpu_is_throttled_period;

  /* Used IO devices in a current cgroup (For example, attached tty). Updated on
   * io.stat read */
  unsigned int used_devices;

  /* IO statistics for each available IO in cgroup */
  cgroup_io_device_statistics_t io_stats[NDEV];
};

/**
 * This funciton returns the root cgroup.
 * Receives void.
 * Return value is a pointer to root cgroup, &cgroups[0].
 */
struct cgroup* cgroup_root(void);

/**
 * Lock the cgroup table.
 */
void cgroup_lock();

/**
 * Unlock the cgroup table.
 */
void cgroup_unlock();

/**
 * This function creates a new cgroup and initializes it at the given path.
 * Checks that cgroup has parent.
 * Checks that the limits set in ancestors are not exceeded.
 * Checks if we have avalible slot in the table.
 * Updates number of descendants for ancestors.
 * Receives string parameter "path".
 * "path" is string of directory names separated by '/'s.
 * Return value is pointer to the newly created cgroup.
 * On failure returns 0.
 */
struct cgroup* cgroup_create(char* path);

/**
 * This function deletes a cgroup, removes directory (when called with type
 * unlink) or unmounts the cgroup filesystem (when called on root cgroup with
 * type umount). If the reference count is not 0, the cgroup becomes dying.
 * Receives string parameter "path", string parameter "type".
 * "path" is string of directory names separated by '/'s.
 * "type" is string with value "umount" or "unlink".
 * Type must be "umount" when called from umount systemcall.
 * Type must be "unlink" when called from unlink systemcall.
 * Return values:
 * - RESULT_SUCCESS when deleted successfully.
 * - RESULT_ERROR_ARGUMENT when path is not a cgroup directory.
 * - RESULT_ERROR_OPERATION when cannot delete cgroup.
 */
result_code cgroup_delete(char* path, char* type);

/**
 * This function initializes a cgroup. (sets default starting values of new
 * cgroups) Receives cgroup pointer parameter "cgroup", string parameter "path",
 * cgroup pointer parameter "parent_cgroup". "cgroup" is pointer to the cgroup
 * to be initialized. "path" is string of directory names separated by '/'s.
 * "parent_cgroup" is pointer to the parent cgroup ("cgroup" will be initialized
 * as the child of this cgroup). Must be valid cgroup, the function does not
 * check validity. Return value is void.
 */
void cgroup_initialize(struct cgroup* cgroup, char* path,
                       struct cgroup* parent_cgroup);

/**
 * These functions insert a process into a cgroup (associates process with
 * cgroup). They erase the process from the previously associated cgroup. Unsafe
 * and safe versions of function (unsafe does not acquire cgroup table lock and
 * safe does). Receive cgroup pointer parameter "cgroup", proc pointer parameter
 * "proc". "cgroup" is pointer to the cgroup into which we insert the process.
 * Must be valid cgroup. "proc" is a pointer to the process to be inserted into
 * the cgroup. Must be valid process. Return values:
 * - RESULT_SUCCESS on success.
 * - RESULT_ERROR if no space.
 */
result_code unsafe_cgroup_insert(struct cgroup* cgroup, struct proc* proc);
result_code cgroup_insert(struct cgroup* cgroup, struct proc* proc);

/**
 * This function removes a process from a cgroup.
 * Process is dissociated with cgroup.
 * Receives cgroup pointer parameter "cgroup", proc pointer parameter "proc".
 * "cgroup" is pointer to the cgroup from which we remove the process. Must be
 * valid cgroup. "proc" is a pointer to the process to be removed from the
 * cgroup. Must be valid process. Return value is void.
 */
void cgroup_erase(struct cgroup* cgroup, struct proc* proc);

/**
 * This function update protect memory counter after moving process from src
 * cgroup to dst cgroup. Return: RESULT_SUCCESS for success. RESULT_ERROR if
 * there is no memory to protect for the "src" cgroup.
 */
result_code protect_memory(struct cgroup* src, struct cgroup* dst,
                           int proc_size);

/* TODO: add documentation */
int calc_dec_dst_protect_pg(struct cgroup* cgroup, int pg);
int calc_inc_src_protect_pg(struct cgroup* cgroup, int pg);

/**
 * These functions enable the cpu controller of a cgroup.
 * Unsafe and safe versions of function (unsafe does not acquire cgroup table
 * lock and safe does). Receives cgroup pointer parameter "cgroup". "cgroup" is
 * pointer to the cgroup in which we enable the controller. Must be valid
 * cgroup. Return values:
 * - RESULT_SUCCESS on success.
 * - RESULT_ERROR on failure.
 */
result_code unsafe_enable_cpu_controller(struct cgroup* cgroup);
result_code enable_cpu_controller(struct cgroup* cgroup);

/**
 * These functions disable the cpu controller of a cgroup.
 * Unsafe and safe versions of function (unsafe does not acquire cgroup table
 * lock and safe does). Receives cgroup pointer parameter "cgroup". "cgroup" is
 * pointer to the cgroup in which we disable the controller. Must be valid
 * cgroup. Return values:
 * - RESULT_SUCCESS on success.
 * - RESULT_ERROR on failure.
 */
result_code unsafe_disable_cpu_controller(struct cgroup* cgroup);
result_code disable_cpu_controller(struct cgroup* cgroup);

/**
 * This function sets the cgroup_dir_path field of a cgroup.
 * Receives cgroup pointer parameter "cgroup", string parameter "path".
 * "cgroup" is pointer to the cgroup in which we set the field.
 * "path" is string of directory names separated by '/'s. We set the
 * cgroup_dir_path field to this path. Return value is void.
 */
void set_cgroup_dir_path(struct cgroup* cgroup, char* path);

/**
 * This function gets the cgroup that is located at a given path.
 * Receives string parameter "path".
 * "path" is string of directory names separated by '/'s.
 * Return value is pointer to the cgroup located at the path "path".
 * On failure returns 0.
 */
struct cgroup* get_cgroup_by_path(char* path);

/**
 * This function sets the max_descendants_value field of a cgroup.
 * Receives cgroup pointer parameter "cgroup", int parameter "value".
 * "cgroup" is pointer to the cgroup in which we set the field.
 * "value" is an integer containing the new value to set. We set the
 * max_descendants_value field to this value. Return value is void.
 */
void set_max_descendants_value(struct cgroup* cgroup, unsigned int value);

/**
 * This function sets the max_depth_value field of a cgroup.
 * Receives cgroup pointer parameter "cgroup", string parameter "value".
 * "cgroup" is pointer to the cgroup in which we set the field.
 * "value" is an integer containing the new value to set. We set the
 * max_depth_value field to this value. Return value is void.
 */
void set_max_depth_value(struct cgroup* cgroup, unsigned int value);

/**
 * This function sets the nr_descendants field of a cgroup.
 * Receives cgroup pointer parameter "cgroup", string parameter "value".
 * "cgroup" is pointer to the cgroup in which we set the field.
 * "value" is an integer containing the new value to set. We set the
 * nr_descendants field to this value. Return value is void.
 */
void set_nr_descendants(struct cgroup* cgroup, unsigned int value);

/**
 * This function sets the nr_dying_descendants field of a cgroup.
 * Receives cgroup pointer parameter "cgroup", string parameter "value".
 * "cgroup" is pointer to the cgroup in which we set the field.
 * "value" is an integer containing the new value to set. We set the
 * nr_dying_descendants field to this value. Return value is void.
 */
void set_nr_dying_descendants(struct cgroup* cgroup, unsigned int value);

/**
 * This function gets all names of children cgroup of cgroup at a path.
 * Receives string parameter "buf", string parameter "path".
 * "buf" is string to be set to the names of all chilren cgroup of cgroup
 * located at the path. "buf" must be big enough to fit all of them. "path" is
 * string of directory names separated by '/'s. The function gets the names of
 * children of the cgroup located at this path. Return value is void.
 */
void get_cgroup_names_at_path(char* buf, char* path);

/**
 * This function gets number of cgroup's immidiate children.
 * Receives cgroup pointer parameter "cgroup".
 * "cgroup" is pointer to the cgroup of which we count the children.
 * Return value is the number of immidiate children of the cgroup.
 * On falure returns -1.
 */
int cgorup_num_of_immidiate_children(struct cgroup* cgroup);

/**
 * This function formats a path.
 * Receives string parameter "buf", string parameter "path".
 * "buf" is string to be set the formatted path. "buf" must be big enough to fit
 * it. "path" is string of directory names separated by '/'s. Return value is
 * void. The function does the following: 1) If "path" does not start with '/'
 * then it copies the current working directory to the buffer, 2) Copies "path"
 * to buffer while changing "/.." and "/." to the appropriate paths. 3) '/'-es
 * at the end of "path" are not copied. 4) If the buffer ends with '/' then it
 * is deleted.
 *
 * Example:
 * for given path = "b/./c/d/../e/////" and cwd = "/a"
 * buf will be set to : "a/b/c/e"
 */
void format_path(char* buf, char* path);

/**
 * This function decrements number of dying descendants for a cgroup and every
 * ancestor.
 * * Receives cgroup pointer parameter "cgroup".
 * "cgroup" is pointer to the cgroup of which number of dying descendants we
 * decrement. Return value is void.
 */
void decrement_nr_dying_descendants(struct cgroup* cgroup);

/**
 * Safe implementation of cgroup file manipulation functions defined in cgfs.c.
 * (Implementation with locks)
 */

/**
 * This function is a lock protected version of the corresponding unsafe
 * function unsafe_cg_open() defined in cgfs.h.
 */
int cg_open(cg_file_type type, char* filename, struct cgroup* cgp, int omode);

/**
 * This function is a lock protected version of the corresponding unsafe
 * function unsafe_cg_read() defined in cgfs.h.
 */
int cg_read(cg_file_type type, struct vfs_file* f, char* addr, int n);

/**
 * This function is a lock protected version of the corresponding unsafe
 * function unsafe_cg_write() defined in cgfs.h.
 */
int cg_write(struct vfs_file* f, char* addr, int n);

/**
 * This function is a lock protected version of the corresponding unsafe
 * function unsafe_cg_close() defined in cgfs.h.
 */
int cg_close(struct vfs_file* file);

/**
 * This function is a lock protected version of the corresponding unsafe
 * function unsafe_cg_stat() defined in cgfs.h.
 */
int cg_stat(struct vfs_file* f, struct stat* st);

/**
 * This function opens cgroup file or directory. Meant to be called in
 * sys_open(). Receives string parameter "path", integer parameter "omode".
 * "path" is string of directory names separated by '/'s, the path of the file
 * to open. "omode" is the opening mode. Same as with regular files. Return
 * values: -1 on failure. file descriptor of the new open file or directory on
 * success.
 */
int cg_sys_open(char* path, int omode);

/**
 * This function sets the maximum number of processes.
 * Receives cgroup pointer parameter "cgroup" and integer "limit".
 * Sets the number of maximum allowed processes in the cgroup to be "limit".
 * Returns:
 * - RESULT_SUCCESS_OPERATION upon successes.
 * - RESULT_SUCCESS if no action taken.
 * - RESULT_ERROR upon failure.
 */
result_code set_max_procs(struct cgroup* cgp, int limit);

/**
 * These functions enables the pid controller of a cgroup.
 * Unsafe and safe versions of function (unsafe does not acquire cgroup table
 * lock and safe does). Receives cgroup pointer parameter "cgroup". "cgroup" is
 * pointer to the cgroup in which we enable the controller. Must be valid
 * cgroup. Return values:
 * - RESULT_SUCCESS on success.
 * - RESULT_ERROR on failure.
 */
result_code unsafe_enable_pid_controller(struct cgroup* cgroup);
result_code enable_pid_controller(struct cgroup* cgroup);

/**
 * These functions disable the pid controller of a cgroup.
 * Unsafe and safe versions of function (unsafe does not acquire cgroup table
 * lock and safe does). Receives cgroup pointer parameter "cgroup". "cgroup" is
 * pointer to the cgroup in which we disable the controller. Must be valid
 * cgroup. Return values:
 * - RESULT_SUCCESS on success.
 * - RESULT_ERROR on failure.
 */
result_code unsafe_disable_pid_controller(struct cgroup* cgroup);
result_code disable_pid_controller(struct cgroup* cgroup);

/**
 * This function sets the cpu id to use.
 * Receives cgroup pointer parameter "cgroup" and integer "cpuid".
 * Sets the cpu id to "cpuid" on which the cgroup has to run.
 * Returns:
 * - RESULT_SUCCESS_OPERATION upon successes.
 * - RESULT_SUCCESS if no action taken.
 * - RESULT_ERROR upon failure.
 */
result_code set_cpu_id(struct cgroup* cgroup, int cpuid);

/**
 * These functions enables the cpu id controller of a cgroup.
 * Unsafe and safe versions of function (unsafe does not acquire cgroup table
 * lock and safe does). Receives cgroup pointer parameter "cgroup". "cgroup" is
 * pointer to the cgroup in which we enable the controller. Must be valid
 * cgroup. Return values:
 * - RESULT_SUCCESS on success.
 * - RESULT_ERROR on failure.
 */
result_code unsafe_enable_set_controller(struct cgroup* cgroup);
result_code enable_set_controller(struct cgroup* cgroup);

/**
 * These functions disable the cpu id controller of a cgroup.
 * Unsafe and safe versions of function (unsafe does not acquire cgroup table
 * lock and safe does). Receives cgroup pointer parameter "cgroup". "cgroup" is
 * pointer to the cgroup in which we disable the controller. Must be valid
 * cgroup. Return values:
 * - RESULT_SUCCESS on success.
 * - RESULT_ERROR on failure.
 */
result_code unsafe_disable_set_controller(struct cgroup* cgroup);
result_code disable_set_controller(struct cgroup* cgroup);

/**
 * This function freezes/unfreezes a cgroup.
 * Receives cgroup pointer parameter "cgroup" and integer "frz".
 * If frz=1 then freezes the group, otherwise, if 0 then unfreezes. All other
 * values result in no action. Returns:
 * - RESULT_SUCCESS_OPERATION upon successes.
 * - RESULT_SUCCESS if no action taken.
 * - RESULT_ERROR upon failure.
 */
result_code frz_grp(struct cgroup* cgroup, int frz);

/**
 *This function sets the maximum amount of memory.
 *Receives cgroup pointer parameter "cgroup" and integer "limit".
 *Sets the maximum allowed amount of memory in the cgroup to be "limit".
 *Returns:
 * - RESULT_SUCCESS_OPERATION upon successes.
 * - RESULT_SUCCESS if no action taken.
 * - RESULT_ERROR upon failure.
 */
result_code set_max_mem(struct cgroup* cgp, unsigned int limit);

/**
 *This function sets the minimum amount of memory.
 *Receives cgroup pointer parameter "cgroup" and integer "limit".
 *Sets the minimum amount of memory in the cgroup to be "limit".
 *calls set_protect_mem that ask mmu to protecet pages for cgroup.
 *Returns:
 * - RESULT_SUCCESS_OPERATION upon successes.
 * - RESULT_SUCCESS if no action taken.
 * - RESULT_ERROR upon failure.
 */
result_code set_min_mem(struct cgroup* cgp, unsigned int limit);

/**
 *This function sets the amount of memory pages that protected for cgroup.
 *Receives cgroup pointer parameter "cgroup" and integer "pages".
 *Increas/decreas the number of protected memory by "pages".
 *Returns:
 * - RESULT_SUCCESS_OPERATION upon successes.
 * - RESULT_ERROR upon failure.
 */
result_code set_protect_mem(struct cgroup* cgroup, unsigned int pages);

/**
 * These functions enables the memory controller of a cgroup.
 * Unsafe and safe versions of function (unsafe does not acquire cgroup table
 * lock and safe does). Receives cgroup pointer parameter "cgroup". "cgroup" is
 * pointer to the cgroup in which we enable the controller. Must be valid
 * cgroup. Return values:
 * - RESULT_SUCCESS on success.
 * - RESULT_ERROR on failure.
 */
result_code unsafe_enable_mem_controller(struct cgroup* cgroup);
result_code enable_mem_controller(struct cgroup* cgroup);

/**
 * These functions disable the memory controller of a cgroup.
 * Unsafe and safe versions of function (unsafe does not acquire cgroup table
 * lock and safe does). Receives cgroup pointer parameter "cgroup". "cgroup" is
 * pointer to the cgroup in which we disable the controller. Must be valid
 * cgroup. Return values:
 * - RESULT_SUCCESS on success.
 * - RESULT_ERROR on failure.
 */
result_code unsafe_disable_mem_controller(struct cgroup* cgroup);
result_code disable_mem_controller(struct cgroup* cgroup);

/**
 * @brief Increments the cgroup Memory Controller stat of file_dirty
 *
 * @param cgroup pointer to a cgroup
 */
void cgroup_mem_stat_file_dirty_incr(struct cgroup* cgroup);

/**
 * @brief Decrements the cgroup Memory Controller stat of file_dirty
 *
 * @param cgroup pointer to a cgroup
 */
void cgroup_mem_stat_file_dirty_decr(struct cgroup* cgroup);

/**
 * @brief Increments the cgroup Memory Controller stat of file_dirty_aggregated
 *
 * @param cgroup pointer to a cgroup
 */
void cgroup_mem_stat_file_dirty_aggregated_incr(struct cgroup* cgroup);

/**
 * @brief Increments the cgroup Memory Controller stat of pgfault
 *
 * @param cgroup pointer to a cgroup
 */
void cgroup_mem_stat_pgfault_incr(struct cgroup* cgroup);

/**
 * @brief Increments the cgroup Memory Controller stat of pgmajfault
 *
 * @param cgroup pointer to a cgroup
 */
void cgroup_mem_stat_pgmajfault_incr(struct cgroup* cgroup);

/* TODO: add documentation */
void get_cgroup_io_stat(struct vfs_file* f, struct cgroup* cgp);
void set_cgroup_io_stat(struct vfs_file* f);

#endif /* XV6_CGROUP_H */
