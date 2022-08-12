#include <iostream>
#include <fstream>
#include <map>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
using namespace std;

int main()
{
	// std::map<std::string,int> file_map;
	// int nblocks=10;

	// for(int i=0;i<nblocks;++i)
 //    {
 //        std::string s=std::to_string(i);

 //        file_map[s]=i+1;

 //    }

 //    cout<<file_map.find("4")->second;

	int fp=open("/Users/muhammadtahazaidi/Documents/Study/Operating Systems/AdvancedOs-Lums/AOS-project1/part1/logfs/3.txt",O_RDWR);
	write(fp,"Hello Taha !",13);
	close(fp);
    return 0;
}