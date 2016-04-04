#include <iostream>
void usage()
{
	//            ------------------------------------width:80------------------------------------
	std::cout << "XPT - Cross(X)platform Packaging Tool - Version: 0.0.0"                           << std::endl << std::endl;

	std::cout << "It's a simple tool that allows you to create packages, push them to server then"  << std::endl;
	std::cout << "install packages automatically pulling out dependencies. It's something similar"  << std::endl;
	std::cout << "to Debians apt."                                                                  << std::endl << std::endl;

	std::cout << "If you want to set the working directory the first argument must always be pwd"   << std::endl;
	std::cout << "if ommited the shells working directory is used."                                 << std::endl;
	std::cout << "  xpt pwd C:\\sandbox"                                                            << std::endl;
	std::cout << "  xpt pwd ."                                                                      << std::endl << std::endl;

	std::cout << "To make a package you must create a directory with subdirectories: CONTENT and"   << std::endl;
	std::cout << "PACKAGE. The first one should contain folders and files to pack (relatively to"   << std::endl;
	std::cout << "the sandbox root - all like in Debian). The PACKAGE folder must contain the file" << std::endl;
	std::cout << "\"control\" with the following content (space after colon required!):"            << std::endl;
	std::cout << "  Package: example-package"                                                       << std::endl;
	std::cout << "  Version: 1.2.3-4"                                                               << std::endl;
	std::cout << "  Depends: dependent-package1, dependent-package2"                                << std::endl;
	std::cout << "The package will be stored in pwd."                                               << std::endl;
	std::cout << "  xpt create DIRECTORY"                                                           << std::endl << std::endl;

	std::cout << "The next command pushes packages found in pwd to the repository:"                 << std::endl;
	std::cout << "  xpt repo \\\\server\\repo"                                                      << std::endl;
	std::cout << "  xpt pwd C:\\packages repo \\\\server\\repo"                                     << std::endl;
}
#include "boost/filesystem.hpp"
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <fstream>
#include <array>
#include <vector>
#include <iterator>
// Global vars used everywhere thru the code -----------------------------------
std::string pwd = ".";
std::string exd; // executable directory

// Universal helper functions --------------------------------------------------
bool isdir(const std::string& s)
{
	struct stat info;
	if (stat(s.c_str(), &info) != 0) return false;
	else if( info.st_mode & S_IFDIR ) return true;
	return false;
}

bool isfile(const std::string& s)
{
	struct stat info;
	if (stat(s.c_str(), &info) != 0) return false;
	else if( info.st_mode & S_IFREG ) return true;
	return false;
}
bool copy(const std::string& src, const std::string& dst)
{
	/*
	Supported upload protocols:
	Windows:
		\\smb-server\repo\tag
		repo-folder\tag
	*/
	std::string cmd = "";
#if defined(_WIN32) || defined(WIN32)
	cmd = "xcopy /q "+src+" "+dst;
#endif
	if (std::system(cmd.c_str()) != 0){std::cout << "*** Error! Executing failed: " << cmd; return false;}
	return true;
}

// Main ------------------------------------------------------------------------
bool repo(const std::string& repo)
{
	/*
	Example of the packages-file in a repo:
	----------------------------------------------------------------------------
	package1 1.2.3-4
	package2 5.6.7-8
	----------------------------------------------------------------------------
	*/
	boost::filesystem::path r(repo);
	if(boost::filesystem::exists(r)){std::cout<<"*** Error! Already exists: "<<r.filename().string()<<std::endl;return false;}
	if(!boost::filesystem::create_directories(r)){std::cout<<"*** Error creating: "<<r.filename().string()<<std::endl;return false;}

	std::vector<std::string > packages;
	boost::filesystem::directory_iterator end_iter;
	for (boost::filesystem::directory_iterator dir_itr(boost::filesystem::path(pwd.c_str()));dir_itr != end_iter;++dir_itr)
	{
		std::string filename = dir_itr->path().filename().string();
		if (filename.size()<5) continue;                             // string(".zip").size() == 4
		if(filename.compare(filename.size()-4,4,".zip")!=0)continue; //
		std::string name = std::string(filename).erase(filename.size()-4);
		std::string package = name.substr(0,name.find("_"));
		std::string version = name.substr(name.find("_")+1);
		packages.push_back(package+" "+version);
		std::cout << "Copying " << filename << " ..." << std::endl;
		if(!copy(filename,repo))return false;
	}
	std::ofstream f("packages");
	for (std::vector<std::string>::iterator it = packages.begin() ; it != packages.end(); ++it)
		f << *it << "\n";
	f.close();
	std::cout << "Copying packages-file ..." << std::endl;
	if(!copy("packages",repo)){std::cout<<"*** Error copying packages-file!"<<std::endl;return false;}
	boost::filesystem::remove(boost::filesystem::path("packages"));
	return false;
}

bool create(const std::string& dir)
{
	if(!isdir(dir)){std::cout << "*** Error! Not a directory: " << dir << std::endl;return 1;}
	if(!isdir(pwd+"/"+dir+"/CONTENT")){std::cout << "*** Error! No CONTENT subdirectory" << std::endl;return 1;}
	if(!isdir(pwd+"/"+dir+"/PACKAGE")){std::cout << "*** Error! No PACKAGE subdirectory" << std::endl;return 1;}

	std::ifstream fcontrol(pwd+"/"+dir+"/PACKAGE/control");
	if (!fcontrol.good()){std::cout << "*** Error! Can not open: " << pwd+"/"+dir+"/PACKAGE" << std::endl;}

	std::string package;
	std::string version;
	std::string depends;
	std::string s;
	while (std::getline(fcontrol, s))
	{
		if (!std::strncmp(s.c_str(), "Package: ", strlen("Package: "))) package = s.substr(std::string("Package: ").size());
		if (!std::strncmp(s.c_str(), "Version: ", strlen("Version: "))) version = s.substr(std::string("Version: ").size());
		if (!std::strncmp(s.c_str(), "Depends: ", strlen("Depends: "))) depends = s.substr(std::string("Depends: ").size());
	}
	if (!package.size()) {std::cout << "*** Error! No \"Package: \" line in the control file" << std::endl;return 1;}
	if (!version.size()) {std::cout << "*** Error! No \"Version: \" line in the control file" << std::endl;return 1;}
	if (!depends.size()) {std::cout << "*** Error! No \"Depends: \" line in the control file" << std::endl;return 1;}

	std::ofstream fsize((pwd+"/"+dir+"/PACKAGE/size"));
	std::ofstream fmd5sums((pwd+"/"+dir+"/PACKAGE/md5sums"));
	std::ofstream fmd5sum((pwd+"/"+dir+"/PACKAGE/md5sum"));
	fsize.close();
	fmd5sums.close();
	fmd5sum.close();

	std::string cmd;
#if defined(_WIN32) || defined(WIN32)
	std::string app = (boost::filesystem::path(exd) / boost::filesystem::path("7za.exe")).string();
	cmd = app+" a \""+pwd+"\\"+package+"_"+version+".zip\" \""+pwd+"\\"+dir+"/*\"";
#else
#endif
	std::cout << cmd << std::endl;
	if (std::system(cmd.c_str()) != 0){std::cout << "*** Error! Executing failed: " << cmd; return false;}
	return true;
}

int main(int argc, char* argv[])
{
	exd = boost::filesystem::system_complete(boost::filesystem::path(argv[0])).parent_path().string();
	int shift = 0;
	if (argc > 2 && !strcmp(argv[1],"pwd"))
	{
		if (!isdir(std::string(argv[2]))){std::cout << "*** Error! Not a directory: " << argv[2] << std::endl; return 1;}
		pwd = std::string(argv[2]);
		shift = 2; // parse argv "plus two" as if pwd was not passed
	}

	if (argc == shift+3 && !std::strcmp(argv[shift+1],"create")) return create(std::string(argv[shift+2]));
	if (argc == shift+3 && !std::strcmp(argv[shift+1],"repo")) return repo(std::string(argv[shift+2]));

	std::cout << "*** Error! Unknown command line options! Read the manual below:" << std::endl << std::endl;
	usage();
	return 1;
}
