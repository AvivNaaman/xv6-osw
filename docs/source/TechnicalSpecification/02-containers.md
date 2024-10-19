# Containers - Pouch
## TTY Devices
Upon xv6 boot 3 tty devices are created with mknod syscall. All devices are controlled by the same device driver and have a common major device number. The major number is the offset into the kernel’s device driver table, which tells the kernel what kind of device driver to use. The minor number tells the kernel special characteristics of the device to be accessed. 
We can recognize each device by its major number (which identifies the driver and devices class) and by minor number which identifies the device itself. In other words every device driver is identified by ordered pair major:minor.

As it was mentioned already 3 tty devices were added. Tty devices are initialized in main.c by `ttyinit()`. 

`ttywrite()` and `ttyread()` functions are wrappers of the existing `consoleread()` and `consolewrite()` functions. They implement actual writing and reading to the user terminal for the tty that is active at the moment. `flags` property added to `devsw` to support different operations on tty devices.

Operations on tty devices were defined in fcntl.h and the corresponding functions are implemented in tty.c, and can be listen in fcntl.h:
```c
// ioctl tty command types
#define DEV_CONNECT 0x1000
#define DEV_DISCONNECT 0x2000
#define DEV_ATTACH 0x0010
#define DEV_DETACH 0x0020

// ioctl tty requests types
#define TTYSETS 0x0001
#define TTYGETS 0x0002
```
The `ioctl` syscall was added to control tty devices allowing to connect / disconnect / attach / detach tty devices as long as to set/get their properties.

## Config files
Containers in xv6 are identified by name (at least on this stage). The identifier name is used for a container specification by (almost) all the commands that pouch utility supply. pouch stores all it's configurations and state in the `/pouch` directory. 

Every container started with the pouch utility has a file names `/pconf/name` where the `name` corresponds to the container identification string as it was specified at the container creation stage. This file contains multiple lines with the configuration of the container, including what tty device is attached to the container, what is the PID of the process that forked the shell running inside the container, the container's name and the container's image name. For example, runnning the `pouch start ca a` command right after system startup, results in the following contents of the `/pconf/ca` file:
```
TTYNUM: 0
PPID: 6
NAME: ca
IMAGE: a
```

Additionally, `/pconf/tty.c[0-2]` files specify a container identification string of the container that is tied to the corresponding tty device. If no container is tied to the tty device, the file is empty. Hence, listing the `/pconf` directory after the above command results in the following:
```sh
$ ls /pconf/
.              1 44 96
..             1 1 1024
tty.c0         2 45 2
tty.c1         2 46 0
tty.c2         2 47 0
ca             2 55 36
```
The contents of the `/pconf/tty.c0` file are:
```sh
$ cat pconf/tty.c0
ca$
```
and the contents of the `/pconf/tty.c1` and `/pconf/tty.c2` files are empty.

## Cgroup usage
Processes running inside xv6 containers are organized by the pouch utility in a flat cgroup hierarchy.  Pouch mounts cgroup fs on the `/cgroup` mountpoint, creates a directory `/cgroup/name` for a container identified by the name identification string and takes advantage of the cgroups mechanism control means to allocate resources for the processes running inside the container. The directory is removed from the croup hierarchy when the container is destroyed. Pouch utility is limiting the container hierarchy to be flat i.e nesting is not supported. The layout of `/cgroup` is depicted on below in continuation of the previous example (ca):
```sh
$ ls /cgroup/
### TODO: Complete!
$ ls /cgroup/ca/
### TODO: Complete!
```

## Images management
Images in xv6 are stored in the `/images/` directory. An xv6 pouch image is a native FS image that contains the root filesystem of the container -- like `internal_fs_a[a-c]`.

## Commands implemnentation