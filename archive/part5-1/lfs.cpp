/*
    FUSE: FS in Userspace
    Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

    This program can be distributed under the terms of the GNU GPL.
    See the file COPYING.

    gcc -Wall `pkg-config fuse --cflags --libs` -lulockmgr fuselfs_fh.c -o fuselfs_fh
*/

#define FUSE_USE_VERSION 26

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define _GNU_SOURCE

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fstream>
#include <string.h>
#include <map>
#include <iostream>
#include "inode.h"
#include "testcases.h"
#include "lfs.h"
#define BLOCK_SIZE 1024
#define MAX_FILES 1000 //Since we have 25*40 inodes only 1000 files can be supported
#define TOTAL_INODE_BLOCKS 40
#define MAX_FILE_SIZE 8192
#define TOTAL_BLOCKS 8192

unsigned long LOG_SIZE;
int fp;
int LOG_FP;
std::map<std::string,int> file_map;
std::map<int,inode> inodes;
std::ofstream debug;
std::bitset<TOTAL_BLOCKS> bitmap;

LogStructuredFileSystem FS;

const char *get_file_name(const char* path)
{
    const char *ptr=path;
    //ptr=path;

    while(*ptr != '\0')
        ++ptr;
    while(*ptr != '/')
        ptr--;
    return ++ptr;
}



void init()
{
   FS.init();
   //debug<<"\n\n Init: ";
   //test_inodes(debug,FS.LOG_FP);

}


static int lfs_getattr(const char *path, struct stat *stbuf)
{
    int res = 0;
    const char* fname;
    int count;
    int value;

    memset(stbuf, 0, sizeof(struct stat));
    //Check if the path is for the root directory
    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0777;
        stbuf->st_nlink = 2;
        return res;
    } else {
        //check in the file map for the filename given in the path
        for(auto const& i: FS.file_map)
        {   
            fname=get_file_name(path);//get filename from the path
            std::string filename(fname);
            if(FS.file_map.count(filename) != 0)//Not such key exists
            {   
                stbuf->st_mode = S_IFREG | 0666;
                stbuf->st_nlink = 1;
                //Get key from filename to index into inode map.e.g file "1.txt" has inode 1
                value=FS.file_map[filename];
                stbuf->st_size = FS.inodes[value].file_size;
                return res;
            }            
        }
    } 
    //No such entry
    return -ENOENT;

}

static int lfs_fgetattr(const char *path, struct stat *stbuf,
                       struct fuse_file_info *fi)
{   
    int res = 0;
    const char* fname;
    int count;
    int value;

    memset(stbuf, 0, sizeof(struct stat));
    //Check if the path is for the root directory
    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0777;
        stbuf->st_nlink = 2;
        return res;
    } else {
        //check in the file map for the filename given in the path
        for(auto const& i: FS.file_map)
        {   
            fname=get_file_name(path);//get filename from the path
            std::string filename(fname);
            if(FS.file_map.count(filename) != 0)//Not such key exists
            {   
                stbuf->st_mode = S_IFREG | 0666;
                stbuf->st_nlink = 1;
                //Get key from filename to index into inode map.e.g file "1.txt" has inode 1
                value=FS.file_map[filename];
                stbuf->st_size = FS.inodes[value].file_size;
                return res;
            }            
        }
    } 
    //No such entry
    return -ENOENT;
}


int lfs_statfs(const char* path, struct statvfs* stbuf)
{
    memset(stbuf, 0, sizeof(struct statvfs));
    stbuf->f_bsize=FS.block_size;
    stbuf->f_frsize=FS.block_size;
    stbuf->f_blocks=1000;
    stbuf->f_bfree=1000;
    stbuf->f_bavail=1000;
    stbuf->f_files=40;
    stbuf->f_ffree=40;
    stbuf->f_favail=40;
    stbuf->f_namemax=128; 
    return 0;
}


static int lfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
             off_t offset, struct fuse_file_info *fi)
{
    int nblocks=LOG_SIZE/BLOCK_SIZE;
    int c=0;

    (void) offset;
    (void) fi;


    if (strcmp(path, "/") != 0)
        return -ENOENT;

    //Add current and parent entries
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    //Add files from the file_map
    for(auto const &i: FS.file_map)
    {   
        // if(i.first.compare(FS.dirname) == 0)
        //     continue;

        filler(buf,i.first.c_str(),NULL,0);

                
    }


    return 0;
}


static int lfs_open(const char *path, struct fuse_file_info *fi)
{
    std::string FileName(get_file_name(path));
    //debug<<"Open "<<FileName<<std::endl;

    if(FS.file_exists(FileName))
    {
        //debug<<"Open Succuess\n";
        return 0;
    }


    return -ENOENT;

}

static int lfs_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi)
{
    size_t len;
    (void) fi;
    const char* fname=get_file_name(path);
    std::string filename(fname);
    // debug<<"Read "<<fname<<std::endl;
    // //if the file exists
    
    return FS.fsread(filename,buf,size,offset);
}

static int lfs_write(const char *path, const char *buf, size_t size,
                     off_t offset, struct fuse_file_info *fi)
{

    const char* fname=get_file_name(path);
    std::string filename(fname);
    
    int bytes=FS.fswrite(filename,buf,size,offset);
    // FS.write_directory_to_log();
    // FS.read_directory_from_log();
    return bytes;
}



static void lfs_destroy (void * private_data)
{   

    FS.write_inodes_to_log();
    FS.write_bitmap_to_log();
    FS.write_directory_to_log();
    
    debug<<"Destroy called ... Closing FS \n";
    close(fp);
    debug.close();
}

static int lfs_truncate (const char* path, off_t size)
{
    
    if(size > FS.max_file_size)
        return -EFBIG;

    const char* fname=get_file_name(path);
    std::string filename(fname);
    
    debug<<"Truncate: "<<filename<<" "<<size<<std::endl;
    if(FS.file_exists(filename))
    {
        //FS.truncate(filename,size);
        return 0;
    }
    else
        return -ENOENT;

    return 0;
}


int lfs_create(const char *path, mode_t mode,struct fuse_file_info *fi)
{
    std::string FileName(get_file_name(path));
    if(FS.file_exists(FileName))
        return 0;

    FS.create(FileName);

    debug<<"Create "<<path<<std::endl;
   // debug<<"File Map Size:"<<FS.file_map.size()<<std::endl;

    return 0;
}

static int lfs_unlink(const char* path)
{
    debug<<"unlink "<<path<<std::endl;
    return 1;
}

static int lfs_access(const char*path ,int mask)
{

    std::string FileName(get_file_name(path));
    debug<<"Access "<<FileName<<std::endl;
    if(FS.file_exists(FileName))
        return 0;
    return -ENOENT;
}
static struct fuse_operations lfs_oper = {
    .getattr    = lfs_getattr,
    .fgetattr   = lfs_fgetattr,
    .open   = lfs_open,
    .read   = lfs_read,
    .write  = lfs_write,
    .destroy= lfs_destroy,
    .readdir=lfs_readdir,
    .truncate = lfs_truncate,
    .create =lfs_create,
    .unlink = lfs_unlink,
    .statfs=lfs_statfs,
};



int main(int argc, char *argv[])
{
    std::ifstream in("lfslog", std::ifstream::ate | std::ifstream::binary);
    FS.log_size = in.tellg();


    debug.open("debug.txt",std::fstream::trunc);
    FS.debug=&debug;
    FS.LOG_FP= open("lfslog",O_RDWR);

    init();
    umask(0);
    return fuse_main(argc, argv, &lfs_oper, NULL);
}
