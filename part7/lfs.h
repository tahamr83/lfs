
#include "inode.h"
#include <map>
#include <math.h>
#include <iostream>
#include "superblock.h"
#include <vector>

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
	int check_inode_num;



	SuperBlock superblock;
	SuperBlock check_superblock;

	unsigned int old_superblock;
	std::string dirname="directory";

	std::bitset<8192> bitmap;
	std::bitset<8192> dirty_map;

	std::map<std::string,int> root_map;//Filename mapping to inode
	std::map<std::string,int> checkpoint_map;
	std::map<std::string,int> check_map;//contains absolute paths for checkpoints and their superblocks

	std::map<int,inode> inodes;
	std::map<int,inode> check_inodes;
	std::ofstream *debug;

	std::string check_path="/checkpoints";
	std::string checkname="checkpoints";






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
	    // for(auto const&i : root_map)
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
		int dirNode=root_map[dirname];
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
	    int tmp=0;
	    lseek(LOG_FP,0,SEEK_SET);
		write(LOG_FP,&superblock.inode_blocks,sizeof(superblock.inode_blocks));
		write(LOG_FP,&superblock.bitmap_block,sizeof(int));		
		write(LOG_FP,&tmp,sizeof(int));	//clear value to be safe
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
	    int count=0;
	    for(int i=0;i<total_inodes;++i)
	    {
	    	old_indirect=inodes[i].indirect;
	    	if(old_indirect != 0)
	    	{
		    	freeblk=get_free_blocknum();
		    	bitmap[freeblk]=1;
		    	inodes[i].indirect=freeblk;
		    	copy_block(freeblk,old_indirect);
		    	count++;
	    	}

	    }


	    //Write the inodes to log in the blocks reffered to by the super blocks.
	    for(int b=0;b<total_inode_blocks;++b)
	    {   
	        //seek to the start of each block
	    	iblock=superblock.inode_blocks[b];
	    	clear_block(iblock);
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
	        iblock=superblock.inode_blocks[b];
	        lseek(LOG_FP,iblock*block_size,SEEK_SET);

	        for(int c=0;c<inodes_per_block;++c)
	        {
	            read(LOG_FP,&inodes[inum],sizeof(inode));
	            inum++;
	        }
	    }

	    // inum=0;
	   	// for(int b=0;b<total_inode_blocks;++b)
	    // {   
	    //     for(int c=0;c<inodes_per_block;++c)
	    //     {
	    //     	std::cout<<"Indirect ->"<<inodes[inum].indirect<<std::endl;
	    //     	inum++;
	    //     }
	    //     std::cout<<"------------------\n";
	        
	    // }
	    
	}


	void write_superblock()
	{
		int new_super=get_free_blocknum();
		int temp=0;
		clear_block(new_super);
		lseek(LOG_FP,new_super,SEEK_SET);
		write(LOG_FP,&superblock.inode_blocks,sizeof(superblock.inode_blocks));
		write(LOG_FP,&superblock.bitmap_block,sizeof(int));	
		write(LOG_FP,&temp,sizeof(int));	//zero out the value

		pwrite(LOG_FP,&new_super,sizeof(int),old_superblock*block_size+164);
			//Write the blockenumber of the current image of superblock
			//To the previous super_block


	}

	void read_superblock()
	{	
		int prev,curr=0;
		int counter=0;

		lseek(LOG_FP,0,SEEK_SET);
		do{
			check_map[check_path+"/"+std::to_string(curr)]=curr;
			prev=curr;
			pread(LOG_FP,&curr,sizeof(int),curr*block_size+164);
			*debug<<"Super: "<<curr<<std::endl;
			
			
		}while(curr !=0);
		
		//std::cout<<"CHECKMAP\n";
		
		for(auto const&i:check_map)
		{
			std::cout<<i.first<<std::endl;
		}
		*debug<<"superblock:"<<prev<<std::endl;
		//read superblock from this position
		//prev=470;
		lseek(LOG_FP,prev,SEEK_SET);
		read(LOG_FP,&superblock.inode_blocks,sizeof(superblock.inode_blocks));
		read(LOG_FP,&superblock.bitmap_block,sizeof(int));		
		old_superblock=prev;
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

		// int nentries= root_map.size(),nblocks=0,bytes=0,blk;
		// pread(LOG_FP,&blk,sizeof(int),0);//First block of directory

		// lseek(LOG_FP,blk*block_size,SEEK_SET);
		// write(LOG_FP,&nentries,sizeof(int));
		// for(auto const &i: root_map)
		// {
		// 	strcpy(fbuff,i.first.c_str());
		// 	write(LOG_FP,fbuff,128);
		// 	write(LOG_FP,&i.second,sizeof(int));
		// }

		
		char fname[128];
		int dirsize=root_map.size();
		//*debug<<"directory size: "<<dirsize<<std::endl;
		char *buf = new char[132*dirsize];

		int total_bytes=(root_map.size()*132);
		int offset=0;
		

		for(auto const&i: root_map)
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
		root_map[dirname]=0;

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
			root_map[filename]=ival;
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

		// int e=inodes[fh].file_size/1024;
		// *debug<<"indirect: "<<inodes[fh].indirect<<std::endl;
		// for(int i=0;i<=e;++i)
		// {
		// 	*debug<<"Block["<<i<<"]"<<inodes[fh].get_block(i,LOG_FP)<<std::endl;
		// }

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
	//Allocate blocks to file with key fh 
	void allocate_free_blocks(int start_block,int end_block, int fh)
	{

		for(int blk=start_block;blk <=end_block;++blk)
            {
                int freeblk=get_free_blocknum();
                // inodes[fh].block[blk]=freeblk;
                // bitmap[freeblk]=1;//set it to be used

                inodes[fh].set_block(blk,freeblk,LOG_FP);
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
		if(root_map.count(filename) != 0)
			return true;
		return false;
	}

	int get_inode_index(std::string filename)
	{
		return root_map[filename];
	}

	int get_file_size(std::string fname)
	{
		int idx = root_map[fname];
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
		int fh=root_map[filename],blk;
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
		//int s=inodes.size();

		// for(int i=0;i<s;++i)
		// {
		// 	if(inodes[i].block[0] == 0)
		// 		return i;
		// }
		return inode_num++;
		
	}

	void create(std::string FileName)
	{
		*debug<<"Create\n";
		if(root_map.count(FileName) == 0)
		{
			int inum=get_free_inode_num();
			root_map[FileName]= inum;
			//Allocate the first block
			allocate_free_blocks(0,0,inum);
			*debug<<"Success Create\n";
		}
	}



	int fswrite(std::string filename,const char *buf, size_t size,off_t offset)
	{
		//if the file exists
	    *debug<<"Write "<<filename<<" "<<size<<" offset:"<<offset<<std::endl;
	    if(file_exists(filename))
	    {   
	        int idx=root_map[filename];
	        int file_size=get_file_size(filename);
	        int excess ,start_block,end_block,bytes=0,rsize,blk_offset,remaining_size=size;
	        inode* node = &inodes[idx];
	        
	        if(offset >= max_file_size )
	            return 0;
	        //If the requested file size and offset overflow beyond the max file size we reset the size(bytes to read)
	        //such that file is written only up to MAX_FILE_SIZE 
	        if(size+ offset > max_file_size)
	        {
	            excess=size+offset-max_file_size;
	            size=size-excess;
	        }
	        //If the size+offset is greater than file size we might have to allocate blocks to the file by updating the 
	        //inodes 
	        
	       
	        if(size+offset > file_size)
	        {

	        	// If the size+offset is greater than direct blocks can handle we need to allocate an indirect block
	        	//and further allocate blocks refered by this indirect block. Here we know that write is exceeding
	        	//current file size and also direct blocks can't handle
	        	if(size + offset > direct_size_limit)
	        	{
	        		if(node->indirect == 0)
	        		{
		        		int freeblk;
		        		freeblk=get_free_blocknum();
		        		node->indirect=freeblk;
		        		bitmap[freeblk]=1;

	        		}

	        		end_block=(size+offset)/block_size;
	        		start_block=file_size/block_size;//This is the block where the last byte resides. Already allocated 
		           
		            allocate_free_blocks(start_block+1,end_block,idx);
		            node->file_size=offset+size;
		            //*debug<<"start block: "<<start_block<<"  end block: "<<end_block<<std::endl;

	        	}

	        	else
	        	{
		            start_block=file_size/block_size;//This is the block where the last byte resides. Already allocated 
		            end_block=(size+offset)/block_size;//The block where the last byte will be written
		            allocate_free_blocks(start_block+1,end_block,idx);
		            //If size+offset is greater than the current file size we increase it to offset+size
		            //other wise there is no need to increase or change file size since we are writing within bounds
		            inodes[idx].file_size=offset+size;
	        	}
	        }        

	        ////Recalculate starting block according to offset and offset+size
	        start_block=offset/block_size;
	        end_block=(offset+size)/block_size;
	        
	        bytes=write_blocks(start_block,end_block,buf,size,offset,idx);
	        // write_inodes_to_log();
	        // write_bitmap_to_log();
	      //  write_directory_to_log();

	       // debug<<"Write "<<fname<<" \n";
	        

	        return bytes;

	    }
	    else
	        return -ENOENT;	
	}

	void clear_block(int blk)
	{
		char * buf = new char[block_size];
		memset(buf,0,block_size);
		pwrite(LOG_FP,buf,block_size,blk*block_size);
		delete buf;
	}

	int fsread(std::string filename, char *buf, size_t size, off_t offset)
	{	



		if(file_exists(filename))
	    {   

	    	// *debug<<"\n\nREAD "<<filename<<" Size"<<size<<"offset "<<offset<<std::endl;
	        int idx=root_map[filename];
	        int file_size=get_file_size(filename);
	        int excess ,start_block,end_block,bytes=0,rsize,blk_offset;

	        if(offset >= max_file_size || offset>=file_size)
	            return 0;
	        //If the requested file size and offset overflow beyond file size we reset the size(bytes to read)
	        //such that only contents of the file are read.
	        if(size+offset > file_size)
	        {
	            excess=size+offset-file_size;
	            size=size-excess;
	        }        

	        start_block=offset/block_size;
	        end_block=(offset+size)/block_size;
	    	// *debug<<"Startblock "<<start_block<<" endblock "<<end_block<<"inode "<<idx
	    	// 	<<" Filesize "<<inodes[idx].file_size<<std::endl;

	        bytes= read_blocks(start_block,end_block,buf,size,offset,idx);
	        
	        return bytes;


	    }
	    else
	        return -ENOENT;
	}


	int read_check_blocks(int start_block,int end_block,char *buf,int size ,int offset,int fh)
	{
		int blk_offset=offset%block_size ,remaining_size=size,rsize,rbytes,bytes=0,iblock;


		for(int blk=start_block;blk<=end_block;++blk)
        {
            rsize=(std::min<int>(block_size-blk_offset,remaining_size));
           // *debug<<"rsize "<<rsize<<" blk_offset"<<blk_offset<<std::endl;
            iblock=check_inodes[fh].get_block(blk,LOG_FP);
            rbytes = read_block(LOG_FP,iblock,buf+bytes,rsize,blk_offset);
           // *debug<<"rbytes "<<rbytes<<std::endl;
            blk_offset=0;//reset block offset for further 
            remaining_size-=rbytes;
            bytes+=rbytes;

        }

        return bytes;
	}


	int check_fsread(std::string filename, char *buf, size_t size, off_t offset)
	{	



		if(file_exists(filename))
	    {   

	    	// *debug<<"\n\nREAD "<<filename<<" Size"<<size<<"offset "<<offset<<std::endl;
	        int idx=checkpoint_map[filename];
	        int file_size=check_inodes[idx].file_size;

	        int excess ,start_block,end_block,bytes=0,rsize,blk_offset;

	        if(offset >= max_file_size || offset>=file_size)
	            return 0;
	        //If the requested file size and offset overflow beyond file size we reset the size(bytes to read)
	        //such that only contents of the file are read.
	        if(size+offset > file_size)
	        {
	            excess=size+offset-file_size;
	            size=size-excess;
	        }        

	        start_block=offset/block_size;
	        end_block=(offset+size)/block_size;
	    	// *debug<<"Startblock "<<start_block<<" endblock "<<end_block<<"inode "<<idx
	    	// 	<<" Filesize "<<inodes[idx].file_size<<std::endl;

	        bytes= read_check_blocks(start_block,end_block,buf,size,offset,idx);
	        
	        return bytes;


	    }
	    else
	        return -ENOENT;
	}

	void read_superblock(int blk_num)
	{	
		int prev,curr=0;
		int counter=0;

		lseek(LOG_FP,blk_num*block_size,SEEK_SET);

		read(LOG_FP,&check_superblock.inode_blocks,sizeof(check_superblock.inode_blocks));
		read(LOG_FP,&check_superblock.bitmap_block,sizeof(int));	

		
	}

	void read_checkpoint_inodes()
	{   
	    int inum=0,iblock;
	    int inodes_per_block=block_size/sizeof(inode);
	    
	    for(int b=0;b<total_inode_blocks;++b)
	    {   
	        //seek to the start of each block
	        iblock=check_superblock.inode_blocks[b];
	        lseek(LOG_FP,iblock*block_size,SEEK_SET);

	        for(int c=0;c<inodes_per_block;++c)
	        {
	            read(LOG_FP,&check_inodes[inum],sizeof(inode));
	            inum++;
	        }
	    }

	}

	void read_checkpoint_directory()
	{

		//*debug<<"READ_DIR\n";
		char fname[128];
		char val[4];
		int ival=0;
		int offset=0;
		int bytes=0;

		int dirent = check_inodes[0].file_size/132;
		//*debug<<"directory size: "<<dirent<<std::endl;
		//Replacing create call in init()
		checkpoint_map[dirname]=0;

		for(int i=0;i<dirent;++i)
		{
			//We are treating directory as a file in the filesystem and use fsread and fswrite.
			//If a file with dirname is not created before this call. We cannot appropriately read the directory
			bytes=check_fsread(dirname,fname,128,offset);
			//*debug<<"Bytes "<<bytes<<std::endl;
			offset=offset+bytes;
			std::string filename(fname);
			bytes=check_fsread(dirname,val,sizeof(int),offset);
			offset=offset+bytes;
			//*debug<<"Bytes:"<<bytes<<std::endl;
			memcpy(&ival,val,4);
			check_map[filename]=ival;
			//*debug<<filename<<" "<<ival<<std::endl;
			check_inode_num++;//Since we are allocating inodes while constructing the directory 
						//we increment this number to indicate next free inode
			//How do we miss inode 1 to be allocate to some specific file ? During init we create directory
			//inode_num gets +1 . Again during this loop inode_num get incremented but this action is done 
			//for the directory again.
			
		}

		// *debug<<"END READDIR\n";
	}



	void read_checkpoint(int blk_num)
	{
		check_inodes.clear();
		checkpoint_map.clear();
		check_superblock.clear();

		for(int i=0;i<total_inodes;++i)
		{
			inode node;
			check_inodes[i]=node;
		}

		check_inode_num=0;
		read_superblock(blk_num);
		read_checkpoint_inodes();
		read_checkpoint_directory();

	}

};

#endif

