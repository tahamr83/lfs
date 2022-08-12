

void test_inodes (std::ofstream &debug,int& LOG_FP)
{
	inode tinode;
	int block_num=39;

	pread(LOG_FP,&tinode,sizeof(inode),block_num*1024+(40*24));

	debug<<"Node size:"<<tinode.file_size<<std::endl;
	debug<<"Block1:"<<tinode.block[0]<<std::endl;


}