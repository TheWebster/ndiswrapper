/*
 *  Copyright (C) 2003-2005 Pontus Fuchs, Giridhar Pemmasani
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <libgen.h>

#include <sys/mman.h>

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <ctype.h>
#include <dirent.h>
#include <syslog.h>
#include <stdlib.h>

#include <linux/major.h>
#include <linux/ioctl.h>

#include "loader.h"

#define PROG_NAME "loadndisdriver"

#define SETTING_LEN (MAX_SETTING_NAME_LEN + MAX_SETTING_VALUE_LEN + 2)

static const char confdir[] = DRIVER_CONFIG_DIR;
static const char ioctl_file[] = "/dev/" DRIVER_NAME;
static int debug;

#ifndef UTILS_VERSION
#error "compile this file with 'make' in the 'utils' directory only"
#endif

#define LOG_MSG(where, fmt, ...)					\
	syslog(LOG_KERN | where, "%s: %s(%d): " fmt "\n",		\
	       PROG_NAME, __func__, __LINE__ , ## __VA_ARGS__)
#define ERROR(fmt, ...) LOG_MSG(LOG_INFO, fmt, ## __VA_ARGS__)
#define INFO(fmt, ...) LOG_MSG(LOG_INFO, fmt, ## __VA_ARGS__)
#define DBG(fmt, ...) do {					\
		if (debug > 0)					\
			LOG_MSG(LOG_INFO, fmt, ## __VA_ARGS__); \
	} while (0)
#define WARN(fmt, ...) LOG_MSG(LOG_INFO, fmt, ## __VA_ARGS__)

/* load .sys or .bin file */
static int load_file(char *filename, struct load_driver_file *driver_file)
{
	int fd;
	size_t size;
	void *image = NULL;
	struct stat statbuf;

	char *file_basename = basename(filename);

	fd = open(filename, O_RDONLY);
	if (fd == -1) {
		ERROR("unable to open file: %s", strerror(errno));
		return -EINVAL;
	}

	if (fstat(fd, &statbuf)) {
		ERROR("incorrect driver file '%s'", filename);
		close(fd);
		return -EINVAL;
	}
	size = statbuf.st_size;

	image = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (image == MAP_FAILED) {
		ERROR("unable to mmap driver: %s", strerror(errno));
		close(fd);
		return -EINVAL;
	}

	strncpy(driver_file->name, file_basename, sizeof(driver_file->name));
	driver_file->name[sizeof(driver_file->name)-1] = 0;
	driver_file->size = size;
	driver_file->data = image;
	return 0;
}

/* split setting into name and value pair */
static int parse_setting_line(const char *setting_line, char *setting_name,
			      char *setting_val)
{
	const char *s;
	char *val, *end;
	int i;

	/* We try to be really paranoid parsing settings */
	for (s = setting_line; isspace(*s); s++)
		;

	/* ignore comments and blank lines */
	if (*s == '#' || *s == ';' || *s == '\0')
		return 0;

	val = strchr(s, '|');
	end = strchr(s, '\n');
	if (val == NULL || end == NULL) {
		ERROR("invalid setting: %s", setting_line);
		return -EINVAL;
	}
	for (i = 0; s != val && i < MAX_SETTING_NAME_LEN; s++, i++)
		setting_name[i] = *s;
	setting_name[i] = 0;
	if (*s != '|') {
		ERROR("invalid setting: %s", setting_line);
		return -EINVAL;
	}

	for (i = 0, s++; s != end && i < MAX_SETTING_VALUE_LEN ; s++, i++)
		setting_val[i] = *s;
	setting_val[i] = 0;
	if (*s != '\n') {
		ERROR("invalid setting: %s", setting_line);
		return -EINVAL;
	}
	DBG("Found setting: name=%s, val=\"%s\"", setting_name, setting_val);

	/* setting_val can be empty, but not name */
	if (strlen(setting_name) == 0) {
		ERROR("invalid setting: \"%s\"", setting_line);
		return -EINVAL;
	}

	return 1;
}

/* read .conf file and store info in driver */
static int read_conf_file(char *conf_file_name, struct load_driver *driver)
{
	char setting_line[SETTING_LEN];
	struct stat statbuf;
	FILE *config;
	char setting_name[MAX_SETTING_NAME_LEN];
	char setting_value[MAX_SETTING_VALUE_LEN];
	int ret, num_settings;
	int vendor, device, subvendor, subdevice, bus;

	if (lstat(conf_file_name, &statbuf)) {
		ERROR("unable to open config file %s: %s",
		      conf_file_name, strerror(errno));
		return -EINVAL;
	}

	if (sscanf(conf_file_name, "%04X:%04X.%X.conf",
		   &vendor, &device, &bus) == 3) {
		DBG("bus: %X", bus);
	} else if (sscanf(conf_file_name, "%04X:%04X:%04X:%04X.%X.conf",
			  &vendor, &device, &subvendor, &subdevice,
			  &bus) == 5) {
		DBG("bus: %X", bus);
	} else {
		ERROR("unable to parse conf file name %s", conf_file_name);
		return -EINVAL;
	}

	num_settings = 0;
	driver->num_settings = 0;

	config = fopen(conf_file_name, "r");
	if (config == NULL) {
		ERROR("unable to open config file: %s", strerror(errno));
		return -EINVAL;
	}
	while (fgets(setting_line, SETTING_LEN-1, config)) {
		struct load_device_setting *setting;

		setting_line[SETTING_LEN-1] = 0;
		ret = parse_setting_line(setting_line, setting_name,
					 setting_value);
		if (ret == 0)
			continue;
		if (ret < 0)
			return -EINVAL;

		setting = &driver->settings[num_settings];
		strncpy(setting->name, setting_name, MAX_SETTING_NAME_LEN);
		strncpy(setting->value, setting_value, MAX_SETTING_VALUE_LEN);

		num_settings++;
		if (num_settings >= MAX_DEVICE_SETTINGS) {
			ERROR("too many settings");
			return -EINVAL;
		}

	}

	fclose(config);

	driver->num_settings = num_settings;
	return 0;
}

static int load_bin_file(int ioctl_device, char *driver_name, char *file_name)
{
	struct load_driver_file driver_file;
	char lc_file_name[MAX_DRIVER_NAME_LEN];
	int i;

	DBG("loading driver %s", driver_name);
	for (i = 0; file_name[i] && i < sizeof(lc_file_name); i++)
		lc_file_name[i] = tolower(file_name[i]);
	lc_file_name[i] = 0;
	if (chdir(confdir) || chdir(driver_name)) {
		ERROR("couldn't change to directory %s: %s",
		      driver_name, strerror(errno));
		return -EINVAL;
	}
	if (load_file(lc_file_name, &driver_file)) {
		ERROR("couldn't open file %s", file_name);
		return -EINVAL;
	}
	strncpy(driver_file.driver_name, driver_name,
		sizeof(driver_file.driver_name));
	if (ioctl(ioctl_device, WRAP_IOCTL_LOAD_BIN_FILE, &driver_file)) {
		ERROR("couldn't upload bin file: %s", file_name);
		return -EINVAL;
	}
	return 0;
}

/*
 * open a windows driver and pass it to the kernel module.
 * returns 0: on success, -1 on error
 */
static int load_driver(int ioctl_device, char *driver_name,
		       char *conf_file_name)
{
	int i;
	struct dirent *dirent;
	struct load_driver *driver;
	int num_sys_files, num_bin_files;
	DIR *driver_dir;

	driver_dir = NULL;
	num_sys_files = 0;
	num_bin_files = 0;

	DBG("loading driver %s", driver_name);
	if (chdir(confdir) || chdir(driver_name)) {
		ERROR("couldn't change to directory %s: %s",
		      driver_name, strerror(errno));
		return -EINVAL;
	}
	driver_dir = opendir(".");
	if (driver_dir == NULL) {
		ERROR("couldn't open driver directory %s: %s",
		      driver_name, strerror(errno));
		return -EINVAL;
	}

	driver = malloc(sizeof(*driver));
	if (driver == NULL) {
		ERROR("couldn't allocate memory for driver %s", driver_name);
		goto err;
	}
	memset(driver, 0, sizeof(*driver));
	strncpy(driver->name, driver_name, MAX_DRIVER_NAME_LEN);

	if (read_conf_file(conf_file_name, driver) ||
	    driver->num_settings == 0) {
		ERROR("couldn't read conf file %s for driver %s",
		      conf_file_name, driver_name);
		goto err;
	}
	while ((dirent = readdir(driver_dir))) {
		int len;
		struct stat statbuf;

		if (dirent->d_name[0] == '.')
			continue;

		if (stat(dirent->d_name, &statbuf) ||
		    !S_ISREG(statbuf.st_mode)) {
			ERROR("%s in %s is not a valid file: %s",
			      dirent->d_name, driver_name, strerror(errno));
			continue;
		}

		len = strlen(dirent->d_name);
		if (len > 4 &&
		     strcasecmp(&dirent->d_name[len-4], ".inf") == 0)
			continue;
		if (len > 5 &&
		     strcasecmp(&dirent->d_name[len-5], ".conf") == 0)
			continue;

		if (len > 4 &&
		    strcasecmp(&dirent->d_name[len-4], ".sys") == 0) {
			if (load_file(dirent->d_name,
				      &driver->sys_files[num_sys_files])) {
				ERROR("couldn't load .sys file %s",
				      dirent->d_name);
				goto err;
			} else
				num_sys_files++;
		} else if (len > 4 &&
			   ((strcasecmp(&dirent->d_name[len-4], ".bin") == 0) ||
			    (strcasecmp(&dirent->d_name[len-4], ".out") == 0))) {
			strcpy(driver->bin_files[num_bin_files].name,
			       dirent->d_name);
			strcpy(driver->bin_files[num_bin_files].driver_name,
			       driver_name);
			driver->bin_files[num_bin_files].size = 0;
			driver->bin_files[num_bin_files].data = NULL;
			num_bin_files++;
		} else
			ERROR("file %s is ignored", dirent->d_name);

		if (num_sys_files == MAX_DRIVER_PE_IMAGES) {
			ERROR("too many .sys files for driver %s",
			      driver_name);
			goto err;
		}
		if (num_bin_files == MAX_DRIVER_BIN_FILES) {
			ERROR("too many .bin files for driver %s",
			      driver_name);
			goto err;
		}
	}

	if (num_sys_files == 0) {
		ERROR("couldn't find valid drivers files for driver %s",
		      driver_name);
		goto err;
	}
	driver->num_sys_files = num_sys_files;
	driver->num_bin_files = num_bin_files;
	strncpy(driver->conf_file_name, conf_file_name,
		sizeof(driver->conf_file_name));
	if (ioctl(ioctl_device, WRAP_IOCTL_LOAD_DRIVER, driver))
		goto err;
	closedir(driver_dir);
	DBG("driver %s loaded", driver_name);
	free(driver);
	return 0;

err:
	if (driver_dir)
		closedir(driver_dir);
	for (i = 0; i < num_sys_files; i++)
		munmap(driver->sys_files[i].data, driver->sys_files[i].size);
	for (i = 0; i < num_bin_files; i++)
		munmap(driver->bin_files[i].data, driver->bin_files[i].size);
	ERROR("couldn't load driver %s", driver_name);
	free(driver);
	return -1;
}

static int get_device(char *driver_name, int vendor, int device, int subvendor,
		      int subdevice, int bus, struct load_device *ld)
{
	int ret;
	struct stat statbuf;
	char file[32];

	DBG("%s", driver_name);
	ret = -1;
	if (chdir(driver_name)) {
		DBG("couldn't chdir to %s: %s", driver_name, strerror(errno));
		return -EINVAL;
	}
	if ((snprintf(file, sizeof(file), "%04X:%04X:%04X:%04X.%X.conf", vendor,
		      device, subvendor, subdevice, bus) &&
	     stat(file, &statbuf) == 0) ||
	    (bus == WRAP_USB_BUS &&
	     snprintf(file, sizeof(file), "%04X:%04X:%04X:%04X.%X.conf", vendor,
		      device, subvendor, subdevice, WRAP_INTERNAL_BUS) &&
	     stat(file, &statbuf) == 0)) {
		DBG("found %s", file);
		ld->subvendor = subvendor;
		ld->subdevice = subdevice;
		ret = 0;
	} else if ((snprintf(file, sizeof(file), "%04X:%04X.%X.conf",
			     vendor, device, bus) &&
		    stat(file, &statbuf) == 0) ||
		   (bus == WRAP_USB_BUS &&
		    snprintf(file, sizeof(file), "%04X:%04X.%X.conf",
			     vendor, device, WRAP_INTERNAL_BUS) &&
		    stat(file, &statbuf) == 0)) {
		DBG("found %s", file);
		ld->subvendor = 0;
		ld->subdevice = 0;
		ret = 0;
	}
	if (chdir(confdir)) {
		ERROR("couldn't chdir to %s: %s", confdir, strerror(errno));
		return -EINVAL;
	}
	if (ret)
		ld->vendor = 0;
	else {
		DBG("found file: %s/%s", driver_name, file);
		ld->vendor = vendor;
		ld->device = device;
		ld->bus = bus;
		strncpy(ld->driver_name, driver_name, sizeof(ld->driver_name));
		strncpy(ld->conf_file_name, file, sizeof(ld->conf_file_name));
	}
	DBG("%04x, %04x, %04x, %04x", ld->vendor, ld->device, ld->subvendor,
	    ld->subdevice);
	return ret;
}

static int load_device(int ioctl_device, int vendor, int device,
		       int subvendor, int subdevice, int bus)
{
	struct dirent  *dirent;
	DIR *dir;
	int res;
	struct load_device load_device;

	DBG("%04x, %04x, %04x, %04x", vendor, device, subvendor, subdevice);
	memset(&load_device, 0, sizeof(load_device));
	if (chdir(confdir)) {
		ERROR("couldn't chdir to %s: %s", confdir, strerror(errno));
		return -EINVAL;
	}
	dir = opendir(".");
	if (dir == NULL) {
		ERROR("directory %s is not valid: %s",
		      confdir, strerror(errno));
		return -EINVAL;
	}
	while ((dirent = readdir(dir))) {
		DBG("%s", dirent->d_name);
		if (dirent->d_name[0] == '.')
			continue;

		if (!get_device(dirent->d_name, vendor, device, subvendor,
				subdevice, bus, &load_device))
			break;
	}
	closedir(dir);

	DBG("%04x, %04x, %04x, %04x", load_device.vendor,
	    load_device.device, load_device.subvendor, load_device.subdevice);
	res = ioctl(ioctl_device, WRAP_IOCTL_LOAD_DEVICE, &load_device);
	DBG("res: %d", res);

	if (res)
		return -1;
	return 0;
}

/*
  * we need a device to use ioctl to communicate with wrapper module
  * we create a device in /dev instead of /tmp as some distributions don't
  * allow creation of devices in /tmp
  */
static int get_ioctl_device(void)
{
	int fd, minor_dev;
	char line[64];
	FILE *proc_misc;

	/* get minor device number used by wrapper driver */
	proc_misc = fopen("/proc/misc", "r");
	if (!proc_misc)
		return -1;
	minor_dev = -1;
	while (fgets(line, sizeof(line), proc_misc)) {
		if (strstr(line, DRIVER_NAME)) {
			long i = strtol(line, NULL, 10);
			if (i != LONG_MAX && i != LONG_MIN) {
				minor_dev = i;
				break;
			}
		}
	}
	fclose(proc_misc);

	if (minor_dev == -1) {
		ERROR("couldn't find wrapper in /proc/misc; "
		      "is module loaded?");
		return -1;
	}

	unlink(ioctl_file);
	if (mknod(ioctl_file, S_IFCHR | 0600, MISC_MAJOR << 8 | minor_dev)) {
		ERROR("couldn't create file %s: %s",
		      ioctl_file, strerror(errno));
		return -1;
	}

	fd = open(ioctl_file, O_RDONLY);
	unlink(ioctl_file);

	if (fd == -1) {
		ERROR("couldn't open file %s: %s",
		      ioctl_file, strerror(errno));
		return -1;
	}
	return fd;
}

int main(int argc, char *argv[0])
{
	int i, res;
	int ioctl_device = -1;
	char *cmd;

	openlog(PROG_NAME, LOG_PERROR | LOG_CONS, LOG_KERN | LOG_DEBUG);

	DBG("argc: %d", argc);

	if (argc == 2 && (strncmp(argv[1], "-v", 2) == 0 ||
			  strncmp(argv[1], "--v", 3) == 0)) {
		printf("version: %s\n", UTILS_VERSION);
		return 0;
	}
	if (argc < 4) {
		res = 1;
		goto out;
	}

	cmd = argv[1];
	i = -1;
	i = atoi(argv[2]);
	if (i < 0) {
		ERROR("invalid debug value %d", i);
		res = 2;
		goto out;
	} else
		debug = i;

	ioctl_device = get_ioctl_device();
	if (ioctl_device == -1) {
		ERROR("unable to open ioctl device %s", ioctl_file);
		res = 5;
		goto out;
	}

	if (atof(argv[3]) != atof(UTILS_VERSION)) {
		ERROR("version %s doesn't match driver version %s",
		      UTILS_VERSION, argv[3]);
		res = 6;
		goto out;
	}

	if (strcmp(cmd, WRAP_CMD_LOAD_DEVICE) == 0) {
		int vendor, device, subvendor, subdevice, bus;
		if (argc != 9) {
			ERROR("incorrect usage of %s (%d)", argv[0], argc);
			res = 7;
			goto out;
		}
		if (sscanf(argv[4], "%04x", &vendor) != 1 ||
		    sscanf(argv[5], "%04x", &device) != 1 ||
		    sscanf(argv[6], "%04x", &subvendor) != 1 ||
		    sscanf(argv[7], "%04x", &subdevice) != 1 ||
		    sscanf(argv[8], "%04x", &bus) != 1) {
			ERROR("couldn't get device info");
			res = 8;
			goto out;
		}
		if (load_device(ioctl_device, vendor, device,
				subvendor, subdevice, bus))
			res = 9;
		else
			res = 0;
	} else if (strcmp(cmd, WRAP_CMD_LOAD_DRIVER) == 0) {
		/* load specific driver and conf file */
		if (argc != 6) {
			ERROR("incorrect usage of %s (%d)", argv[0], argc);
			res = 11;
			goto out;
		}
		res = load_driver(ioctl_device, argv[4], argv[5]);
	} else if (strcmp(cmd, WRAP_CMD_LOAD_BIN_FILE) == 0) {
		/* load specific driver and conf file */
		if (argc != 6) {
			ERROR("incorrect usage of %s (%d)", argv[0], argc);
			res = 12;
			goto out;
		}
		res = load_bin_file(ioctl_device, argv[4], argv[5]);
	} else {
		ERROR("incorrect usage of %s (%d)", argv[0], argc);
		res = 13;
		goto out;
	}
out:
	if (ioctl_device != -1)
		close(ioctl_device);
	closelog();
	return res;
}
