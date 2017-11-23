//Project 3: Sarat Kavuru, skavuru; Rachit Thirani, rthiran;
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
#include <sys/mman.h>
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
bool *fsbitmap ;
bool *dbitmap ;
struct nphfs_file* root;
static __u64 failcount; 

struct nphfs_file* search(const char *path){
  int i = 2 ;
 bool *temp ;
 temp = fsbitmap + 2;
 void *node;
  if (strcmp("/",path) == 0)
  {
    log_msg("search root directory\n"); 
    return root;
  }
 while(i < 8192){
 //log_msg("i=\"%d %d\"\n",i,*temp);
 if (*temp){
  
  log_msg("Searching offset \"%d\"\n",i);
  npheap_lock(NPHFS_DATA->devfd,i);
  node = npheap_alloc(NPHFS_DATA->devfd,i,8192);
  failcount ++;
  npheap_unlock(NPHFS_DATA->devfd,i); 
  if(node == -1){
    log_msg("BIchokkoo for offset \"%d\"\n",i);
  }
  struct nphfs_file *search_result = (struct nphfs_file*)node;
  log_msg("Address of offset \"%d\" is \"%p\" with failcount \"%ld\"\n",i,search_result,failcount);
  log_msg("search for path \"%s\" of length \"%d\" found \"%s\"  \n",path,strlen(path),search_result->path);
  if(strcmp(search_result -> path,path)==0){
    log_msg("search for path \"%s\" of length \"%d\" returned \"%s\"  \n",path,strlen(path),search_result->path);
    return search_result ; 
  }
 }
 munmap(node,8192);
  i++;
  temp++;
 }
 return NULL;
} 

//Flag is 1 for increment and -1 for decrement
/*void update_links(int data_offset,int flag){
 int i = 2 ;
 bool *temp ;
 temp = fsbitmap + 2;
 struct nphfs_file *node;
 while(i < 8192){
  //log_msg("i=\"%d %d\"\n",i,*temp);
 if (*temp){
  node = npheap_alloc(NPHFS_DATA -> devfd,i,8192);
  //strcmp is to differentiate between hard and soft links.
  if(node->data_offset == data_offset){
     
     if(strcmp(node->symlink_path,"0") == 0) {
       log_msg("update link for file \"%s\"\n",node->filename);
       node->metadata.st_nlink += flag;
  }
 }
}
  i++;
  temp++;
 }
} */

void update_metadata(int data_offset,struct stat metadata){
  int i = 2 ;
 bool *temp ;
 temp = fsbitmap + 2;
 struct nphfs_file *node;
 while(i < 8192){
  //log_msg("i=\"%d %d\"\n",i,*temp);
 if (*temp){
  node = npheap_alloc(NPHFS_DATA -> devfd,i,8192);
  if(node->data_offset == data_offset){
    log_msg("update metadata for file \"%s\"\n",node->filename);
    node->metadata = metadata;
  }
 }
  i++;
  temp++;
  munmap(node,8192);
 }
}

int set_bitmap(int flag,int offset,bool value){
//flag is 0 for fs and 1 for data bitmaps
if(!flag){
  bool *temp;
  temp = fsbitmap + offset;
  *temp = value;
  log_msg("Set fs bitmap for offset \"%d\" - \"%d\"\n",offset,value);
  return 0;
}
else{
  bool *temp;
  temp = dbitmap + offset - 8192;
  *temp = value;
  log_msg("Set d bitmap for offset \"%d\" - \"%d\"\n",offset,value);
  return 0;
}
return 1;
}

int search_bitmap(int flag){
//flag is 0 for fs and 1 for data bitmaps
int i;
bool *temp; 
if(!flag){
  temp = fsbitmap + 2;
  i=2;
}
else{
  temp = dbitmap;
  i=8192;
}
while(*temp){ 
 i++;
 temp++; 
}
if(i>((flag+1)*8191))
{ 
  log_msg("Search bitmap returned -1\n");
  return -1;
}
log_msg("Search bitmap returned \"%d\"\n",i);
return i;
}

// Returns filename. split_parent returns parent name.

char* split_path(const char *path){
    char *p = strrchr(path,'/');
      return ++p;
}

char* split_parent(const char *path,char* filename){
  char *p = malloc(strlen(path)-strlen(filename));  
  if((strlen(path)-strlen(filename)) == 1){
        p = "/";
    }
    else{
        memset(p, '\0', strlen(path)-strlen(filename));
        strncpy(p,path,(strlen(path)-strlen(filename))-1);
        //p[strlen(p)] = '\0';
    }
    return p;
}

void initialize_newnode(struct nphfs_file *node){
  strcpy(node->path,"0");
  strcpy(node->parent_path,"/");
  strcpy(node->symlink_path,"0");
  node->fs_offset=-1;
  node->data_offset = -1;
  node->pin_count = 0;
  memset(&node->metadata,0,sizeof(struct stat));
  node->fdflag = -1;
  strcpy(node->filename,"0");
}

int nphfuse_getattr(const char *path, struct stat *stbuf)
{
  log_msg("In getattr for \"%s\"\n",path);
  int retval = 0;
  struct nphfs_file *search_result;
  if (path == NULL){
    log_msg("ENOENT for path in getattr\n");
    return -ENOENT;
  }
  if (strcmp("/",path) == 0)
  {
    log_msg("get attr of root directory\n");
    memset(stbuf, 0, sizeof(struct stat));
    stbuf->st_mode = S_IFDIR | 0777;
    stbuf->st_nlink = 2;
    stbuf->st_size = 8192; 
    log_msg("get attr of root directory return\n");
    return 0;
  }
  
  search_result = search(path);
  log_msg("search for path \"%s\" of length \"%d\" returned \"%s\"  \n",path,strlen(path),search_result->path);
  if(search_result != NULL){
    memset(stbuf, 0, sizeof(struct stat));
    memcpy(stbuf,&search_result->metadata,sizeof(struct stat));
    log_msg("getattr for path \"%s\" found\n",path);
    log_msg("I-node number: %ld\n", (long) stbuf->st_ino);
    log_msg("Mode:  %lo (octal)\n",(unsigned long) stbuf->st_mode);
    log_msg("Link count: %ld\n", (long) stbuf->st_nlink);
    log_msg("Ownership:  UID=%ld   GID=%ld\n", (long) stbuf->st_uid, (long) stbuf->st_gid);
    log_msg("Last status change:   %s", ctime(&stbuf->st_ctime));
    log_msg("Last file access:    %s", ctime(&stbuf->st_atime));
    log_msg("Last file modification:  %s", ctime(&stbuf->st_mtime));
  }
  else{
    log_msg("getattr for path \"%s\" not found\n",path);
    return -ENOENT;
  }
  log_msg("getattr for path \"%s\" returned \"%d\"\n",path,retval);
  munmap(search_result,8192);
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
    log_msg("In readlink for path \"%s\"\n",path);
    char *filename;
    struct nphfs_file *search_result;
  if (path == NULL){
    log_msg("ENOENT for path in readlink\n");
    return -ENOENT;
  }
  
  search_result = search(path);
  log_msg("search for path \"%s\" of length \"%d\" returned \"%s\"  \n",path,strlen(path),search_result->path);
  if(search_result != NULL){
    filename = search_result -> symlink_path;
    strcpy(link,filename); 
    log_msg("Read link  \"%s\" is \"%s\"\n",path,link);
    return 0;
  }
    log_msg("readlink returns -1 for  \"%s\" \n",path);
    return -ENOENT;
}

/** Create a file node
 *
 * There is no create() operation, mknod() will be called for
 * creation of all non-directory, non-symlink nodes.
 */
int nphfuse_mknod(const char *path, mode_t mode, dev_t dev)
{
  log_msg("In mknod of path \"%s\"\n",path);
  int fsretval;
  int dretval;
  int fs_bmoffset;
  int d_bmoffset;
  char *filename;
  char *parent_path;
  struct fuse_context *context;
  struct nphfs_file *parent;
  struct nphfs_file *newfile = search(path);
  if(path == NULL){
    log_msg("\"%s\" : Illegal path \n",path);
    return -ENOENT; 
  }
  if(newfile != NULL){
    log_msg("\"%s\" : File already exists \n",path);
    return -EEXIST; 
  }
  //Create a new file.
  filename = split_path(path);
  parent_path = split_parent(path,filename);
  log_msg("filename : \"%s\" of length \"%d\"\n",filename,strlen(filename));
  log_msg("Parent : \"%s\" of length \"%d\" for \"%s\" of length \"%d\"\n",parent_path,strlen(parent_path),path,strlen(path));
  parent = search(parent_path);
  if (parent == NULL){
    log_msg("Cannot create \"%s\" : Parent not found for \"%s\"\n",path,parent_path);
    return -ENOENT;
  }
  if((parent != NULL) && (!parent->fdflag)){
    log_msg("Cannot create \"%s\" : Parent  \"%s\" is not a directory\n",path,parent_path);
    return -ENOTDIR;
  }
  fs_bmoffset = search_bitmap(0);
  if(fs_bmoffset == -1){
     log_msg("Fbitmap returned -1 \n");
    return -ENOMEM;
  }
  d_bmoffset = search_bitmap(1);
  if(d_bmoffset == -1){
     log_msg("dbitmap returned -1 \n");
    return -ENOMEM;
  }
  log_msg("Creating new node for \"%s\" with parent \"%s\"\n",path,parent_path);
  newfile = npheap_alloc(NPHFS_DATA->devfd,fs_bmoffset,8192);
  failcount ++;
  fsretval = set_bitmap(0,fs_bmoffset,true);
  dretval = set_bitmap(1,d_bmoffset,true);
  log_msg("Return values for bitmaps d - \"%d\" and f -  \"%d\" \n",dretval,fsretval);
  initialize_newnode(newfile);
  strcpy(newfile->path,path);
  strcpy(newfile->parent_path,parent_path);
 newfile->fs_offset = fs_bmoffset;
 //Changing data offset to that of oldfile to make it a symlink.
 newfile->data_offset = d_bmoffset;
 newfile->metadata.st_nlink = 1;
 //Should increment numlink in parent.
 parent->metadata.st_nlink ++;
 parent->metadata.st_ctime = (time_t)time(NULL);
 context = fuse_get_context();
 newfile->metadata.st_rdev = dev;
 newfile->metadata.st_mode = S_IFREG|mode; 
 newfile->metadata.st_uid = context->uid;
 newfile->metadata.st_gid = context->gid;
 newfile->metadata.st_atime = (time_t)time(NULL);
 newfile->metadata.st_mtime = (time_t)time(NULL);
 newfile->metadata.st_ctime = (time_t)time(NULL);
  //fdflag = 0 for files
 newfile->fdflag = 0; 
 strcpy(newfile->filename,filename);
 log_msg("Link for path \"%s\" with has filename \"%s\"\n",path,newfile->filename);
  munmap(newfile,8192);
  munmap(parent,8192);

 return 0;
}


/** Create a directory */
int nphfuse_mkdir(const char *path, mode_t mode)
{
  log_msg("In mkdir for path \"%s\"\n",path);
  int retval = 0;
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
    return -EEXIST;
  }
  filename = split_path(path);
  parent_path = split_parent(path,filename);
  log_msg("Parent : \"%s\" for \"%s\"\n",parent_path,path);
  parent = search(parent_path);
  log_msg("parent found for d \"%s\"\n",parent->path);
  if (parent == NULL){
    log_msg("Cannot create \"%s\" : Parent not found for \"%s\"\n",path,parent_path);
    return -ENOENT;
  }
  if((parent != NULL) && (!parent->fdflag)){
    log_msg("Cannot create \"%s\" : Parent  \"%s\" is not a directory\n",path,parent_path);
    return -ENOTDIR;
  }
  fs_bmoffset = search_bitmap(0);
  if(fs_bmoffset == -1){
     log_msg("Fbitmap returned -1 \n");
    return -ENOMEM;
  }
  log_msg("Creating new node for \"%s\" with parent \"%s\"\n",path,parent_path);
  struct nphfs_file *newnode = npheap_alloc(NPHFS_DATA->devfd,fs_bmoffset,8192);
  res = set_bitmap(0,fs_bmoffset,true);
  if (res){
   log_msg("Set bit map failed\n");
  }
  initialize_newnode(newnode);
  strcpy(newnode->path,path);
  strcpy(newnode->parent_path,parent_path);
  newnode->fs_offset = fs_bmoffset;
  //Not changing data offset since this is a directory.
  newnode->metadata.st_nlink = 2;
  //Should increment numlink in parent.
  parent->metadata.st_nlink ++;
  parent->metadata.st_ctime = (time_t)time(NULL);
  context = fuse_get_context() ;
  newnode->metadata.st_size = 64;
  parent->metadata.st_size += 64;
  newnode->metadata.st_mode = S_IFDIR|mode; 
  newnode->metadata.st_uid = context->uid;
  newnode->metadata.st_gid = context->gid;
  newnode->metadata.st_atime = (time_t)time(NULL);
  newnode->metadata.st_mtime = (time_t)time(NULL);
  newnode->metadata.st_ctime = (time_t)time(NULL);
  //fdflag = 1 for directories
  newnode->fdflag = 1; 
  strcpy(newnode->filename,filename);
  log_msg("mkdir for path \"%s\" with mode \"%o\"returned \"%d\"\n",path,mode,retval);
  munmap(newnode,8192);
  munmap(parent,8192);
  return retval;
}

/** Remove a file */
int nphfuse_unlink(const char *path)
{
 log_msg("In unlink for path \"%s\"\n",path);
 int retdval = 1;
 int retfsval = 1;
 int ddval = 1;
 int dfsval = 1;
 char *filename;
 char *parent_path;
 struct nphfs_file *parent;
 struct nphfs_file *search_result;
 search_result = search(path);
 if(search_result == NULL) {
  log_msg("Search for path \"%s\" returned null\n",path);
  return -ENOENT;
}
if(search_result->fdflag){
  log_msg("Cannot delete: File \"%s\" is a directory\n",search_result->filename);
  return -EISDIR;
}
  filename = split_path(path);
  parent_path = split_parent(path,filename);
  log_msg("Parent : \"%s\" for \"%s\"\n",parent_path,path);
  parent = search(parent_path);
  log_msg("parent found for d \"%s\"\n",parent->path);
  if (parent == NULL){
    log_msg("Cannot create \"%s\" : Parent not found for \"%s\"\n",path,parent_path);
    return -ENOENT;
  }
if(search_result->pin_count == 0){
  log_msg("Deleting file \"%s\" with link count \"%d\" \n",search_result->filename,search_result->metadata.st_nlink);
  dfsval = npheap_delete(NPHFS_DATA->devfd,search_result->fs_offset);
  retfsval = set_bitmap(0,search_result->fs_offset,false);
  search_result->metadata.st_nlink --;
  parent->metadata.st_nlink --;
  parent->metadata.st_ctime = (time_t)time(NULL);
  search_result->metadata.st_ctime = (time_t)time(NULL);
  munmap(parent,8192);

  //edge case for symbolic link
  if(strcmp(search_result->symlink_path,"0") != 0){
    log_msg("Deleted link to \"%s\"\n",search_result->symlink_path);
    return 0;
  }
  update_metadata(search_result->data_offset,search_result->metadata);
  if(search_result->metadata.st_nlink == 0){
    //Deleting data only if it's the last link.
    retdval = set_bitmap(1,search_result->data_offset,false);
    ddval = npheap_delete(NPHFS_DATA->devfd,search_result->data_offset);
  }
  log_msg("Return values for bitmaps d - \"%d\" and f -  \"%d\" \n",retdval,retfsval);
  log_msg("Return values for npheap delete d - \"%d\" and f -  \"%d\" \n",ddval,dfsval);
  munmap(search_result,8192);
  return 0;
} 
else{
  log_msg("Cannot delete: File \"%s\" is in use\n",search_result->filename);
  return -EBUSY;
}
 log_msg("Error in deleting link for \"%s\" \n",search_result->filename);
 return -1; 
}

/** Remove a directory */
int nphfuse_rmdir(const char *path)
{
  log_msg("In rmdir for path \"%s\"\n",path);
  int retval = 1;
  char *filename;
  char *parent_path;
  struct nphfs_file *parent;
  struct nphfs_file *search_result;
  if (path == NULL){
    log_msg("ENOENT for path in rmdir\n");
    return -ENOENT;
  }
  search_result = search(path);
  if (search_result == NULL){
    log_msg("Directory \"%s\" doesn't exist\n ",path);
    return -ENOENT ;
  }
  if(! search_result->fdflag){
    log_msg("Failed \"%s\" is a file.\n ",path);
    return -ENOTDIR;
  }
  filename = split_path(path);
  parent_path = split_parent(path,filename);
  log_msg("Parent : \"%s\" for \"%s\"\n",parent_path,path);
  parent = search(parent_path);
  log_msg("parent found for d \"%s\"\n",parent->path);
  if (parent == NULL){
    log_msg("Cannot create \"%s\" : Parent not found for \"%s\"\n",path,parent_path);
    return -ENOENT;
  }
  //Find out whether the directory is empty 
  int i = 2 ;
  bool *temp = fsbitmap + 2;
  struct nphfs_file *node;
  while(i < 8192){
   if (*temp){
    node = npheap_alloc(NPHFS_DATA -> devfd,i,8192);
    if(strcmp(node->parent_path,path) == 0){
     log_msg("Directory \"%s\" not empty\n ",path);
     return -ENOTEMPTY;
   }
 }
 i++;
 temp++;
 munmap(node,8192);
}
//Deleting the directory if it's empty.
retval = npheap_delete(NPHFS_DATA->devfd,search_result->fs_offset);
set_bitmap(0,search_result->fs_offset,false);
parent->metadata.st_size -= 64;
parent->metadata.st_nlink --;
parent->metadata.st_ctime = (time_t)time(NULL);
munmap(parent,8192);
munmap(search_result,8192);
log_msg("Directory \"%s\" should be deleted and retval is \"%d\" \n ",path,retval);
return retval;
}

/** Create a symbolic link */
// The parameters here are a little bit confusing, but do correspond
// to the symlink() system call.  The 'path' is where the link points,
// while the 'link' is the link itself.  So we need to leave the path
// unaltered, but insert the link into the mounted directory.
int nphfuse_symlink(const char *path, const char *link)
{
  log_msg("In symlink for link \"%s\" to path \"%s\"\n",link,path);
  int fsretval;
  int dretval;
  int fs_bmoffset;
  int d_bmoffset;
  char *filename;
  char *parent_path;
  struct fuse_context *context;
  struct nphfs_file *parent;
  struct nphfs_file *oldfile = search(path);
  struct nphfs_file *newfile = search(link);
  
  if(newfile != NULL){
    log_msg("\"%s\" : File already exists \n",path);
    return -EEXIST; 
  }
    //Create a new file and copy the data_offset and metadata.
  filename = split_path(link);
  parent_path = split_parent(link,filename);
  log_msg("Parent : \"%s\" for \"%s\"\n",parent_path,link);
  parent = search(parent_path);
  if (parent == NULL){
    log_msg("Cannot create \"%s\" : Parent not found for \"%s\"\n",path,parent_path);
    return -ENOENT;
  }
  if((parent != NULL) && (!parent->fdflag)){
    log_msg("Cannot create \"%s\" : Parent  \"%s\" is not a directory\n",path,parent_path);
    return -ENOTDIR;
  }
  fs_bmoffset = search_bitmap(0);
  if(fs_bmoffset == -1){
     log_msg("Fbitmap returned -1 \n");
    return -ENOMEM;
  }
  //d_bmoffset = search_bitmap(1);
  log_msg("Creating new node for \"%s\" with parent \"%s\"\n",link,parent_path);
  newfile = npheap_alloc(NPHFS_DATA->devfd,fs_bmoffset,8192);
  fsretval = set_bitmap(0,fs_bmoffset,true);
  //dretval = set_bitmap(1,d_bmoffset,true);
  log_msg("Return values for bitmaps f -  \"%d\" \n",fsretval);
  initialize_newnode(newfile);
  strcpy(newfile->path,link);
  strcpy(newfile->parent_path,parent_path);
  newfile->fs_offset = fs_bmoffset;
 //Changing data offset to that of oldfile to make it a symlink.
 //newfile->data_offset = oldfile->data_offset;
 //Symlink_path should be updated to differentiate between hard and soft links.
  strcpy(newfile->symlink_path,path);

  newfile->metadata.st_nlink = 1;
 //Should increment numlink in parent.
 parent->metadata.st_nlink ++;
 parent->metadata.st_ctime = (time_t)time(NULL);

 context = fuse_get_context();
 newfile->metadata.st_size = strlen(path);
 newfile->metadata.st_mode = S_IFLNK | 0777; 
 newfile->metadata.st_uid = context->uid;
 newfile->metadata.st_gid = context->gid;
 newfile->metadata.st_atime = (time_t)time(NULL);
 newfile->metadata.st_mtime = (time_t)time(NULL);
 newfile->metadata.st_ctime = (time_t)time(NULL);
  //fdflag = 0 for files
 newfile->fdflag = 0; 
 strcpy(newfile->filename,filename);
 log_msg("Link for path \"%s\" with has filename \"%s\"\n",link,newfile->filename);
 //Make the link 
 munmap(newfile,8192);
 munmap(parent,8192);

 return 0;
}


/** Rename a file */
// both path and newpath are fs-relative
int nphfuse_rename(const char *path, const char *newpath)
{
   log_msg("In rename from path \"%s\" to \"%s\"\n",path,newpath);
   char *filename;
   char *parent_path;
   struct nphfs_file *search_result = search(path);
   struct nphfs_file *old_parent;
   struct nphfs_file *new_parent;
        if(search_result != NULL)
        {  
           old_parent = search(search_result->parent_path);
           //Updating path,parent and filename of the file
           filename = split_path(newpath );
           parent_path = split_parent(newpath,filename);
           log_msg("Parent : \"%s\" for \"%s\"\n",parent_path,path);
           new_parent = search(parent_path);
           if (new_parent == NULL){
             log_msg("Cannot create \"%s\" : Parent  \"%s\"not found\n",newpath,parent_path);
             return -ENOTDIR;
            }
           if((new_parent != NULL) && (!new_parent->fdflag)){
             log_msg("Cannot create \"%s\" : Parent  \"%s\" is not a directory\n",newpath,parent_path);
             return -ENOTDIR;
            }
           strcpy(search_result->filename,filename);
           strcpy(search_result->parent_path,parent_path);
           strcpy(search_result->path,newpath);
           //Updating link counts of old parent and new parent.
           old_parent->metadata.st_nlink --;
           new_parent->metadata.st_nlink ++;
           old_parent->metadata.st_size -= search_result->metadata.st_size;
           new_parent->metadata.st_size += search_result->metadata.st_size;
           old_parent->metadata.st_ctime = (time_t)time(NULL);
           new_parent->metadata.st_ctime = (time_t)time(NULL);
           munmap(search_result,8192);
           munmap(old_parent,8192);
           munmap(new_parent,8192);
           return 0;
        }

    log_msg("Search for path \"%s\" returned null\n",path);  
    return -1;
}

/** Create a hard link to a file */
int nphfuse_link(const char *path, const char *newpath)
{
  log_msg("In link for path \"%s\" to \"%s\"\n",newpath,path);
  int fsretval;
  int dretval;
  int fs_bmoffset;
  int d_bmoffset;
  char *filename;
  char *parent_path;
  //struct fuse_context *context;
  struct nphfs_file *parent;
  struct nphfs_file *oldfile = search(path);
  struct nphfs_file *newfile = search(newpath);
  if(oldfile == NULL){
    log_msg("\"%s\" : No such file or directory \n",path);
    return -ENOENT; 
  }
  if(newfile != NULL){
    log_msg("\"%s\" : File already exists \n",newpath);
    return -EEXIST; 
  }
    //Create a new file and copy the data_offset and metadata.
  filename = split_path(newpath);
  parent_path = split_parent(newpath,filename);
  parent = search(parent_path);
  log_msg("Parent : \"%s\" for \"%s\"\n",parent_path,newpath);
  if (parent == NULL){
    log_msg("Cannot create \"%s\" : Parent not found for \"%s\"\n",newpath,parent_path);
    return -ENOTDIR;
  }
  if((parent != NULL) && (!parent->fdflag)){
    log_msg("Cannot create \"%s\" : Parent  \"%s\" is not a directory\n",newpath,parent_path);
    return -ENOTDIR;
  }
  fs_bmoffset = search_bitmap(0);
  if(fs_bmoffset == -1){
     log_msg("Fbitmap returned -1 \n");
    return -ENOMEM;
  }
  //d_bmoffset = search_bitmap(1);
  log_msg("Creating new node for \"%s\" with parent \"%s\"\n",path,parent_path);
  newfile = npheap_alloc(NPHFS_DATA->devfd,fs_bmoffset,8192);
  fsretval = set_bitmap(0,fs_bmoffset,true);
  //dretval = set_bitmap(1,d_bmoffset,true);
  log_msg("Return values for bitmaps  f -  \"%d\" \n",fsretval);
 initialize_newnode(newfile);
 strcpy(newfile->path,newpath);
 strcpy(newfile->parent_path,parent_path);
 newfile->fs_offset = fs_bmoffset;
 //Changing data offset since this is a file.
 //Update metadata will handle this for the link. 
 oldfile->metadata.st_nlink ++;
 oldfile->metadata.st_ctime = (time_t)time(NULL);

 //Should increment numlink in parent.
 parent->metadata.st_nlink ++;
 parent->metadata.st_ctime = (time_t)time(NULL);

  //fdflag = 0 for files
 newfile->fdflag = 0; 
 strcpy(newfile->filename,filename);
 log_msg("Link for path \"%s\" with has filename \"%s\"\n",newpath,newfile->filename);
 //Make the link and update link counts
 newfile->data_offset = oldfile->data_offset;
 newfile->metadata = oldfile->metadata;
 update_metadata(oldfile->data_offset,oldfile->metadata);
 munmap(newfile,8192);
 munmap(parent,8192);
 munmap(oldfile,8192);
 return 0;
}

/** Change the permission bits of a file */
int nphfuse_chmod(const char *path, mode_t mode)
{
  log_msg("In nphfuse_chmod\n");
  struct nphfs_file *search_result;
  if (path == NULL){
    log_msg("ENOENT for path in chmod\n");
    return -ENOENT;
  }
  
  search_result = search(path);
  log_msg("search for path \"%s\" of length \"%d\" returned \"%s\"  \n",path,strlen(path),search_result->path);
  if(search_result != NULL){
    search_result->metadata.st_mode = mode;
    log_msg("Changed mode for  \"%s\" \n",path);
    search_result->metadata.st_ctime = (time_t)time(NULL);
    if(!search_result->fdflag){
    update_metadata(search_result->data_offset,search_result->metadata);
    }
    munmap(search_result,8192);
    return 0;
  }
    log_msg("Chmod returns -1 for  \"%s\" \n",path);
    return -ENOENT;
}

/** Change the owner and group of a file */
int nphfuse_chown(const char *path, uid_t uid, gid_t gid)
{
  log_msg("In chown\n");
  struct nphfs_file *search_result;
  if (path == NULL){
    log_msg("ENOENT for path in chown\n");
    return -ENOENT;
  }
  
  search_result = search(path);
  log_msg("search for path \"%s\" of length \"%d\" returned \"%s\"  \n",path,strlen(path),search_result->path);
  if(search_result != NULL){
    if(uid != -1){
    search_result->metadata.st_uid = uid;
    log_msg("Changed ownership for  \"%s\" \"%ld\" \n",path,uid);
  }
    if(gid != -1){
    search_result->metadata.st_gid = gid;
    log_msg("Changed ownership for  \"%s\" \"%ld\" \n",path,gid);
  }
    search_result->metadata.st_ctime = (time_t)time(NULL);
    if(!search_result->fdflag){
    update_metadata(search_result->data_offset,search_result->metadata);
    }
    munmap(search_result,8192);
    return 0;
  }
    log_msg("Chmod returns -1 for  \"%s\" \n",path);
    return -ENOENT;
}


/** Change the size of a file */
int nphfuse_truncate(const char *path, off_t newsize)
{
   log_msg("In truncate for path \"%s\"\n",path);
   struct nphfs_file *search_result;
   char *filename;
   char *parent_path;
   struct nphfs_file *parent;
   char *data;
   int filesize=0;
  if (path == NULL){
    log_msg("ENOENT for path in read\n");
    return -ENOENT;
  }
  filename = split_path(path);
  parent_path = split_parent(path,filename);
  parent = search(parent_path);
  if (parent == NULL){
    log_msg("Cannot create \"%s\" : Parent not found for \"%s\"\n",path,parent_path);
    return -ENOENT;
  }
  search_result = search(path);
  log_msg("search for path \"%s\" of length \"%d\" returned \"%s\" with fdflag \"%d\" \n",path,strlen(path),search_result->path,search_result->fdflag);
  if(search_result != NULL){
    filesize = search_result->metadata.st_size;
    data = npheap_alloc(NPHFS_DATA->devfd,search_result->data_offset,8192);
    failcount ++;
    log_msg("Truncate for oldsize \"%d\" and size \"%d\"\n",filesize,newsize); 
    if(filesize > newsize){
      memset(data+newsize,'\0',8192-newsize);
      search_result->metadata.st_size = newsize;
      parent->metadata.st_size -= (filesize-newsize);
      update_metadata(search_result->data_offset,search_result->metadata);
      log_msg("Truncated data for  \"%s\" of size \"%d\"\n",path,newsize);
      munmap(search_result,8192);
      munmap(data,8192);
      munmap(parent,8192);
      return 0;  
    }
    else {
      if(newsize > 8192){
        log_msg("Maximum file size is 8192\n");
        return -EINVAL;
      }

      memset(data+filesize,'\0',8192-filesize);
      search_result->metadata.st_size = newsize;
      search_result->metadata.st_ctime = (time_t)time(NULL);
      parent->metadata.st_size += (newsize-filesize);
      parent->metadata.st_ctime = (time_t)time(NULL);

      update_metadata(search_result->data_offset,search_result->metadata);
      log_msg("filesize < newsize  \"%d\" \n",path);
      munmap(search_result,8192);
      munmap(data,8192);
      munmap(parent,8192);
      return 0;
    }
  }
    log_msg("File not found for  \"%s\" \n",path);
    return -ENOENT;
}

/** Change the access and/or modification times of a file */
int nphfuse_utime(const char *path, struct utimbuf *ubuf)
{
  log_msg("In utime\n");
  struct nphfs_file *search_result;
  if (path == NULL){
    log_msg("ENOENT for path in utime\n");
    return -ENOENT;
  }
  
  search_result = search(path);
  log_msg("search for path \"%s\" of length \"%d\" returned \"%s\"  \n",path,strlen(path),search_result->path);
  if(search_result != NULL){
    if(ubuf != NULL){
     search_result->metadata.st_atime = ubuf->actime;
     search_result->metadata.st_mtime = ubuf->modtime; 
    }
    else{
     search_result->metadata.st_atime = (time_t)time(NULL);
     search_result->metadata.st_mtime = (time_t)time(NULL); 
    }
    search_result->metadata.st_ctime = (time_t)time(NULL);
    log_msg("Changed utime for  \"%s\" \n",path);
    if(!search_result->fdflag){
    update_metadata(search_result->data_offset,search_result->metadata);
    }
    munmap(search_result,8192);
    return 0;
  }
    log_msg("utime returns -1 for  \"%s\" \n",path);
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
    log_msg("In open for path \"%s\"\n",path);
    /*if ((fi->flags & O_ACCMODE) != O_RDONLY){
      log_msg("Not sure about this\n");
      return -EACCES;
    }*/
    struct nphfs_file *search_result;
  if (path == NULL){
    log_msg("ENOENT for path in chmod\n");
    return -ENOENT;
  }
   search_result = search(path);
  log_msg("search for path \"%s\" of length \"%d\" returned \"%s\" with fdflag \"%d\" \n",path,strlen(path),search_result->path,search_result->fdflag);
  if(search_result != NULL){
    search_result->pin_count++;
    log_msg("Changed pin_count for  \"%s\" \n",path);
    munmap(search_result,8192);
    return 0;
  }
    log_msg("Open returns -1 for  \"%s\" \n",path);
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
   log_msg("In Read for path \"%s\"\n",path);
   int i = 0;
   int filesize = 0;
   struct nphfs_file *search_result;
   char *data;
  if (path == NULL){
    log_msg("ENOENT for path in read\n");
    return -ENOENT;
  }
  search_result = search(path);
  log_msg("search for path \"%s\" of length \"%d\" returned \"%s\" with fdflag \"%d\" \n",path,strlen(path),search_result->path,search_result->fdflag);
  if(search_result != NULL){
    filesize = search_result->metadata.st_size;
    data = npheap_alloc(NPHFS_DATA->devfd,search_result->data_offset,8192);
    failcount ++;
    data = data + offset;
    log_msg("Read for offset \"%d\" and size \"%d\" and filesize\"%d\"\n",offset,size,filesize); 
    if(filesize > offset+size){
      memcpy(buf,data,size);
      log_msg("Copied data for  \"%s\" of size \"%d\"\n",path,size);
      munmap(search_result,8192);
      munmap(data,8192);
      return size;  
    }
    else if (filesize > offset){
      memcpy(buf,data,filesize - offset);
      //memcpy(buf+search_result->filesize - offset,0,size-(search_result->filesize - offset));
      log_msg("Copied data for  \"%s\" of size \"%d\"\n",path,(filesize - offset));
      munmap(search_result,8192);
      munmap(data,8192);
      return filesize - offset;
    }
    else {
      log_msg("Reached EOF for offset \"%d\" and size \"%d\" and filesize\"%d\"\n",offset,size,filesize);
      return -EINVAL;
    }
  }
    log_msg("Open returns -1 for  \"%s\" \n",path);
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
   log_msg("In write for path \"%s\"\n",path);
   struct nphfs_file *search_result;
   struct nphfs_file *parent;
   char *filename;
   char *parent_path;
   char *data;
   int filesize=0;
  if (path == NULL){
    log_msg("ENOENT for path in read\n");
    return -ENOENT;
  }
  filename = split_path(path);
  parent_path = split_parent(path,filename);
  parent = search(parent_path);
  if (parent == NULL){
    log_msg("Cannot create \"%s\" : Parent not found for \"%s\"\n",path,parent_path);
    return -ENOENT;
  }
  search_result = search(path);
  log_msg("search for path \"%s\" of length \"%d\" returned \"%s\" with fdflag \"%d\" \n",path,strlen(path),search_result->path,search_result->fdflag);
  if(search_result != NULL){
    filesize = search_result->metadata.st_size;
    data = npheap_alloc(NPHFS_DATA->devfd,search_result->data_offset,8192);
    failcount ++;
    log_msg("Write for offset \"%d\" and size \"%d\" and filesize\"%d\"\n",offset,size,filesize); 
    if(offset+size < 8192){
      data = data + offset;
      memcpy(data,buf,size);
      search_result->metadata.st_size += size;
      update_metadata(search_result->data_offset,search_result->metadata);
      parent->metadata.st_size += size;
      log_msg("Copied data for  \"%s\" of size \"%d\"\n",path,size);
      munmap(search_result,8192);
      munmap(data,8192);
      munmap(parent,8192);
      return size;  
    }
    else {
      log_msg("Reached 8K limit for offset \"%d\" and size \"%d\" and filesize\"%d\"\n",offset,size,filesize);
      munmap(search_result,8192);
      munmap(data,8192);
      munmap(parent,8192);
      return 0;
    }
  }
    log_msg("File not found for  \"%s\" \n",path);
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
  log_msg("In statfs\n");
  int i=0;
  int cnt=0;
  bool *temp =  dbitmap;
  statv->f_bsize =8192;
  statv->f_blocks = 8192;
  //log_msg("bszie is \"%d\" \"%d\"\n",statv->f_bsize,statv->f_blocks);
  while(i<8192)
  {
    if(*temp)
     cnt++;
   i++;
   temp++;
 }
 statv->f_bfree = 8192-cnt  ;
 statv->f_bavail = 8192-cnt ;
 //log_msg("f_bfree is \"%d\" \"%d\"\n",statv->f_bfree,statv->f_bavail);
 cnt=0;
 i = 2;
 temp =  fsbitmap + 2;
 while(i<8192)
 {
  if(*temp)
   cnt++;
 i++;
 temp++;
} 
statv->f_files = 8190;    
statv->f_ffree = 8190 - cnt; 
//log_msg("f_files is \"%d\" \"%d\"\n",statv->f_files,statv->f_ffree);   
statv->f_namemax = 250; 
log_statvfs(statv);
return 0; 
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
    log_msg("In Release for path \"%s\"\n",path);
    struct nphfs_file *search_result;
  if (path == NULL){
    log_msg("ENOENT for path in chmod\n");
    return -ENOENT;
  }
   search_result = search(path);
  log_msg("search for path \"%s\" of length \"%d\" returned \"%s\" with fdflag \"%d\" \n",path,strlen(path),search_result->path,search_result->fdflag);
  if(search_result != NULL){
    search_result->pin_count--;
    log_msg("Changed pin_count for  \"%s\" \n",path);
    return 0;
  }
    log_msg("Release returns -1 for  \"%s\" \n",path);
    return -ENOENT;
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
    log_msg("In fsync\n");
    return -1;
}

#ifdef HAVE_SYS_XATTR_H
/** Set extended attributes */
int nphfuse_setxattr(const char *path, const char *name, const char *value, size_t size, int flags)
{
    log_msg("In nphfuse_setxattr\n"); 
    return -61;
}

/** Get extended attributes */
int nphfuse_getxattr(const char *path, const char *name, char *value, size_t size)
{
    log_msg("In getxattr\n"); 
    return -61;
}

/** List extended attributes */
int nphfuse_listxattr(const char *path, char *list, size_t size)
{
    log_msg("In nphfuse_listxattr\n"); 
    return -61;
}

/** Remove extended attributes */
int nphfuse_removexattr(const char *path, const char *name)
{
    log_msg("In removexattr\n"); 
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
   log_msg("In opendir for path \"%s\"\n",path);
   struct nphfs_file *search_result;
  if (path == NULL){
    log_msg("ENOENT for path \"%s\" in opendir\n",path);
    return -ENOENT;
  }
  search_result = search(path);
  if(search_result != NULL){
    search_result->pin_count++;
    log_msg("Search result is  \"%s\" with fdflag  \"%d\"\n ",search_result->path,search_result->fdflag);
    return 0;
  }
 log_msg("directory \"%s\" doesn't exist\n ",path);
 return -ENOENT;
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
   char *filename;
   char *parent_path;
   struct nphfs_file *parent;
   int retval =1;

  log_msg("In readdir for path \"%s\"\n",path);
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
  filename = split_path(path);
  parent_path = split_parent(path,filename);
  parent = search(parent_path);
  if (parent == NULL){
    log_msg("Cannot create \"%s\" : Parent not found for \"%s\"\n",path,parent_path);
    return -ENOENT;
  }
  retval = filler(buf,".",&search_result->metadata,0);
  retval = filler(buf,"..",&parent->metadata,0);
  
  //Fill all the nodes with parent_path=path in buffer 
  int i = 2 ;
  bool *temp = fsbitmap + 2;
  while(i < 8192){
   if (*temp){
    search_result = npheap_alloc(NPHFS_DATA -> devfd,i,8192);
    if(strcmp(search_result -> parent_path,path)==0){
     if (!filler(buf,search_result->filename,&search_result->metadata,0)){
      log_msg("File \"%s\" name copied to buffer\n",search_result->filename);
    }
    else {
      log_msg("Filler fail/buffer overflow for File \"%s\" ",search_result->filename);      
    } 
  }
}
i++;
temp++;
munmap(search_result,8192);
}
munmap(parent,8192);
return 0;
}

/** Release directory
 */
int nphfuse_releasedir(const char *path, struct fuse_file_info *fi)
{
  log_msg("In Releasedir for path \"%s\"\n",path);
  struct nphfs_file *search_result;
  if (path == NULL){
    log_msg("ENOENT for path \"%s\" in Releasedir\n",path);
    return -ENOENT;
  }
  search_result = search(path);
  if(search_result != NULL){
    search_result->pin_count--;
    log_msg("Search result is  \"%s\" with fdflag  \"%d\" and filename  \"%s\" and fs_offset \"%d\" and parent_path \"%s\"\n ",search_result->path,search_result->fdflag,search_result->filename,search_result->fs_offset,search_result->parent_path);    
    munmap(search_result,8192);
return 0;
  }
 log_msg("directory \"%s\" doesn't exist\n ",path);
 return -ENOENT;
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
    log_msg("In fsyncdir\n"); 
    return 0;
}

int nphfuse_access(const char *path, int mask)
{
    log_msg("In access with path \"%s\"\n",path);
    struct nphfs_file *search_result;
    search_result = search(path);
    if (search_result == NULL){
    log_msg("ENOENT for path in access\n");
    return -ENOENT;
  }
  if (mask & (1 << 1))
  {
    log_msg("looking for r permission \n");
    if (!search_result->metadata.st_mode & (S_IWUSR | S_IWGRP | S_IWOTH))
      return -EACCES;
  }
  if (mask & (1 << 2))
  {
    log_msg("looking for w permission \n");
    if (!search_result->metadata.st_mode & (S_IRUSR | S_IRGRP | S_IROTH))
      return -EACCES;
  }
  if (mask & 1)
  {
    log_msg("looking for x permission \n");
    if (!search_result->metadata.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))
      return -EACCES;
  }
     log_msg("Access ok  for \"%s\"\n",path);
     return 0;
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
    log_msg("In ftruncate for path \"%s\"\n",path);
    int retval = 1;
    retval = nphfuse_truncate(path,offset); 
    log_msg("Ftruncate for path \"%s\" returned \"%d\"\n",path,retval);
    return retval;
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
    log_msg("In fgetattr\n"); 
    int retval = 1; 
    retval = nphfuse_getattr(path,statbuf); 
    log_msg("fgetattr for path \"%s\" returned \"%d\"\n",path,retval);
    return retval;
}

void *nphfuse_init(struct fuse_conn_info *conn)
{
  log_msg("\nnphfuse_init()\n");
  log_conn(conn);
  log_fuse_context(fuse_get_context());
  fsbitmap = npheap_alloc(NPHFS_DATA -> devfd,0,8192);
  dbitmap = npheap_alloc(NPHFS_DATA -> devfd,1,8192);
  failcount = 2;
  root = malloc(sizeof(struct nphfs_file));
  struct fuse_context *context = fuse_get_context();
  initialize_newnode(root);
  root->fs_offset = 2;
  root->fdflag = 1;
  root->metadata.st_mode = S_IFDIR | 0777; 
  root->metadata.st_size = 8192;
  root->metadata.st_nlink = 1;
  root->metadata.st_uid = context->uid;
  root->metadata.st_gid = context->gid;
  root->metadata.st_atime = (time_t)time(NULL);
  root->metadata.st_mtime = (time_t)time(NULL);
  root->metadata.st_ctime = (time_t)time(NULL);
  strcpy(root->path,"/");
  strcpy(root->filename,"root");
  /*log_msg("Line 574\n");
  if(!set_bitmap(0,2,true)){
    log_msg("Root created with path \n");
  } 
  struct nphfs_file *root = npheap_alloc(NPHFS_DATA -> devfd,2,8192);
  struct nphfs_file *search_result;
  
  log_msg("Line 579\n");
  initialize_newnode(root);
  log_msg("Initialised root \"%s\" with fdflag  \"%d\" and filename  \"%s\" and fs_offset \"%d\" and parent_path \"%s\"\n ",root->path,root->fdflag,root->filename,root->fs_offset,root->parent_path);
  log_msg("Line 581\n");
  root->fs_offset = 2;
  root->metadata.st_mode = S_IFDIR | 0777; 
  root->metadata.st_nlink = 1;
  root->metadata.st_size = 8192;
  root->metadata.st_uid = context->uid;
  root->metadata.st_gid = context->gid;
  root->metadata.st_atime = (time_t)time(NULL);
  root->metadata.st_mtime = (time_t)time(NULL);
  root->metadata.st_ctime = (time_t)time(NULL);
  log_msg("Line 590\n");
  //fdflag = 1 for directories
  root->fdflag = 1; 
  strcpy(root->path,"/");
  strcpy(root->filename,"root");
  log_msg("Root path \"%s\" and fdflag \"%d\"\n",root->path,root->fdflag);
  log_msg("Finalised root \"%s\" with fdflag  \"%d\" and filename  \"%s\" and fs_offset \"%d\" and parent_path \"%s\"\n ",root->path,root->fdflag,root->filename,root->fs_offset,root->parent_path);
  search_result = search(root->path);
  log_msg("Search result is  \"%s\" with fdflag  \"%d\" and filename  \"%s\" and fs_offset \"%d\" and parent_path \"%s\"\n ",search_result->path,search_result->fdflag,search_result->filename,search_result->fs_offset,search_result->parent_path);  
  */
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
    free(root);
    log_msg("................................................\n");
}
