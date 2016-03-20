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
	cout << "  xpt create DIRECTORY"                                                           << endl << endl;

	cout << "The next command pushes packages found in pwd to the repository:"                 << endl;
	cout << "  xpt repo \\\\server\\repo"                                                      << endl;
	cout << "  xpt pwd C:\\packages repo \\\\server\\repo"                                     << endl;
}
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <fstream>
#include <vector>
#include <iterator>
// Global vars used everywhere thru the code -----------------------------------
string pwd = ".";

// Universal helper functions --------------------------------------------------
bool isdir(const string& s)
{
	struct stat info;
	if (stat(s.c_str(), &info) != 0) return false;
	else if( info.st_mode & S_IFDIR ) return true;
	return false;
}

bool isfile(const string& s)
{
	struct stat info;
	if (stat(s.c_str(), &info) != 0) return false;
	else if( info.st_mode & S_IFREG ) return true;
	return false;
}

// Main ------------------------------------------------------------------------
bool create(const string& dir)
{
	if(!isdir(dir)){cout << "*** Error! Not a directory: " << dir << endl;return 1;}
	if(!isdir(pwd+"/"+dir+"/CONTENT")){cout << "*** Error! No CONTENT subdirectory" << endl;return 1;}
	if(!isdir(pwd+"/"+dir+"/PACKAGE")){cout << "*** Error! No PACKAGE subdirectory" << endl;return 1;}

	ifstream fcontrol(pwd+"/"+dir+"/PACKAGE/control");
	if (!fcontrol.good()){cout << "*** Error! Can not open: " << pwd+"/"+dir+"/PACKAGE" << endl;}
	vector<string> ss;string s;while (getline(fcontrol, s)){ss.push_back(s);}

	ofstream fsize((pwd+"/"+dir+"/PACKAGE/size"));
	ofstream fmd5sums((pwd+"/"+dir+"/PACKAGE/md5sums"));
	ofstream fmd5sum((pwd+"/"+dir+"/PACKAGE/md5sum"));
	fsize.close();
	fmd5sums.close();
	fmd5sum.close();
	return false;
}

int main(int argc, char* argv[])
{
	int shift = 0;
	if (argc > 2 && !strcmp(argv[1],"pwd"))
	{
		if (!isdir(string(argv[2]))){cout << "*** Error! Not a directory: " << argv[2] << endl; return 1;}
		pwd = string(argv[2]);
		shift = 2; // parse argv "plus two" as if pwd was not passed
	}

	if (argc == shift+3 && !strcmp(argv[shift+1],"create")) return create(string(argv[shift+2]));

	cout << "*** Error! Unknown command line options! Read the manual below:" << endl << endl;
	usage();
	return 1;
}
