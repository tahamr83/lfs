/*
    FUSE: Filesystem in Userspace
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
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#include <fstream>
#include <string.h>
#include <map>
#include <iostream>
#define BLOCK_SIZE 1024
static const char *hello_str = "Hello World!\n";
static const char *hello_path = "/hello";

unsigned long LOG_SIZE;
int fp;
int LOG_FP;
std::map<std::string,int> file_map;

static void LOG(const char * msg)
{
    
    write(fp,msg,strlen(msg));
    
}
static void LOG(int i)
{
    std::string s=std::to_string(i);
    const char *p=s.c_str();
    write(fp,p,strlen(p));
    
}

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
    int nblocks=LOG_SIZE/BLOCK_SIZE;
    std::string s;
    
    for(int i=0;i<nblocks;++i)
    {
        s=std::to_string(i);
        s=s+".txt";

        file_map[s]=i;

    }

    //  LOG("File Map size:");
    // LOG(file_map.size());
    // LOG("\n");
    
}


static int lfs_getattr(const char *path, struct stat *stbuf)
{
    int res = 0;
    const char* fname;
    int count;

    memset(stbuf, 0, sizeof(struct stat));
    //Check if the path is for the root directory
    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0777;
        stbuf->st_nlink = 2;
        return res;
    } else {
        //check in the file map for the filename given in the path
        for(auto const& i: file_map)
        {   
            fname=get_file_name(path);//get filename from the path
            std::string filename(fname);
            if(file_map.count(filename) != 0)//Not such key exists
            {
                stbuf->st_mode = S_IFREG | 0666;
                stbuf->st_nlink = 1;
                stbuf->st_size = BLOCK_SIZE;
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
    return -ENOENT;
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
    
    for(auto const &i: file_map)
    {   
        filler(buf,i.first.c_str(),NULL,0);
        
    }

    // LOG("File Map size:");
    // LOG(file_map.size());
    // LOG("\n");
    return 0;
}


static int lfs_open(const char *path, struct fuse_file_info *fi)
{

    // if (strcmp(path, hello_path) != 0)
    //     return -ENOENT;

    // if ((fi->flags & 3) != O_RDONLY)
    //     return -EACCES;

    return 0;

}

static int lfs_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi)
{
    size_t len;
    (void) fi;
    const char* fname=get_file_name(path);
    std::string filename(fname);

    //Each file if only of size BLOCK_SIZE. Any read that exceeds this size 
    //is an over flow
    if(offset + size > BLOCK_SIZE)
        return -EOVERFLOW;

    //if the filename exists
    if(file_map.count(filename) != 0)
    {
        //get the block num i.e key
        int block_no=file_map.find(filename)->second;
        block_no=file_map[filename];
        lseek(LOG_FP,(block_no*BLOCK_SIZE) + offset ,SEEK_SET);
        int bytes=read(LOG_FP,buf,size);
        return bytes;

    }
    else
        return 0;
}

static int lfs_write(const char *path, const char *buf, size_t size,
                     off_t offset, struct fuse_file_info *fi)
{
    LOG("Write called \n");
    size_t len;
    (void) fi;
    const char* fname=get_file_name(path);
    std::string filename(fname);

    //Each file if only of size BLOCK_SIZE. Any read that exceeds this size 
    //is an over flow
    if(offset + size > BLOCK_SIZE)
        return -EOVERFLOW;

    //if the filename exists
    if(file_map.count(filename) != 0)
    {
        //get the block num i.e key
        int block_no=file_map.find(filename)->second;
        lseek(LOG_FP,(block_no*BLOCK_SIZE) + offset ,SEEK_SET);
        int bytes=write(LOG_FP,buf,size);
        return bytes;

    }
    else
        return -ENOENT;
}

static void lfs_destroy (void * private_data)
{
    
    LOG("Destroy called ... Closing filesystem \n");
    close(fp);
    close(LOG_FP);
}

static int lfs_truncate (const char* path, off_t size)
{
    LOG("Truncate called \n");
    if(size > BLOCK_SIZE)
        return -EFBIG;
    return 0;
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
};



int main(int argc, char *argv[])
{
    std::ifstream in("lfslog", std::ifstream::ate | std::ifstream::binary);
    LOG_SIZE = in.tellg();

    LOG_FP= open("lfslog",O_RDWR);

    fp=open("msglog.txt",O_CREAT|O_APPEND|O_RDWR);
    init();
    umask(0);
    return fuse_main(argc, argv, &lfs_oper, NULL);
}
