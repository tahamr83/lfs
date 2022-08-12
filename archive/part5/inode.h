
#include <iostream>

#ifndef INODE_H
#define INODE_H

#define MAX_BLOCKS 8
#define INODE_SIZE 40
class inode
{
public:
	unsigned int file_size;
	unsigned int block [MAX_BLOCKS];
	unsigned int indirect;
	
	inode()
	{
		file_size=0;
		for(int i=0;i<MAX_BLOCKS;++i)
			block[i]=0;
		indirect=0;

	}

	inode(int fsize,int first_block)
	{
		file_size=fsize;
		block[0]=first_block;
		for(int i=1;i<MAX_BLOCKS;++i)
			block[i]=0;
		indirect=0;
	}

	void print()
	{
		std::cout<<"File Size:		"<<file_size<<std::endl;
		for(int i =0; i<MAX_BLOCKS;++i)
		{
			std::cout<<"block["<<i<<"] :		"<<block[i]<<std::endl;
		}

	}

	int get_block(int block_num,int LOG_FP)
	{
		if(block_num<=7)
			return block[i];
		block_num=block_num-8;
		int blk=0;
		pread(LOG_FP,&blk,sizeof(int),block_num*4,indirect_block*1024);
		return get_block;

	}

};

#endif