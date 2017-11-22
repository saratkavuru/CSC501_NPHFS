//Project 3: Sarat Kavuru, skavuru; Rachit Thirani, rthiran;
/*
  Copyright (C) 2016 Hung-Wei Tseng, Ph.D. <hungwei_tseng@ncsu.edu>

  This program can be distributed under the terms of the GNU GPLv3.
  See the file COPYING.

  You may extend this file if necessary  
*/
#include<sys/stat.h>

struct nphfuse_state {
    FILE *logfile;
    char *device_name;
    int devfd;
};

struct nphfs_file {
	//int inode;
	//int p_inode;
	char path[1024];
	char parent_path[1024];
	char symlink_path[1024];
	int fs_offset;
	int pin_count;
	int data_offset;
	struct stat metadata;
	int fdflag;
	char filename[250];
};