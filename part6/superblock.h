
class superblock
{
	int inode_blocks[40];
	int last_block_written; //address of the log's last block from where to start appending
}