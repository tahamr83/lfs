#include <iostream>
#include <fstream>
#include <map>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "inode.h"
#include <map>
#include <vector>
#include <fstream>
#include <map>
using namespace std;
#define BLOCK_SIZE 1024


void test_()
{
		std::map<std::string,int> file_map;
	int nblocks=10;

	for(int i=0;i<nblocks;++i)
    {
        std::string s=std::to_string(i);

        file_map[s]=i+1;

    }

    cout<<file_map.find("4")->second;
}


void test_read()
{

		int offset = 2245, size =3015;
        int excess ,start_block,end_block,bytes=0,rsize,blk_offset,remaining_size=size;

	 	start_block=offset/BLOCK_SIZE;
        end_block=(offset+size)/BLOCK_SIZE;
        blk_offset=offset%BLOCK_SIZE;
        
        cout<<"startblock: "<<start_block<<endl;
       // lseek(LOG_FP,(indoes[idx].block[start_block]*BLOCK_SIZE) + offset ,SEEK_SET);
        for(int blk=start_block;blk<=end_block;++blk)
        {
        	cout<<"Block offset: "<<blk_offset+blk*BLOCK_SIZE<<endl;
            cout<<"Size to read: "<<(rsize=(min(BLOCK_SIZE,remaining_size)-blk_offset));
            cout<<endl;
            blk_offset=0;//reset block offset for further 
           	cout<<"Remaining size: "<< (remaining_size-=rsize);
           	cout<<endl;

        }

}


void test_inode_save_to_file()
{
		std::map<int,inode> i;
	int sblock=41;
	for( int c=0;c<2;++c)
	{
		inode node(c,sblock++);
		i[c]=node;
	}

	std::map<int,inode> j;
	sblock=41;
	for( int c=0;c<2;++c)
	{
		inode node(c,sblock++);
		j[c]=node;
	}


	
	i[0].file_size=4224;
	i[0].block[1]=2241;
	i[0].block[2]=2345;

	i[1].file_size=99284;
	i[1].block[1]=2241;
	i[1].block[2]=5432;


	int fp=open("inode",O_CREAT|O_RDWR|0666);
	write(fp,&i[0],sizeof(inode));
	write(fp,&i[1],sizeof(inode));
	close(fp);

	//std::vector<inode> j(2);
	

	fp=open("inode",O_CREAT|O_RDWR);
	read(fp,&j[0],sizeof(inode));
	read(fp,&j[1],sizeof(inode));
	close(fp);
	j[0].print();
	j[1].print();
}


void test_bit_vector_block()
{
	
	std::bitset<8192> data_bitmap[2];
	data_bitmap[1][1]=1;


	int fp=open("bitvector",O_RDWR);

	write(fp,&data_bitmap[1],1024);
	close(fp);


	std::bitset<8192> test_map[2];
	fp=open("bitvector",O_RDWR);
	read(fp,&test_map[1],1024);
	close(fp);

	cout<<test_map[1][1]<<std::endl;

}


int get_size(std::map<string,int> &fmap)
{
	int size=0;

	for(auto const &i:fmap)
	{
		size+= (i.first.size() + 6);

	}

	return size;
}

void test_map()
{
	std::map<std::string,int> file_map;
	char fbuff[128];
	file_map["file1.txt"]=0;
	file_map["file2.txt"]=1;
	file_map["MyImportantFile.txt"]=2;
	file_map["MyImportantFile2.txt"]=6;

	int fp=open("testmap",O_CREAT| O_RDWR);
	int nent=file_map.size();


	write(fp,&nent,sizeof(int));	

	for(auto const &i: file_map)
	{
		strcpy(fbuff,i.first.c_str());
		write(fp,fbuff,128);
		write(fp,&i.second,sizeof(int));
	}

	close(fp);
	int n=0;
	int val=0;
	std::map<std::string,int> test_map;

	fp=open("testmap",O_CREAT| O_RDWR);
	read(fp,&n,sizeof(int));
	for(int i=0;i<n;++i)
	{	
		read(fp,&fbuff,128);
		string filename(fbuff);
		read(fp,&val,sizeof(int));
		test_map[filename]=val;
	}

	close(fp);
	for(auto const &i: test_map)
	{
		cout<<i.first<<" "<<i.second<<endl;

	}

}


void test_write()
{
	const char * path="/Users/muhammadtahazaidi/Documents/Study/Operating Systems/AdvancedOs-Lums/AOS-project1/part3/logfs/lfs.h";
	const char* write_string="test write \n";
	ofstream fout(path,std::fstream::app);
	fout<<write_string;
	fout.close();

// 	int fp =open(path,O_RDWR);
// 	//lseek(fp,250,SEEK_SET);
// //	write(fp,write_string,strlen(write_string));
// 	pwrite(fp,write_string,strlen(write_string),250);
// 	close(fp);


}


int main()
{

	//test_map();

	test_write();
    return 0;

}


