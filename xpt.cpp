#include <iostream>
using namespace std;
void usage()
{
	//       ------------------------------------width:80------------------------------------
	cout << "XPT - Cross(X)platform Packaging Tool - Version: 0.0.0"                           << endl << endl;

	cout << "It's a simple tool that allows you to create packages, push them to server then"  << endl;
	cout << "install packages automatically pulling out dependencies. It's something similar"  << endl;
	cout << "to Debians apt."                                                                  << endl << endl;

	cout << "If you want to set the working directory the first argument must always be pwd"   << endl;
	cout << "if ommited the shells working directory is used."                                 << endl;
	cout << "  xpt pwd C:\\sandbox"                                                            << endl;
	cout << "  xpt pwd ."                                                                      << endl << endl;

	cout << "To make a package you must create a directory with subdirectories: CONTENT and"   << endl;
	cout << "PACKAGE. The first one should contain folders and files to pack (relatively to"   << endl;
	cout << "the sandbox root - all like in Debian). The PACKAGE folder must contain the file" << endl;
	cout << "\"control\" with the following content:"                                          << endl;
	cout << "  Package: example-package"                                                       << endl;
	cout << "  Version: 1.2.3-4"                                                               << endl;
	cout << "  Depends: dependent-package1, dependent-package2"                                << endl;
	cout << "The package will be stored in pwd."                                               << endl;
	cout << "  xpt create PACKAGE_TREE"                                                        << endl << endl;

	cout << "The next command pushes packages found in pwd to the repository:"                 << endl;
	cout << "  xpt repo \\\\server\\repo"                                                      << endl;
	cout << "  xpt pwd C:\\packages repo \\\\server\\repo"                                     << endl;
}
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
// Global wars used everywhere thru the code
string pwd = "";

// Helper functions
bool isdir(char *s)
{
	struct stat info;
	if (stat(s, &info) != 0) return false;
	else if( info.st_mode & S_IFDIR ) return true;
	return false;
}

int main(int argc, char* argv[])
{
	if (argc < 2) { usage(); return 1; }
	if (!strcmp(argv[1],"pwd"))
	{
		if (argc < 3) { usage(); return 1; }
		if (!isdir(argv[2])){cout << "*** Error! Not a directory: " << argv[2] << endl; return 1;}
		pwd = string(argv[2]);
	}
	else
		pwd = ".";
	return 0;
}
