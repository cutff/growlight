#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mmap.h"
#include "popen.h"
#include "growlight.h"

static int
ext4_mkfs(const char *dev,const char *name){
	// if we're an mdadm, get chunk size and pass it as -Estride= FIXME
	// same for stripe_width FIXME
	// need -F for non-partition or block special FIXME
	// pass -M with mount point FIXME
	// allow a UUID to be supplied FIXME
	// allow -c (badblock check) FIXME
	if(name == NULL){
		name = "SprezzaEXT4";
	}
	if(vspopen_drain("mkfs -t ext4 -b -2048 -o SprezzOS -E lazy_itable_init=0,lazy_journal_init=0 -L %s -O dir_index,extent,^uninit_bg %s",name,dev)){
		return -1;
	}
	return 0;
}

// FIXME surely there's a less grotesque way of doing this? parse blkid -k?
static const struct fs {
	const char *name;
	const char *desc;
	int (*mkfs)(const char *,const char *);
} fss[] = {
	{
		.name = "vfat",
		.desc = "File Allocation Table (DOS default)",
	},
	{
		.name = "swsuspend",
		.desc = "Software suspend block device",
	},
	{
		.name = "swap",
		.desc = "Swap device",
	},
	{
		.name = "xfs",
		.desc = "SGI's XFS (IRIX default)",
	},
	{
		.name = "ext4dev",
		.desc = "Obsolete, development-series ext4",
	},
	{
		.name = "ext4",
		.desc = "Extended Filesystem 4 (Linux default)",
		.mkfs = ext4_mkfs,
	},
	{
		.name = "ext3",
		.desc = "Extended Filesystem 3",
	},
	{
		.name = "ext2",
		.desc = "Extended Filesystem 2",
	},
	{
		.name = "jbd",
		.desc = "Journaling Block Device",
	},
	{
		.name = "reiserfs",
		.desc = "ReiserFS",
	},
	{
		.name = "reiser4",
		.desc = "ReiserFS 4",
	},
	{
		.name = "jfs",
		.desc = "IBM's Journaled Filesystem (AIX JFS2)",
	},
	{
		.name = "udf",
		.desc = "Universal Disk Format (ISO/IEC 13346)",
	},
	{
		.name = "iso9660",
		.desc = "Compact Disc Filesystem (ISO 9660:1999)",
	},
	{
		.name = "zfs_member",
		.desc = "ZFS zpool member",
	},
	{
		.name = "hfsplus",
		.desc = "HFS+ (Mac OS Extended) (OS X default)",
	},
	{
		.name = "hfs",
		.desc = "Hierarchal Filesystem (Mac OS Standard)",
	},
	{
		.name = "ufs",
		.desc = "UNIX Filesystem (BFFS) (BSD default)",
	},
	{
		.name = "hpfs",
		.desc = "OS/2's High Performance Filesystem",
	},
	{
		.name = "sysv",
		.desc = "System V Filesystem (S5FS)",
	},
	{
		.name = "xenix",
		.desc = "XENIX Filesystem (XENIX default)",
	},
	{
		.name = "ntfs",
		.desc = "Microsoft's New Technology Filesystem (Windows default)",
	},
	{
		.name = "cramfs",
		.desc = "Compressed Read-Only Filesystem",
	},
	{
		.name = "romfs",
		.desc = "Read-Only Filesystem",
	},
	{
		.name = "minix",
		.desc = "MINIX Filesystem (MINIX default)",
       	},
	{
		.name = "gfs",
		.desc = "Red Hat's Global Filesystem",
	},
	{
		.name = "gfs2",
		.desc = "Red Hat's Global Filesystem 2",
	},
	{
		.name = "ocfs",
		.desc = "Oracle Cluster Filesystem",
	},
	{
		.name = "ocfs2",
		.desc = "Oracle Cluster Filesystem 2",
	},
	{
		.name = "oracleasm",
		.desc = "Oracle Automatic Storage Management",
	},
	{
		.name = "vxfs",
		.desc = "VERITAS Filesystem (HP-UX JFS) (HP-UX default)",
	},
	{
		.name = "squashfs",
		.desc = "Squashed Read-Only Filesystem",
	},
	{
		.name = "nss",
		.desc = "Novell Storage Services",
	},
	{
		.name = "btrfs",
		.desc = "Oracle's B-Tree Filesystem",
	},
	{
		.name = "ubifs",
		.desc = "Unsorted Block Image Filesystem",
	},
	{
		.name = "bfs",
		.desc = "UNIXWare Boot Filesystem (SCO boot default)",
	},
	{
		.name = "VMFS",
		.desc = "VMware's Virtual Machine Filesystem",
	},
	{
		.name = "befs",
		.desc = "Be Filesystem (BeOS default)",
	},
	{
		.name = "nilfs2",
		.desc = "NTT's New Implementation of Log-structued FS 2",
	},
	{
		.name = "exfat",
		.desc = "Microsoft's Extented File Allocation Table",
	},
	{ .name = NULL, }
};

pttable_type *get_fs_types(int *count){
	pttable_type *pt;
	int z;

	*count = (sizeof(fss) / sizeof(*fss)) - 1;
	if(*count <= 0){
		diag("Invalid fs type count (%d), aborting",*count);
		return NULL;
	}
	if((pt = malloc(sizeof(*pt) * *count)) == NULL){
		*count = 0;
		return NULL;
	}
	for(z = 0 ; z < *count ; ++z){
		if((pt[z].name = strdup(fss[z].name)) == NULL){
			goto err;
		}
		if(fss[z].desc == NULL){
			// FIXME they all ought have descriptions
			pt[z].desc = strdup("FIXME Need description");
		}else{
			if((pt[z].desc = strdup(fss[z].desc)) == NULL){
				free(pt[z].name);
				goto err;
			}
		}
	}
	return pt;

err:
	while(z--){
		free(pt[z].name);
		free(pt[z].desc);
	}
	free(pt);
	*count = 0;
	return NULL;
}

int make_filesystem(device *d,const char *ptype,const char *name){
	const struct fs *pt;

	if(d == NULL || ptype == NULL){
		diag("Passed NULL arguments, aborting\n");
		return -1;
	}
	if(d->target){
		diag("Won't create fs on target mount %s (%s)\n",
				d->name,d->target->path);
		return -1;
	}
	if(d->mnt){
		diag("Won't create fs on active mount %s (%s)\n",
				d->name,d->mnt);
		return -1;
	}
	if(d->swapprio >= SWAP_MAXPRIO){
		diag("Won't create fs on active mount %s (%s)\n",
				d->name,d->mnt);
		return -1;
	}
	for(pt = fss ; pt->name ; ++pt){
		if(strcmp(pt->name,ptype) == 0){
			char dbuf[PATH_MAX],*mnttype;

			if(snprintf(dbuf,sizeof(dbuf),"/dev/%s",d->name) >= (int)sizeof(dbuf)){
				diag("Bad name: %s\n",d->name);
				return -1;
			}
			if(pt->mkfs == NULL){
				diag("Don't know how to make %s\n",ptype);
				return -1;
			}
			if((mnttype = strdup(ptype)) == NULL){
				return -1;
			}
			// FIXME needs accept/set UUID and label!
			if(pt->mkfs(dbuf,name)){
				free(mnttype);
				return -1;
			}
			// FIXME reprobe device?
			free(d->mnttype);
			d->mnttype = mnttype;
			return 0;
		}
	}
	diag("Unsupported partition table type: %s\n",ptype);
	return -1;
}

int parse_filesystems(const glightui *gui __attribute__ ((unused)),const char *fn){
	off_t len,idx;
	char *map;
	int fd;

	if((map = map_virt_file(fn,&fd,&len)) == MAP_FAILED){
		return -1;
	}
	idx = 0;
	while(idx < len){
		off_t fsstart;
		int virt = 0;

		while(idx < len && isspace(map[idx])){
			++idx;
		}
		if(len - idx >= (off_t)strlen("nodev")){
			if(strncmp(map + idx,"nodev",strlen("nodev")) == 0){
				idx += strlen("nodev");
				virt = 1;
			}
		}
		while(idx < len && isspace(map[idx])){
			++idx;
		}
		fsstart = idx;
		while(idx < len && !isspace(map[idx])){
			++idx;
		}
		if(fsstart < len){
			verbf("%sfilesystem support: %*.*s\n",
					virt ? "virtual " : "",
					(int)(idx - fsstart),
					(int)(idx - fsstart),map + fsstart);
		}
	}
	munmap_virt(map,len);
	close(fd);
	return 0;
}

int wipe_filesystem(device *d){
	if(d->mnttype == NULL){
		diag("No filesystem on %s\n",d->name);
		return -1;
	}
	diag("Filesystem wipe is not yet implemented FIXME"); return -1;
	return 0;
}
