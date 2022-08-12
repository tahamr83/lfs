
#include "inode.h"
#include <map>
#include <math.h>
#include <iostream>
#include "superblock.h"

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
	unsigned short direct_size_limit;
	unsigned short num_blocks_indirect; //Number of blocks indirect block can support;
	int inode_num;

	SuperBlock superblock;

	std::string dirname="directory";

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
		inode_start=0;
		//First four bytes of each block hold the number of entries currently in this block
		dirent_per_block=(max_file_name+4)/(block_size-4);
		
		num_blocks_indirect=block_size/sizeof(int);

		max_file_size=(direct_blocks*block_size)+(indirect_blocks*num_blocks_indirect*block_size);
		inode_num=1;
		direct_size_limit=direct_blocks*block_size;
	}

	void init()
	{

		//Do not remove

		for(int i=0;i<total_inodes;++i)
		{
			inode node;
			inodes[i]=node;
		}

		//create(dirname);

	   	//ORDER IS VERY IMPORTANT
	   	read_superblock();
	   	// superblock.print();

	    read_inodes_from_log();

	    // for(int i=0;i<3;++i)
	    // {
	    // 	inodes[i].print();
	    // }

	    read_directory_from_log();
	    // for(auto const&i : file_map)
	    // {
	    // 	std::cout<<i.first<<std::endl;
	    // }
	    read_bitmap_from_log();
	    //Initialize the dirty map to our bitmap . All datablocks read are dirty (the most updated bitmap is stored in file)
	    dirty_map=bitmap;
	}


	void format()
	{
		int freeblk,iblock;
		//for super_block
		bitmap[0]=1;
		//Initialize all inode blocks
		for(int i=0;i<total_inode_blocks;++i)
		{
			freeblk=get_free_blocknum();
			bitmap[freeblk]=1;			
			superblock.inode_blocks[i]=freeblk;

		}
		freeblk=get_free_blocknum();
		bitmap[freeblk]=1;
		superblock.bitmap_block=freeblk;

		//Initilize the inode datastructure
		for(int i=0;i<total_inodes;++i)
		{
			inode node;
			inodes[i]=node;
		}

		 dirty_map=bitmap;

		 create(dirname);

	    //********* Write Directory to log *********************************
	    //Directory has only one entry and its block is refered by inode 0
	    char fname[128];
		char *buf = new char[132];
		int dirNode=file_map[dirname];
		strcpy(fname,dirname.c_str());
		memcpy(buf,fname,128);
		memcpy(buf+128,&dirNode,4);			
		
		fswrite(dirname,buf,132,0);
		delete buf;
	    //***************************************************************

	  

		//********* Write inodes to log *********************************
		int inum=0,bytes=0;
	    for(int b=0;b<total_inode_blocks;++b)
	    {   
	        //seek to the start of each block
	    	iblock=superblock.inode_blocks[b];
	        lseek(LOG_FP,iblock*block_size,SEEK_SET);
	        for(int c=0;c<inodes_per_block;++c)
	        {
	           bytes+= write(LOG_FP,&inodes[inum],sizeof(inode));
	            inum++;
	        }
	    }
	    //*****************************************************************

	    //********* Write superblock to log *******************************
	    lseek(LOG_FP,0,SEEK_SET);
		write(LOG_FP,&superblock.inode_blocks,sizeof(superblock.inode_blocks));
		write(LOG_FP,&superblock.bitmap_block,sizeof(int));		
		//*****************************************************************

	    //********* Write bitmap to log *********************************
	    pwrite(LOG_FP,&bitmap,1024,superblock.bitmap_block*block_size);
	    //***************************************************************


	}


	unsigned int get_free_blocknum()
	{
		for(int i=0;i<total_blocks;++i)
	    {
	        if(bitmap[i] == 0){
	            bitmap[i]=1;
	            return i;
	        }
	    }
    	return 0;
	}

	void write_inodes_to_log()
	{
	    int inum=0,bytes,freeblk,iblock=0,old_indirect;
	    //lseek(LOG_FP,0,SEEK_SET);//seek to start of the file
	    
	    //While writing the inodes to log we update the super block (which knows where inodes are) to reflect where 
	    //the new inodes will be written. 
	    for(int i=0;i<40;++i)
	    {	
	    	freeblk=get_free_blocknum();
	    	bitmap[freeblk]=1;
	    	superblock.inode_blocks[i]=freeblk;	    	
	    }

	    //For each inode we also have to move its indirect block
	    for(int i=0;i<total_inodes;++i)
	    {
	    	old_indirect=inodes[i].indirect;
	    	if(old_indirect != 0)
	    	{
		    	freeblk=get_free_blocknum();
		    	bitmap[freeblk]=1;
		    	inodes[i].indirect=freeblk;
		    	copy_block(freeblk,old_indirect);
	    	}
	    }

	    //Write the inodes to log in the blocks reffered to by the super blocks.
	    for(int b=0;b<total_inode_blocks;++b)
	    {   
	        //seek to the start of each block
	    	iblock=superblock.inode_blocks[b];
	        lseek(LOG_FP,iblock*block_size,SEEK_SET);
	        for(int c=0;c<inodes_per_block;++c)
	        {
	           bytes+= write(LOG_FP,&inodes[inum],sizeof(inode));
	            inum++;
	        }
	    }

	}

	void read_inodes_from_log()
	{   
	    int inum=0,iblock;
	    int inodes_per_block=block_size/sizeof(inode);
	    

	    
	    for(int b=0;b<total_inode_blocks;++b)
	    {   
	        //seek to the start of each block
	        iblock=superblock.inode_blocks[0];
	        lseek(LOG_FP,iblock*block_size,SEEK_SET);

	        for(int c=0;c<inodes_per_block;++c)
	        {
	            read(LOG_FP,&inodes[inum],sizeof(inode));
	            inum++;
	        }
	    }


	    
	}

	void write_superblock()
	{
		lseek(LOG_FP,0,SEEK_SET);
		write(LOG_FP,&superblock.inode_blocks,sizeof(superblock.inode_blocks));
		write(LOG_FP,&superblock.bitmap_block,sizeof(int));		

	}

	void read_superblock()
	{
		lseek(LOG_FP,0,SEEK_SET);
		read(LOG_FP,&superblock.inode_blocks,sizeof(superblock.inode_blocks));
		read(LOG_FP,&superblock.bitmap_block,sizeof(int));		
	}

	void write_bitmap_to_log()
	{
	    //Write the bitmap to file. The bitmap uses only one block so we write 1024 bytes to block 40
	    int freeblk=get_free_blocknum(),bitmap_block;
	    bitmap[freeblk]=1;
	    bitmap_block=superblock.bitmap_block;
	    pwrite(LOG_FP,&bitmap,1024,bitmap_block*block_size);

	}

	void read_bitmap_from_log()
	{
		int bitmap_block=superblock.bitmap_block;

		pread(LOG_FP,&bitmap,1024,bitmap_block*block_size);
		 for(int i=0;i<1024;++i)
	    {
	    	if(bitmap[i] == 0 ){
	    		*debug<<"last written block "<<i<<std::endl;
	    		return;
	    	}

	    }
	}



	void write_directory_to_log()
	{
		// //Seek to block zero
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

		
		char fname[128];
		int dirsize=file_map.size();
		//*debug<<"directory size: "<<dirsize<<std::endl;
		char *buf = new char[132*dirsize];

		int total_bytes=(file_map.size()*132);
		int offset=0;
		

		for(auto const&i: file_map)
		{
			strcpy(fname,i.first.c_str());

			memcpy(buf+offset,fname,128);
			offset+=128;

			memcpy(buf+offset,&i.second,4);			
			offset+=4;

		}

		fswrite(dirname,buf,total_bytes,0);
		delete buf;

	}


	void read_directory_from_log()
	{

		//*debug<<"READ_DIR\n";
		char fname[128];
		char val[4];
		int ival=0;
		int offset=0;
		int bytes=0;

		int dirent = inodes[0].file_size/132;
		//*debug<<"directory size: "<<dirent<<std::endl;
		//Replacing create call in init()
		file_map[dirname]=0;

		for(int i=0;i<dirent;++i)
		{
			//We are treating directory as a file in the filesystem and use fsread and fswrite.
			//If a file with dirname is not created before this call. We cannot appropriately read the directory
			bytes=fsread(dirname,fname,128,offset);
			//*debug<<"Bytes "<<bytes<<std::endl;
			offset=offset+bytes;
			std::string filename(fname);
			bytes=fsread(dirname,val,sizeof(int),offset);
			offset=offset+bytes;
			//*debug<<"Bytes:"<<bytes<<std::endl;
			memcpy(&ival,val,4);
			file_map[filename]=ival;
			//*debug<<filename<<" "<<ival<<std::endl;
			inode_num++;//Since we are allocating inodes while constructing the directory 
						//we increment this number to indicate next free inode
			//How do we miss inode 1 to be allocate to some specific file ? During init we create directory
			//inode_num gets +1 . Again during this loop inode_num get incremented but this action is done 
			//for the directory again.
			
		}

		// *debug<<"END READDIR\n";
	}


	int read_blocks(int start_block,int end_block,char *buf,int size ,int offset,int fh)
	{
		int blk_offset=offset%block_size ,remaining_size=size,rsize,rbytes,bytes=0,iblock;


		for(int blk=start_block;blk<=end_block;++blk)
        {
            rsize=(std::min<int>(block_size-blk_offset,remaining_size));
           // *debug<<"rsize "<<rsize<<" blk_offset"<<blk_offset<<std::endl;
            iblock=inodes[fh].get_block(blk,LOG_FP);
            rbytes = read_block(LOG_FP,iblock,buf+bytes,rsize,blk_offset);
           // *debug<<"rbytes "<<rbytes<<std::endl;
            blk_offset=0;//reset block offset for further 
            remaining_size-=rbytes;
            bytes+=rbytes;

        }

        return bytes;
	}

	int write_blocks(int start_block,int end_block,const char *buf,int size ,int offset,int fh)
	{
		int blk_offset=offset%block_size ,remaining_size=size,rsize,rbytes,bytes=0,iblock;

		int e=inodes[fh].file_size/1024;
		*debug<<"indirect: "<<inodes[fh].indirect<<std::endl;
		for(int i=0;i<=e;++i)
		{
			*debug<<"Block["<<i<<"]"<<inodes[fh].get_block(i,LOG_FP)<<std::endl;
		}

		for(int blk=start_block;blk<=end_block;++blk)
        {
        	
        	//Allocate if the block we are writing is dirty
        	if(is_dirty(inodes[fh].get_block(blk,LOG_FP)))
        	{
        		iblock=inodes[fh].get_block(blk,LOG_FP);
	        	int freeblk=allocate_free_block(blk,fh);//Allocate a new free block to block#blk of fh
	        	copy_block(freeblk,iblock);//copy the old block into the new block
	        	*(debug)<<"Dirty:"<<iblock<<"	New:"<<freeblk<<std::endl;

	        	dirty_map[freeblk]=1;//Since we are writing to it make it dirty
	        	inodes[fh].set_block(blk,freeblk,LOG_FP);//update the inode
        	}
        	/*All dirty blocks from start to end will be newly allocated.
        	Previously allocated new blocks (in the write call of FUSE if we extend the file size) will not be dirty
        	since we set a block dirty only in write_block call and such blocks will not be copied 
			*/
        	iblock=inodes[fh].get_block(blk,LOG_FP);
            rsize=(std::min<int>(block_size-blk_offset,remaining_size));
            rbytes = write_block(LOG_FP,iblock ,buf+bytes,rsize,blk_offset);
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
      	iblock=inodes[fh].get_block(blk,LOG_FP);
            rsize=(std::min<int>(block_size-blk_offset,remaining_size));
            rbytes = write_block(LOG_FP,iblock ,buf+bytes,rsize,blk_offset);
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

	}      	iblock=inodes[fh].get_block(blk,LOG_FP);
            rsize=(std::min<int>(block_size-blk_offset,remaining_size));
            rbytes = write_block(LOG_FP,iblock ,buf+bytes,rsize,blk_offset);
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

	void copy_block(int dest_block,int src_block)
	{
		char *buf = new char[block_size];

		pread(LOG_FP,buf,block_size,src_block*block_size);
		pwrite(LOG_FP,buf,block_size,dest_block*block_size);
		delete buf;

	}      	iblock=inodes[fh].get_block(blk,LOG_FP);
            rsize=(std::min<int>(block_size-blk_offset,remaining_size));
            rbytes = write_block(LOG_FP,iblock ,buf+bytes,rsize,blk_offset);
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
	void copy_block(int dest_block,int src_block)
	{
		char *buf = new char[block_size];

		pread(LOG_FP,buf,block_size,src_block*block_size);
		pwrite(LOG_FP,buf,block_size,dest_block*block_size);
		delete buf;

	}      	iblock=inodes[fh].get_block(blk,LOG_FP);
            rsize=(std::min<int>(block_size-blk_offset,remaining_size));
            rbytes = write_block(LOG_FP,iblock ,buf+bytes,rsize,blk_offset);
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



	void copy_block(int dest_block,int src_block)
	{
		char *buf = new char[block_size];

		pread(LOG_FP,buf,block_size,src_block*block_size);
		pwrite(LOG_FP,buf,block_size,dest_block*block_size);
		delete buf;

	}


	void copy_block(int dest_block,int src_block)
	{
		char *buf = new char[block_size];

		pread(LOG_FP,buf,block_size,src_block*block_size);
		pwrite(LOG_FP,buf,block_size,dest_block*block_size);
		delete buf;

	}
	

	void copy_block(int dest_block,int src_block)
	{
		char *buf = new char[block_size];

		pread(LOG_FP,buf,block_size,src_block*block_size);
		pwrite(LOG_FP,buf,block_size,dest_block*block_size);
		delete buf;

	}


	void copy_block(int dest_block,int src_block)
	{
		char *buf = new char[block_size];

		pread(LOG_FP,buf,block_size,src_block*block_size);
		pwrite(LOG_FP,buf,block_size,dest_block*block_size);
		delete buf;

	}
	

	void copy_block(int dest_block,int src_block)
	{
		char *buf = new char[block_size];

		pread(LOG_FP,buf,block_size,src_block*block_size);
		pwrite(LOG_FP,buf,block_size,dest_block*block_size);
		delete buf;

	}


	void copy_block(int dest_block,int src_block)
	{
		char *buf = new char[block_size];

		pread(LOG_FP,buf,block_size,src_block*block_size);
		pwrite(LOG_FP,buf,block_size,dest_block*block_size);
		delete buf;

	}
	

	void copy_block(int dest_block,int src_block)
	{
		char *buf = new char[block_size];

		pread(LOG_FP,buf,block_size,src_block*block_size);
		pwrite(LOG_FP,buf,block_size,dest_block*block_size);
		delete buf;

	}
	
	void copy_block(int dest_block,int src_block)
	{
		char *buf = new char[block_size];

		pread(LOG_FP,buf,block_size,src_block*block_size);
		pwrite(LOG_FP,buf,block_size,dest_block*block_size);
		delete buf;

	}
	

	void copy_block(int dest_block,int src_block)
	{
		char *buf = new char[block_size];

		pread(LOG_FP,buf,block_size,src_block*block_size);
		pwrite(LOG_FP,buf,block_size,dest_block*block_size);
		delete buf;

	}