
#include "inode.h"
#include <map>
#include <math.h>
#include <iostream>

#ifndef LFS_H
#define LFS_H

class LogStructuredFileSystem{
public:
	int LOG_FP;
	const unsigned int block_size;
	const unsigned int max_files;
	const unsigned short direct_blocks;
	const unsigned short indirect_blocks;
	unsigned int max_file_size;
	unsigned int total_inodes;
	unsigned int log_size;
	unsigned int total_inode_blocks;
	unsigned int inodes_per_block;
	unsigned int total_blocks;
	unsigned short max_file_name;
	unsigned short inode_start;
	unsigned short inode0;
	unsigned short dirent_per_block;
	unsigned short bitmap_block;
	unsigned short num_blocks_indirect; //Number of blocks indirect block can support;
	

	std::bitset<8192> bitmap;
	std::bitset<8192> dirty_map;
	std::map<std::string,int> file_map;//Filename mapping to inode
	std::map<int,inode> inodes;
	std::ofstream *debug;




	LogStructuredFileSystem(): block_size(1024),max_files(1000),total_blocks(8192),max_file_name(128),direct_blocks(8)
	,indirect_blocks(1)
	{
		total_inodes=max_files;
		inodes_per_block=block_size/sizeof(inode);
		total_inode_blocks=ceil(total_inodes/inodes_per_block);
		inode0=0;
		inode_start=1;
		//First four bytes of each block hold the number of entries currently in this block
		dirent_per_block=(max_file_name+4)/(block_size-4);
		bitmap_block=41;
		num_blocks_indirect=block_size/sizeof(int);

		max_file_size=(direct_blocks*block_size)+(indirect_blocks*num_blocks_indirect*block_size);
	}

	void init()
	{
		std::string s;
		
		//For inode 0
		bitmap[0]=1;

		for(int i=inode_start;i<inode_start+total_inode_blocks;++i)
			bitmap[i]=1;

		
		//************** First block of the directory allocated and written ******************
		// int blk=get_free_blocknum();
		// lseek(LOG_FP,0,SEEK_SET);
		// write(LOG_FP,&blk,sizeof(int));
		// bitmap[blk]=1;


		//*************************** ********************************************************

		for(int i=0;i<total_inodes;++i)
		{
			inode node;
			inodes[i]=node;
		}



	    write_inodes_to_log();
	    write_bitmap_to_log();
	   	write_directory_to_log();
	   	//Initialize the dirty map to our bitmap . All datablocks read are dirty (the most updated bitmap is stored in file)
	   	dirty_map=bitmap;
	    // read_inodes_from_log();
	    // read_bitmap_from_log();
	    // read_directory_from_log();
	}



	unsigned int get_free_blocknum()
	{
		for(int i=0;i<total_blocks;++i)
	    {
	        if(bitmap[i] == 0)
	            return i;
	    }
    	return 0;
	}

	void write_inodes_to_log()
	{
	    int inum=0;
	    lseek(LOG_FP,0,SEEK_SET);//seek to start of the file
	    int bytes;
	    for(int b=inode_start;b<total_inode_blocks+inode_start;++b)
	    {   
	        //seek to the start of each block
	        lseek(LOG_FP,b*block_size,SEEK_SET);
	        for(int c=0;c<inodes_per_block;++c)
	        {
	           bytes+= write(LOG_FP,&inodes[inum],sizeof(inode));
	            inum++;
	        }
	    }

	}

	void read_inodes_from_log()
	{   
	    int inum=0;
	    int inodes_per_block=block_size/sizeof(inode);
	    lseek(LOG_FP,0,SEEK_SET);//seek to start of the file
	    
	    for(int b=inode_start;b<total_inode_blocks+inode_start;++b)
	    {   
	        //seek to the start of each block
	        lseek(LOG_FP,b*block_size,SEEK_SET);
	        for(int c=0;c<inodes_per_block;++c)
	        {
	            read(LOG_FP,&inodes[inum],sizeof(inode));
	            inum++;
	        }
	    }
	    
	}

	void write_bitmap_to_log()
	{
	    //Write the bitmap to file. The bitmap uses only one block so we write 1024 bytes to block 40
	    pwrite(LOG_FP,&bitmap,1024,bitmap_block*block_size);

	}

	void read_bitmap_from_log()
	{
		pread(LOG_FP,&bitmap,1024,bitmap_block*block_size);
	}


	void write_directory_to_log()
	{
		//Seek to block zero
		// lseek(LOG_FP,0,SEEK_SET);
		// char fbuff[128];

		// int nentries= file_map.size(),nblocks=0,bytes=0,blk;
		// pread(LOG_FP,&blk,sizeof(int),0);//First block of directory

		// lseek(LOG_FP,blk*block_size,SEEK_SET);
		// write(LOG_FP,&nentries,sizeof(int));
		// for(auto const &i: file_map)
		// {
		// 	strcpy(fbuff,i.first.c_str());
		// 	write(LOG_FP,fbuff,128);
		// 	write(LOG_FP,&i.second,sizeof(int));
		// }

		inodes[0].file_size=dir_size();
		int start_block=0;
		int end_block=file_size/block_size;

		write


	}

	int dir_size(){

		int size=0;
		for(auto const &i: file_map)
		{
			size+=i.first.size();
			size+=4;
		}
	}

	void read_directory_from_log()
	{
		lseek(LOG_FP,0,SEEK_SET);
		char fbuff[128];

		int nentries= 0,nblocks=0,blk,val=0;
		pread(LOG_FP,&blk,sizeof(int),0);//First block of directory

		lseek(LOG_FP,blk*block_size,SEEK_SET);
		read(LOG_FP,&nentries,sizeof(int));

		for(int i=0;i<nentries;++i)
		{
			read(LOG_FP,&fbuff,128);
			std::string filename(fbuff);
			read(LOG_FP,&val,sizeof(int));
			file_map[filename]=val;
		}

	}


	int read_blocks(int start_block,int end_block,char *buf,int size ,int offset,int fh)
	{
		int blk_offset=offset%block_size ,remaining_size=size,rsize,rbytes,bytes=0;


		for(int blk=start_block;blk<=end_block;++blk)
        {
            rsize=(std::min<int>(block_size,remaining_size)-blk_offset);
            rbytes = read_block(LOG_FP,inodes[fh].block[blk] ,buf+bytes,rsize,blk_offset);
            blk_offset=0;//reset block offset for further 
            remaining_size-=rbytes;
            bytes+=rbytes;

        }

        return bytes;
	}

	int write_blocks(int start_block,int end_block,const char *buf,int size ,int offset,int fh)
	{
		int blk_offset=offset%block_size ,remaining_size=size,rsize,rbytes,bytes=0;


		for(int blk=start_block;blk<=end_block;++blk)
        {
        	
        	//Allocate if the block we are writing is dirty
        	if(is_dirty(inodes[fh].block[blk]))
        	{

	        	int freeblk=allocate_free_block(blk,fh);//Allocate a new free block to block#blk of fh
	        	copy_block(freeblk,inodes[fh].block[blk]);//copy the old block into the new block
	        	*(debug)<<"Dirty:"<<inodes[fh].block[blk]<<"	New:"<<freeblk<<std::endl;

	        	dirty_map[freeblk]=1;//Since we are writing to it make it dirty
	        	inodes[fh].block[blk]=freeblk;//update the inode



        	}
        	/*All dirty blocks from start to end will be newly allocated.
        	Previously allocated new blocks (in the write call of FUSE if we extend the file size) will not be dirty
        	since we set a block dirty only in write_block call and such blocks will not be copied 
			*/

            rsize=(std::min<int>(block_size,remaining_size)-blk_offset);
            rbytes = write_block(LOG_FP,inodes[fh].block[blk] ,buf+bytes,rsize,blk_offset);
            blk_offset=0;//reset block offset for further 
            remaining_size-=rbytes;
            bytes+=rbytes;

        }

        return bytes;
	}


	void copy_block(int dest_block,int src_block)
	{
		char *buf = new char[block_size];

		pread(LOG_FP,buf,block_size,src_block*block_size);
		pwrite(LOG_FP,buf,block_size,dest_block*block_size);
		delete buf;

	}
	//Allocate blocks to file with key fh 
	void allocate_free_blocks(int start_block,int end_block, int fh)
	{

		for(int blk=start_block;blk <=end_block;++blk)
            {
                int freeblk=get_free_blocknum();
                inodes[fh].block[blk]=freeblk;
                bitmap[freeblk]=1;//set it to be used
            }

	}

	int allocate_free_block(int blk_num,int fh)
	{
		int freeblk=get_free_blocknum();
		bitmap[freeblk]=1;
		return freeblk;
	}

	bool is_dirty(int blk)
	{
		if(dirty_map[blk] == 1)
			return true;
		return false;
	}



	bool file_exists(std::string filename)
	{
		if(file_map.count(filename) != 0)
			return true;
		return false;
	}

	int get_inode_index(std::string filename)
	{
		return file_map[filename];
	}

	int get_file_size(std::string fname)
	{
		int idx = file_map[fname];
		return inodes[idx].file_size;
	}

	int read_block(int LOG_FP,int blk_num,char* buf,size_t size, off_t offset)
	{
		return pread(LOG_FP,buf,size,(blk_num*block_size)+offset);
	}

	int write_block(int LOG_FP,int blk_num,const char* buf,size_t size, off_t offset)
	{
		dirty_map[blk_num]=1;
		return pwrite(LOG_FP,buf,size,(blk_num*block_size)+offset);
	}

	void truncate(std::string filename,int size)
	{
		int fh=file_map[filename],blk;
		inode *node=&inodes[fh];//get the inode for this file

		if(size < node->file_size)
		{
			
		}

		//First deallocate 

		blk=size/block_size;

		//Free the blocks
		for(int i=blk+1;i<8;++i)
			bitmap[node->block[i]]=0;

	}


	int get_free_inode_num()
	{
		int s=inodes.size();

		for(int i=0;i<s;++i)
		{
			if(inodes[i].block[0] == 0)
				return i;
		}

		return -1;
	}

	void create(std::string FileName)
	{
		int inum=get_free_inode_num();
		file_map[FileName]= inum;
		//Allocate the first block
		allocate_free_blocks(0,0,inum);

	}

};

#endif

