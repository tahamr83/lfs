
#include <stdio.h>
#ifndef BLOCK_SIZE
#define BLOCK_SIZE 1024
#endif

int read_block(int LOG_FP,int blk_num,char* buf,size_t size, off_t offset)
{
	return pread(LOG_FP,buf,size,(blk_num*BLOCK_SIZE)+offset);
}

int write_block(int LOG_FP,int blk_num,const char* buf,size_t size, off_t offset)
{
	return pwrite(LOG_FP,buf,size,(blk_num*BLOCK_SIZE)+offset);
}