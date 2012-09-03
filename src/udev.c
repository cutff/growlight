#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libudev.h>

#include "udev.h"
#include "pthread.h"
#include "growlight.h"

static struct udev *udev;
struct udev_monitor *udmon;

int udev_event(void){
	struct udev_device *dev;

	while( (dev = udev_monitor_receive_device(udmon)) ){
		verbf("udev: %s %s %s %s %s %s %s\n",
			udev_device_get_devpath(dev),udev_device_get_subsystem(dev),
			udev_device_get_devtype(dev),udev_device_get_syspath(dev),
			udev_device_get_sysname(dev),udev_device_get_sysnum(dev),
			udev_device_get_devnode(dev));
		rescan_device(udev_device_get_sysname(dev));
	}
	return 0;
}

int monitor_udev(void){
	int r;

	if((udev = udev_new()) == NULL){
		diag("Couldn't get udev instance (%s?)\n",strerror(errno));
		return -1;
	}
	if((udmon = udev_monitor_new_from_netlink(udev,"udev")) == NULL){
		diag("Couldn't get udev monitor (%s?)\n",strerror(errno));
		udev_unref(udev);
		return -1;
	}
	if(udev_monitor_filter_add_match_subsystem_devtype(udmon,"block",NULL)){
		diag("Couldn't filter block events\n");
		udev_monitor_unref(udmon);
		udev_unref(udev);
		return -1;
	}
	if(udev_monitor_enable_receiving(udmon)){
		diag("Couldn't receive events from udev\n");
		udev_monitor_unref(udmon);
		udev_unref(udev);
		return -1;
	}
	if((r = udev_monitor_get_fd(udmon)) < 0){
		diag("Couldn't get udev fd\n");
		udev_monitor_unref(udmon);
		udev_unref(udev);
		return -1;
	}
	return r;
}

int shutdown_udev(void){
	udev_monitor_unref(udmon);
	udev_unref(udev);
	udmon = NULL;
	udev = NULL;
	return 0;
}
