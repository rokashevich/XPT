#include <iostream>
void usage(){
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

#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <fstream>
#include <array>
#include <vector>
#include <iterator>
#include <unordered_set>
#include <unordered_map>

#include <boost/filesystem.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/predicate.hpp>

// GLOBAL VARS USED EVERYWHERE THRU THE CODE -----------------------------------
std::string pwd;
std::string exd; // executable directory
boost::filesystem::path varxptpath;
boost::filesystem::path varxptpackagespath;
std::unordered_set<std::string> newfiles; // store here every newly created filepath


// UNIVERSAL HELPER FUNCTIONS --------------------------------------------------
bool isdir(const std::string& s){
	struct stat info;
	if (stat(s.c_str(), &info) != 0) return false;
	else if( info.st_mode & S_IFDIR ) return true;
	return false;
}

bool isfile(const std::string& s){
	struct stat info;
	if (stat(s.c_str(), &info) != 0) return false;
	else if( info.st_mode & S_IFREG ) return true;
	return false;
}

bool e(const std::string &cmd){
	std::cout << ">" << cmd << std::endl;
	return std::system(cmd.c_str()) == 0;
}

bool copy(const std::string& src, const std::string& dst){
/*
Supported upload protocols:
Windows:
	\\smb-server\repo\tag
	repo-folder\tag
*/
	std::string cmd = "";
#if defined(_WIN32) || defined(WIN32)
	cmd = "copy \""+src+"\" \""+dst+"\" 1>nul";
#endif
	return e(cmd);
}

// CURL DOWNLOAD FILE PART -----------------------------------------------------
#define CURL_STATICLIB // required when using static libcurl
#include <curl/curl.h>
static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream){return fwrite(ptr, size, nmemb, (FILE *)stream);}
bool download(const std::string& src, const std::string& dst){
	std::cout<<"Downloading: "<<src<<std::endl;
	std::cout<<"         To: "<<dst<<std::endl;
	curl_global_init(CURL_GLOBAL_ALL);
	CURL *curl_handle;
	curl_handle = curl_easy_init();
	curl_easy_setopt(curl_handle, CURLOPT_URL, src.c_str());
	curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, false);
	curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, true);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data);
	curl_easy_setopt(curl_handle, CURLOPT_FAILONERROR, true);
	FILE *pagefile;
	pagefile = fopen(dst.c_str(), "wb");
	if(pagefile){
		curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, pagefile);
		CURLcode code = curl_easy_perform(curl_handle);
		fclose(pagefile);
		if(code){std::cout<<"*** Error curl_easy_perform: "<<code<<std::endl;return false;}
	}else{
		std::cout<<"*** Error fopen: "<<dst<<std::endl;
		return false;
	}
	curl_easy_cleanup(curl_handle);
	return true;
}
// MAIN ------------------------------------------------------------------------
int revert(){
	for (const auto& elem: newfiles) {
		if (boost::filesystem::exists(elem))
			try{boost::filesystem::remove(elem);}
			catch(std::exception const& e){std::cout<<"*** Error: "<<e.what()<<std::endl;}
	}
	return 1; // must always return false, revert - is a mulfunction
}
int removeold(){
	std::cout<<"Cleaning up ..."<<std::endl;
	boost::filesystem::directory_iterator end_iter;
	for (boost::filesystem::directory_iterator dir_itr(varxptpackagespath);dir_itr != end_iter;++dir_itr){
		if (boost::algorithm::ends_with((*dir_itr).path().string(), ".old")){
			try{boost::filesystem::remove(*dir_itr);}
			catch(std::exception const& e){std::cout<<"*** Error: "<<e.what()<<std::endl;return 1;}
		}
	}
	return 0;
}
int install(const std::vector<std::string>& arg){
	// DOWNLOAD ALL PACKAGES FILES FROM ALL REPOS --------------------------------
	boost::filesystem::directory_iterator end_iter;
	//
	// var/xpt/packages/* -> var/xpt/packages/*.old
	//
	for (boost::filesystem::directory_iterator dir_itr(varxptpackagespath);dir_itr != end_iter;++dir_itr){
		if (boost::algorithm::ends_with((*dir_itr).path().string(), ".old")){std::cout<<"*** Error! File exists from a previous run:"<<(*dir_itr).path().string()<<std::endl;return 1;}
		try{boost::filesystem::rename(*dir_itr, boost::filesystem::path((*dir_itr).path().string()+".old"));}
		catch(std::exception const& e){std::cout<<"*** Error renaming "<<(*dir_itr).path().string()<<" into "<<(*dir_itr).path().string()+".old"<<" What: "<<e.what()<<std::endl;return revert();}
	}
	//
	// repo http://*/packages * -> /var/xpt/packages
	//
	std::vector<std::string> tags;
	std::vector<std::string> packages;
	bool sep = false;
	for (std::vector<std::string>::const_iterator it = arg.begin() ; it != arg.end(); ++it){
		if(it->compare("@")==0){sep=true;continue;};
		if(sep)  tags.push_back(*it);
		else packages.push_back(*it);
	}
	std::unordered_map<std::string, std::array<std::string, 2> > db; // packagename -> {baseurl, version}
	std::ifstream sourceslist(pwd+"/etc/xpt/sources.list");
	if (!sourceslist.good()){std::cout << "*** Error openning: " << pwd+"/etc/xpt/sources.list" << std::endl;}
	std::string s;
	while (std::getline(sourceslist, s)){
		if (s.size()>4&&s.compare(0,5,"repo ")==0){
			std::size_t tagsbeginat = s.find(" ",5);
			std::string repobase = s.substr(5,tagsbeginat-5); // get a string like "http://repo/project/version"
			std::string tagsline = s.substr(tagsbeginat)+" "; // get a string like " any win32 win linux " so we can search a tag unambiguously
			for (std::vector<std::string>::iterator it = tags.begin() ; it != tags.end(); ++it){
				if (tagsline.find(" "+*it+" ")==std::string::npos) continue;
				std::string untransformed = repobase+"/"+*it; // http://127.0.0.1:8080/repos/project/any/
				std::string transformed = untransformed;      // needs transformation into a filename,
				boost::replace_all(transformed, ".", "");     // so it goes http1270018080reposprojectany
				boost::replace_all(transformed, ":", "");     //
				boost::replace_all(transformed, "/", "");     //
				boost::replace_all(transformed, "\\","");     //
				boost::filesystem::path saveas = varxptpackagespath/boost::filesystem::path(transformed);
				if(boost::filesystem::exists(saveas)){std::cout<<"*** Error! Possible duplicate in sources.list when saving "<<untransformed<<std::endl;return revert();}
				newfiles.insert(saveas.string());
				if (!download(untransformed+"/packages", saveas.string())) return revert();
				boost::filesystem::path varxpturl = varxptpackagespath/boost::filesystem::path(transformed+"_url");
			}
		}
	}
	// Everything's done well, remove all *.old files --------------------------
	return removeold();
}
int repo(const std::string& repo){
/*
Example of the packages-file in a repo:
-------------------------------------------------------------------------------
package1_1.2.3-4.zip
package2_5.6.7-8.zip
-------------------------------------------------------------------------------
*/
	boost::filesystem::path r(repo);
	if(boost::filesystem::exists(r)){std::cout<<"*** Error! Already exists: "<<r.filename().string()<<std::endl;return 1;}
	if(!boost::filesystem::create_directories(r)){std::cout<<"*** Error creating: "<<r.filename().string()<<std::endl;return 1;}

	std::ofstream f("packages");
	boost::filesystem::directory_iterator end_iter;
	for (boost::filesystem::directory_iterator dir_itr(boost::filesystem::path(pwd.c_str()));dir_itr != end_iter;++dir_itr){
		std::string filename = boost::filesystem::relative(dir_itr->path()).string();
		if (filename.size()<5) continue;                             // string(".zip").size() == 4
		if(filename.compare(filename.size()-4,4,".zip")!=0)continue; //
		if(!copy(filename,repo)){std::cout<<"*** Error copying package to the repo!"<<std::endl;return 1;}
		f << dir_itr->path().filename().string() << "\n";
	}
	f.close();
	if(!copy("packages",repo)){std::cout<<"*** Error copying packages-file!"<<std::endl;return 1;}
	boost::filesystem::remove(boost::filesystem::path("packages"));
	return 0;
}

bool create(const std::string& dir){
	if(!isdir(dir)){std::cout << "*** Error! Not a directory: " << dir << std::endl;return 1;}
	if(!isdir(dir+"/CONTENT")){std::cout << "*** Error! No CONTENT subdirectory" << std::endl;return 1;}
	if(!isdir(dir+"/PACKAGE")){std::cout << "*** Error! No PACKAGE subdirectory" << std::endl;return 1;}

	std::ifstream fcontrol(dir+"/PACKAGE/control");
	if (!fcontrol.good()){std::cout << "*** Error openning: " << dir+"/PACKAGE" << std::endl;}

	std::string package;
	std::string version;
	std::string depends;
	std::string s;
	while (std::getline(fcontrol, s)){
		if (!std::strncmp(s.c_str(), "Package: ", strlen("Package: "))) package = s.substr(std::string("Package: ").size());
		if (!std::strncmp(s.c_str(), "Version: ", strlen("Version: "))) version = s.substr(std::string("Version: ").size());
		if (!std::strncmp(s.c_str(), "Depends: ", strlen("Depends: "))) depends = s.substr(std::string("Depends: ").size());
	}
	if (!package.size()) {std::cout << "*** Error! No \"Package: \" line in the control file" << std::endl;return 1;}
	if (!version.size()) {std::cout << "*** Error! No \"Version: \" line in the control file" << std::endl;return 1;}
	if (!depends.size()) {std::cout << "*** Error! No \"Depends: \" line in the control file" << std::endl;return 1;}

	std::ofstream fsize(dir+"/PACKAGE/size");
	std::ofstream fmd5sums(dir+"/PACKAGE/md5sums");
	std::ofstream fmd5sum(dir+"/PACKAGE/md5sum");
	fsize.close();
	fmd5sums.close();
	fmd5sum.close();

	std::string cmd;
#if defined(_WIN32) || defined(WIN32)
	std::string app = (boost::filesystem::path(exd) / boost::filesystem::path("7za.exe")).string();
	cmd = app+" a \""+pwd+"\\"+package+"_"+version+".zip\" \""+dir+"/*\"";
#else
#endif
	std::cout << cmd << std::endl;
	if (std::system(cmd.c_str()) != 0){std::cout << "*** Error executing: " << cmd; return false;}
	return true;
}

int main(int argc, char* argv[]){
	exd = boost::filesystem::system_complete(boost::filesystem::path(argv[0])).parent_path().string();

	int shift;
	if (argc > 2 && !strcmp(argv[1],"pwd")){
		if (!isdir(std::string(argv[2]))){std::cout << "*** Error! Not a directory: " << argv[2] << std::endl; return 1;}
		pwd = std::string(argv[2]);
		shift = 2; // parse argv "plus two" as if pwd was not passed
	}else{
		pwd = ".";
		shift = 0;
	}
	varxptpath = boost::filesystem::path(pwd+"/var/xpt");
	if(!boost::filesystem::exists(varxptpath))if(!boost::filesystem::create_directories(varxptpath)){std::cout<<"*** Error creating "<<varxptpath.string()<<std::endl;return false;}
	varxptpackagespath = boost::filesystem::path(pwd+"/var/xpt/packages");
	if(!boost::filesystem::exists(varxptpackagespath))if(!boost::filesystem::create_directories(varxptpackagespath)){std::cout<<"*** Error creating "<<varxptpackagespath.string()<<std::endl;return false;}

	if (argc == shift+3 && !std::strcmp(argv[shift+1],"create")) return create(std::string(argv[shift+2]));
	if (argc == shift+3 && !std::strcmp(argv[shift+1],"repo")) return repo(std::string(argv[shift+2]));
	if (argc >  shift+3 && !std::strcmp(argv[shift+1],"install")) return install(std::vector<std::string>(argv + 2 + shift, argv + argc));

	std::cout << "*** Error! Unknown command line options! Read the manual below:" << std::endl << std::endl;
	usage();
	return 1;
}
