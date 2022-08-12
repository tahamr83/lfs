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
#include <errno.h>
#include <fstream>
#include <string.h>
#include <map>
#include <iostream>
#include "inode.h"
#include "testcases.h"
#include "myfunctions.h"
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

int get_free_blocknum()
{
    //Traverse the bitmap to find a free block. 0 denotes a free block
    for(int i=0;i<TOTAL_BLOCKS;++i)
    {
        if(bitmap[i] == 0)
            return i;
    }
    return 0;
}

void write_inodes_to_log()
{
    int inum=0;
    int inodes_per_block=BLOCK_SIZE/sizeof(inode);
    lseek(LOG_FP,0,SEEK_SET);//seek to start of the file
    for(int b=0;b<TOTAL_INODE_BLOCKS;++b)
    {   

        //seek to the start of each block
        lseek(LOG_FP,b*BLOCK_SIZE,SEEK_SET);
        for(int c=0;c<inodes_per_block;++c)
        {
            write(LOG_FP,&inodes[inum],sizeof(inode));
            inum++;
        }
    }

}

void write_bitmap_to_log()
{
        //Write the bitmap to file. The bitmap uses only one block so we write 1024 bytes to block 40
    pwrite(LOG_FP,&bitmap,1024,40*BLOCK_SIZE);

}

void read_bitmap_from_log()
{
    pread(LOG_FP,&bitmap,1024,40*BLOCK_SIZE);
}

void read_inodes_from_log()
{   
    int inum=0;
    int inodes_per_block=BLOCK_SIZE/sizeof(inode);
    lseek(LOG_FP,0,SEEK_SET);//seek to start of the file
    for(int b=0;b<TOTAL_INODE_BLOCKS;++b)
    {   

        //seek to the start of each block
        lseek(LOG_FP,b*BLOCK_SIZE,SEEK_SET);
        for(int c=0;c<inodes_per_block;++c)
        {
            read(LOG_FP,&inodes[inum],sizeof(inode));
            inum++;
        }
    }
}

void init()
{
    std::string s;
    
    
    /*The starting block number for file 0. Each file will have its first block allocated.
    *Since first 40 blocks are occupied by inode and 41th (40 index) is for data bitmap
    The first block allocated to data will be the 42nd block (41 index)*/
    int sblock=41;
    
    for(int i=0;i<MAX_FILES;++i)
    {
        s=std::to_string(i);
        s=s+".txt";

        file_map[s]=i;

    }

    for(int i=0;i<41;++i) //First 41 blocks are occupied
        bitmap[i]=1;
    //Initialize our datastructure for inodes
    for(int i=0;i<MAX_FILES;++i)
    {   
        inode node(0,sblock);
        bitmap[sblock++]=1;
        inodes[i]=node;
    }

    //write these indoes to the first 40 block of the file
    write_inodes_to_log();
    write_bitmap_to_log();

    
    test_inodes(debug,LOG_FP);
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
        for(auto const& i: file_map)
        {   
            fname=get_file_name(path);//get filename from the path
            std::string filename(fname);
            if(file_map.count(filename) != 0)//Not such key exists
            {   
                stbuf->st_mode = S_IFREG | 0666;
                stbuf->st_nlink = 1;
                //Get key from filename to index into inode map.e.g file "1.txt" has inode 1
                value=file_map[filename];
                stbuf->st_size = inodes[value].file_size;
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
   
    //if the file exists
    if(file_map.count(filename) != 0)
    {   
        int idx=file_map[filename];
        int file_size=inodes[idx].file_size;
        int excess ,start_block,end_block,bytes=0,rsize,blk_offset,remaining_size=size;

        if(offset >= MAX_FILE_SIZE || offset>=file_size)
        return 0;
        //If the requested file size and offset overflow beyond file size we reset the size(bytes to read)
        //such that only contents of the file are read.
        if(size+offset > file_size)
        {
            excess=size+offset-file_size;
            size=size-excess;
        }        

        start_block=offset/BLOCK_SIZE;
        end_block=(offset+size)/BLOCK_SIZE;
        blk_offset=offset%BLOCK_SIZE;
        

       // lseek(LOG_FP,(indoes[idx].block[start_block]*BLOCK_SIZE) + offset ,SEEK_SET);
        for(int blk=start_block;blk<=end_block;++blk)
        {
            rsize=(std::min(BLOCK_SIZE,remaining_size)-blk_offset);
            int rbytes = read_block(LOG_FP,inodes[idx].block[blk] ,buf+bytes,rsize,blk_offset);
            blk_offset=0;//reset block offset for further 
            remaining_size-=rbytes;
            bytes+=rbytes;

        }

        return bytes;

    }
    else
        return -ENOENT;
}

static int lfs_write(const char *path, const char *buf, size_t size,
                     off_t offset, struct fuse_file_info *fi)
{
     size_t len;
    (void) fi;
    const char* fname=get_file_name(path);
    std::string filename(fname);
    debug<<"Write "<<size<<std::endl;
    //if the file exists
    if(file_map.count(filename) != 0)
    {   
        int idx=file_map[filename];
        int file_size=inodes[idx].file_size;
        int excess ,start_block,end_block,bytes=0,rsize,blk_offset,remaining_size=size;

        if(offset >= MAX_FILE_SIZE )
            return 0;
        //If the requested file size and offset overflow beyond the max file size we reset the size(bytes to read)
        //such that file is written only up to MAX_FILE_SIZE 
        if(size+ offset > MAX_FILE_SIZE)
        {
            excess=size+offset-MAX_FILE_SIZE;
            size=size-excess;
        }
        //If the size+offset is greater than file size we might have to allocate blocks to the file by updating the 
        //inodes 
        if(size+offset > file_size)
        {
            start_block=file_size/BLOCK_SIZE;//This is the block where the last byte resides. Already allocated 
            end_block=(size+offset)/BLOCK_SIZE;//The block where the last byte will be written
            for(int blk=start_block+1;blk <=end_block;++blk)
            {
                int freeblk=get_free_blocknum();
                bitmap[freeblk]=1;//set it to be used
                //except for first block allocate blocks                
                inodes[idx].block[blk]=freeblk;
            }
            //If size+offset is greater than the current file size wo increase it to offset+size
            //other wise there is no need to increase or change file size since we are writing within bounds
            inodes[idx].file_size=offset+size;
        }        

        ////Recalculate starting block according to offset and offset+size
        start_block=offset/BLOCK_SIZE;
        end_block=(offset+size)/BLOCK_SIZE;
        blk_offset=offset%BLOCK_SIZE;
        
        for(int blk=start_block;blk<=end_block;++blk)
        {
            rsize=(std::min(BLOCK_SIZE,remaining_size)-blk_offset);
            debug<<"block being written " <<inodes[idx].block[blk]<<" of size "<<rsize<<std::endl;
            int rbytes = write_block(LOG_FP,inodes[idx].block[blk] ,buf+bytes,rsize,blk_offset);
            blk_offset=0;//reset block offset for further 
            remaining_size-=rbytes;
            bytes+=rbytes;

        }
        debug<<"End \n";
        return bytes;

    }
    else
        return -ENOENT;
}

static void lfs_destroy (void * private_data)
{   
    
    debug<<"Destroy called ... Closing filesystem \n";
    close(fp);
    debug.close();
}

static int lfs_truncate (const char* path, off_t size)
{
    debug<<"Truncate: "<<size<<std::endl;
    if(size > MAX_FILE_SIZE)
        return -EFBIG;

    const char* fname=get_file_name(path);
    std::string filename(fname);
    if(file_map.count(filename) != 0)
    {
        int idx=file_map[filename];
        inodes[idx].file_size=size;
        
        return 0;
    }
    else
        return -ENOENT;

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

    debug.open("debug.txt",std::fstream::trunc);
    LOG_FP= open("lfslog",O_RDWR);

    init();
    umask(0);
    return fuse_main(argc, argv, &lfs_oper, NULL);
}
