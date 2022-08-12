#include <iostream>

using namespace std;

int main()

{
	int x=10;
	const char *name="sadad\n";
	cout<<"x: "<<x<<endl;

	cout<<name;
	(void) name;
	cout<<name;
}