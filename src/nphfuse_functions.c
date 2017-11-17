/*
  NPHeap File System
  Copyright (C) 2016 Hung-Wei Tseng, Ph.D. <hungwei_tseng@ncsu.edu>

  This program can be distributed under the terms of the GNU GPLv3.
  See the file COPYING.

  This code is derived from function prototypes found /usr/include/fuse/fuse.h
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  His code is licensed under the LGPLv2.
  A copy of that code is included in the file fuse.h
  
  The point of this FUSE filesystem is to provide an introduction to
  FUSE.  It was my first FUSE filesystem as I got to know the
  software; hopefully, the comments in this code will help people who
  follow later to get a gentler introduction.

*/

#include "nphfuse.h"
#include <npheap.h>
#include <stdbool.h>
///////////////////////////////////////////////////////////
//
// Prototypes for all these functions, and the C-style comments,
// come from /usr/include/fuse.h
//
/** Get file attributes.
 *
 * Similar to stat().  The 'st_dev' and 'st_blksize' fields are
 * ignored.  The 'st_ino' field is ignored except if the 'use_ino'
 * mount option is given.
 */
//Reserving offsets 0 and 1 for bitmaps
bool *fsbitmap = npheap_alloc(NPHFS_DATA -> devfd,0,8192);
bool *dbitmap = npheap_alloc(NPHFS_DATA -> devfd,1,8192);

struct nphfs_file* search(const char *path){
  int i = 2 ;
 bool *temp = fsbitmap + 2;
 struct nphfs_file *search_result;
 while(i < 8192){
 if (&temp){
  search_result = npheap_alloc(NPHFS_DATA -> devfd,i,8192);
  if(strcmp(search_result -> path,path)==0){
    return search_result ; 
  }
 }
  i++;
  temp++;
 }
 return NULL;
} 

int set_bitmap(int flag,int offset,bool value){
//flag is 0 for fs and 1 for data bitmaps
if(!flag){
  bool *temp = fsbitmap + 2 + offset;
  &temp = value;
  return 0;
}
else{
  bool *temp = dbitmap + offset - 8192;
  &temp = value;
  return 0;
}
return 1;
}

int search_bitmap(int flag){
//flag is 0 for fs and 1 for data bitmaps
int i; 
if(!flag){
  bool *temp = fsbitmap + 2;
  i=2;
}
else{
  bool *temp = dbitmap;
  i=8192;
}
while(temp){ 
 i++;
 temp++; 
}
return i;
}

char* split_path(const char *path){
char *p =strrchr(path,'/');
// Returns /filename. So, we have to remove / for filename and remove length
// of this from path.
return p;
}

void initialize_newnode(struct nphfs_file *node){
  strcpy(node->path,"0");
  strcpy(node->parent_path,"0");
  node->data_offset = -1;
  memset(&node->metadata,0,sizeof(struct stat));
  node->fdflag = -1;
  strcpy(node->filename,"0");
  node->fsize = 0;
}

int nphfuse_getattr(const char *path, struct stat *stbuf)
{
  int retval = 1;
  struct nphfs_file *search_result;
  if (path == NULL){
    log_msg("ENOENT for path in getattr");
    return -ENOENT;
  }
  search_result = search(path);
  if(search_result != NULL){
    stbuf = &search_result->metadata;
  }
  log_msg("getattr for path \"%s\" returned \"%d\"\n",path,retval);
  return retval;   
}

/** Read the target of a symbolic link
 *
 * The buffer should be filled with a null terminated string.  The
 * buffer size argument includes the space for the terminating
 * null character.  If the linkname is too long to fit in the
 * buffer, it should be truncated.  The return value should be 0
 * for success.
 */
// Note the system readlink() will truncate and lose the terminating
// null.  So, the size passed to to the system readlink() must be one
// less than the size passed to nphfuse_readlink()
// nphfuse_readlink() code by Bernardo F Costa (thanks!)
int nphfuse_readlink(const char *path, char *link, size_t size)
{
    return -1;
}

/** Create a file node
 *
 * There is no create() operation, mknod() will be called for
 * creation of all non-directory, non-symlink nodes.
 */
int nphfuse_mknod(const char *path, mode_t mode, dev_t dev)
{
    return -ENOENT;
}

/** Create a directory */
int nphfuse_mkdir(const char *path, mode_t mode)
{
  int retval = 1;
  int res = 0;
  int fs_bmoffset = 0;
  char *filename;
  char *parent_path;
  struct nphfs_file *search_result,*parent;
  struct fuse_context *context;
  if (path == NULL){
    log_msg("ENOENT for path in getattr\n");
    return -ENOENT;
  }
  search_result = search(path);
  if (search_result != NULL){
    log_msg("Cannot create \"%s\" : F/D already exists\n",path);
    return -1;
  }
  filename = split_path(path);
  strncpy(parent_path, path, strlen(path)-strlen(filename));
  parent = search(parent_path);
  if (parent == NULL){
    log_msg("Cannot create \"%s\" : Parent not found for \"%s\"\n",path,parent_path);
    return -1;
  }
  fs_bmoffset = search_bitmap(0);
  log_msg("Creating new node for \"%s\" with parent \"%s\"\n",path,parent_path);
  struct npfhs_file *newnode = npheap_alloc(NPHFS_DATA->devfd,fs_bmoffset,8192);
  res = set_bitmap(0,fs_bmoffset,true);
  if (res){
   log_msg("Set bit map failed\n");
  }
  initialize_newnode(*newnode);
  strcpy(newnode->path,path);
  strcpy(newnode->parent_path,parent_path);
  //Not changing data offset since this is a directory.
  newnode->metadata.st_nlink = 1;
  //Should increment numlink in parent.
  parent->metadata.st_nlink ++;
  context = fuse_get_context() ;
  newnode->metadata.st_mode = mode & ~(context->umask) & 0777; 
  newnode->metadata.st_uid = context->uid;
  newnode->metadata.st_gid = context->gid;
  newnode->metadata.st_atim = (time_t)time(NULL);
  newnode->metadata.st_mtim = (time_t)time(NULL);
  newnode->metadata.st_ctim = (time_t)time(NULL);
  //fdflag = 1 for directories
  newnode->fdflag = 1; 
  strcpy(newnode->filename,filename);
  log_msg("mkdir for path \"%s\" with mode \"%o\"returned \"%d\"\n",path,mode,retval);
  return retval;
}

/** Remove a file */
int nphfuse_unlink(const char *path)
{
    return -1;
}

/** Remove a directory */
int nphfuse_rmdir(const char *path)
{
    return -1;
}

/** Create a symbolic link */
// The parameters here are a little bit confusing, but do correspond
// to the symlink() system call.  The 'path' is where the link points,
// while the 'link' is the link itself.  So we need to leave the path
// unaltered, but insert the link into the mounted directory.
int nphfuse_symlink(const char *path, const char *link)
{
    return -1;
}

/** Rename a file */
// both path and newpath are fs-relative
int nphfuse_rename(const char *path, const char *newpath)
{
    return -1;
}

/** Create a hard link to a file */
int nphfuse_link(const char *path, const char *newpath)
{
    return -1;
}

/** Change the permission bits of a file */
int nphfuse_chmod(const char *path, mode_t mode)
{
        return -ENOENT;
}

/** Change the owner and group of a file */
int nphfuse_chown(const char *path, uid_t uid, gid_t gid)
{
        return -ENOENT;
}

/** Change the size of a file */
int nphfuse_truncate(const char *path, off_t newsize)
{
        return -ENOENT;
}

/** Change the access and/or modification times of a file */
int nphfuse_utime(const char *path, struct utimbuf *ubuf)
{
        return -ENOENT;
}

/** File open operation
 *
 * No creation, or truncation flags (O_CREAT, O_EXCL, O_TRUNC)
 * will be passed to open().  Open should check if the operation
 * is permitted for the given flags.  Optionally open may also
 * return an arbitrary filehandle in the fuse_file_info structure,
 * which will be passed to all file operations.
 *
 * Changed in version 2.2
 */
int nphfuse_open(const char *path, struct fuse_file_info *fi)
{
    if ((fi->flags & O_ACCMODE) != O_RDONLY)
        return -EACCES;

    return -ENOENT;

}

/** Read data from an open file
 *
 * Read should return exactly the number of bytes requested except
 * on EOF or error, otherwise the rest of the data will be
 * substituted with zeroes.  An exception to this is when the
 * 'direct_io' mount option is specified, in which case the return
 * value of the read system call will reflect the return value of
 * this operation.
 *
 * Changed in version 2.2
 */
// I don't fully understand the documentation above -- it doesn't
// match the documentation for the read() system call which says it
// can return with anything up to the amount of data requested. nor
// with the fusexmp code which returns the amount of data also
// returned by read.
int nphfuse_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    return -ENOENT;
}

/** Write data to an open file
 *
 * Write should return exactly the number of bytes requested
 * except on error.  An exception to this is when the 'direct_io'
 * mount option is specified (see read operation).
 *
 */
int nphfuse_write(const char *path, const char *buf, size_t size, off_t offset,
	     struct fuse_file_info *fi)
{
    return -ENOENT;
}

/** Get file system statistics
 *
 * The 'f_frsize', 'f_favail', 'f_fsid' and 'f_flag' fields are ignored
 *
 * Replaced 'struct statfs' parameter with 'struct statvfs' in
 * version 2.5
 */
int nphfuse_statfs(const char *path, struct statvfs *statv)
{
    return -1;
}

/** Possibly flush cached data
 *
 * BIG NOTE: This is not equivalent to fsync().  It's not a
 * request to sync dirty data.
 *
 * Flush is called on each close() of a file descriptor.  So if a
 * filesystem wants to return write errors in close() and the file
 * has cached dirty data, this is a good place to write back data
 * and return any errors.  Since many applications ignore close()
 * errors this is not always useful.
 *
 * NOTE: The flush() method may be called more than once for each
 * open().  This happens if more than one file descriptor refers
 * to an opened file due to dup(), dup2() or fork() calls.  It is
 * not possible to determine if a flush is final, so each flush
 * should be treated equally.  Multiple write-flush sequences are
 * relatively rare, so this shouldn't be a problem.
 *
 * Filesystems shouldn't assume that flush will always be called
 * after some writes, or that if will be called at all.
 *
 * Changed in version 2.2
 */

// this is a no-op in NPHFS.  It just logs the call and returns success
int nphfuse_flush(const char *path, struct fuse_file_info *fi)
{
    log_msg("\nnphfuse_flush(path=\"%s\", fi=0x%08x)\n", path, fi);
    // no need to get fpath on this one, since I work from fi->fh not the path
    log_fi(fi);
	
    return 0;
}

/** Release an open file
 *
 * Release is called when there are no more references to an open
 * file: all file descriptors are closed and all memory mappings
 * are unmapped.
 *
 * For every open() call there will be exactly one release() call
 * with the same flags and file descriptor.  It is possible to
 * have a file opened more than once, in which case only the last
 * release will mean, that no more reads/writes will happen on the
 * file.  The return value of release is ignored.
 *
 * Changed in version 2.2
 */
int nphfuse_release(const char *path, struct fuse_file_info *fi)
{
    return 0;
}

/** Synchronize file contents
 *
 * If the datasync parameter is non-zero, then only the user data
 * should be flushed, not the meta data.
 *
 * Changed in version 2.2
 */
int nphfuse_fsync(const char *path, int datasync, struct fuse_file_info *fi)
{
    return -1;
}

#ifdef HAVE_SYS_XATTR_H
/** Set extended attributes */
int nphfuse_setxattr(const char *path, const char *name, const char *value, size_t size, int flags)
{
    return -61;
}

/** Get extended attributes */
int nphfuse_getxattr(const char *path, const char *name, char *value, size_t size)
{
    return -61;
}

/** List extended attributes */
int nphfuse_listxattr(const char *path, char *list, size_t size)
{
    return -61;
}

/** Remove extended attributes */
int nphfuse_removexattr(const char *path, const char *name)
{
    return -61;
}
#endif

/** Open directory
 *
 * This method should check if the open operation is permitted for
 * this directory
 *
 * Introduced in version 2.3
 */
int nphfuse_opendir(const char *path, struct fuse_file_info *fi)
{
  if (path == NULL){
    log_msg("ENOENT for path \"%s\" in opendir\n",path);
    return -ENOENT;
  }
 return 0;
}

/** Read directory
 *
 * This supersedes the old getdir() interface.  New applications
 * should use this.
 *
 * The filesystem may choose between two modes of operation:
 *
 * 1) The readdir implementation ignores the offset parameter, and
 * passes zero to the filler function's offset.  The filler
 * function will not return '1' (unless an error happens), so the
 * whole directory is read in a single readdir operation.  This
 * works just like the old getdir() method.
 *
 * 2) The readdir implementation keeps track of the offsets of the
 * directory entries.  It uses the offset parameter and always
 * passes non-zero offset to the filler function.  When the buffer
 * is full (or an error happens) the filler function will return
 * '1'.
 *
 * Introduced in version 2.3
 */

int nphfuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
  struct fuse_file_info *fi)
{ 

  struct nphfs_file *search_result;
  
  if (path == NULL){
    log_msg("ENOENT for path in readdir\n");
    return -ENOENT;
  }
  search_result = search(path);
  if (search_result == NULL){
    log_msg("directory \"%s\" doesn't exist\n ",path);
    return -1;
  }
  //Fill all the nodes with parent_path=path in buffer 
  int i = 2 ;
  bool *temp = fsbitmap + 2;
  while(i < 8192){
   if (&temp){
    search_result = npheap_alloc(NPHFS_DATA -> devfd,i,8192);
    if(strcmp(search_result -> parent_path,path)==0){
     if (!filler(buf,search_result->filename,&search_result->metadata,0)){
      log_msg("File \"%s\" data copied to buffer\n",search_result->filename);
    }
    else {
      log_msg("Filler fail/buffer overflow for File \"%s\" ",search_result->filename);      
    } 
  }
}
i++;
temp++;
}
return 0;
}

/** Release directory
 */
int nphfuse_releasedir(const char *path, struct fuse_file_info *fi)
{
    return 0;
}

/** Synchronize directory contents
 *
 * If the datasync parameter is non-zero, then only the user data
 * should be flushed, not the meta data
 *
 * Introduced in version 2.3
 */
// when exactly is this called?  when a user calls fsync and it
// happens to be a directory? ??? 
int nphfuse_fsyncdir(const char *path, int datasync, struct fuse_file_info *fi)
{
    return 0;
}

int nphfuse_access(const char *path, int mask)
{
    return 0;
//    return -1;
}

/**
 * Change the size of an open file
 *
 * This method is called instead of the truncate() method if the
 * truncation was invoked from an ftruncate() system call.
 *
 * If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the truncate() method will be
 * called instead.
 *
 * Introduced in version 2.5
 */
int nphfuse_ftruncate(const char *path, off_t offset, struct fuse_file_info *fi)
{
    return -1;
}

/**
 * Get attributes from an open file
 *
 * This method is called instead of the getattr() method if the
 * file information is available.
 *
 * Currently this is only called after the create() method if that
 * is implemented (see above).  Later it may be called for
 * invocations of fstat() too.
 *
 */
int nphfuse_fgetattr(const char *path, struct stat *statbuf, struct fuse_file_info *fi)
{
        return -ENOENT;
}

void *nphfuse_init(struct fuse_conn_info *conn)
{
    log_msg("\nnphfuse_init()\n");
    log_conn(conn);
    log_fuse_context(fuse_get_context());
        
    return NPHFS_DATA;
}

/**
 * Clean up filesystem
 *
 * Called on filesystem exit.
 *
 * Introduced in version 2.3
 */
void nphfuse_destroy(void *userdata)
{
    log_msg("\nnphfuse_destroy(userdata=0x%08x)\n", userdata);
}
