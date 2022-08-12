#include <iostream>
#ifndef SUPER_BLOCK_H
#define SUPER_BLOCK_H

class SuperBlock
{	
public:
	int inode_blocks[40];
	int bitmap_block;
	// /int next_super;


	SuperBlock()
	{
		for(int i=0;i<40;++i)
		{
			
			inode_blocks[i]=0;
		}
		bitmap_block=0;
		//next_super=0;
	}

	void clear()
	{
		for(int i=0;i<40;++i)
		{
			
			inode_blocks[i]=0;
		}
		bitmap_block=0;
	}
	void print()
	{
		for(int i=0;i<40;++i)
		{
			std::cout<<"Inode Block["<<i<<"]"<<": "<<inode_blocks[i]<<std::endl;
		}
		std::cout<<"Bitmap Block: "<<bitmap_block<<std::endl;
		//std::cout<<"Next Super Block: "<<next_super<<std::endl;
	}
};

#endif