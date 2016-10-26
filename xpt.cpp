#include <iostream>
#include <boost/version.hpp>
#include <curl/curlver.h>
void usage(){
	std::cout<<"XPT "<<VERSION<<" "
					 <<"("
					 <<"Boost "<<BOOST_VERSION/100000<<"."
										 <<BOOST_VERSION/100%1000<<"."
										 <<BOOST_VERSION%100<<", "
					 <<"Curl " <<LIBCURL_VERSION
					 <<")"<<std::endl;
	std::cout<<"Usage:"<<std::endl;
	std::cout<<"  xpt [wd <directory to create zip in>] create <unzipped package>"<<std::endl;
	std::cout<<"  xpt [wd <sandbox>] install <package 1> <package N> [@ <tag>]"<<std::endl;
	std::cout<<"  xpt [wd <sandbox>] uninstall"<<std::endl;
}

#include <sstream>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <fstream>
#include <array>
#include <vector>
#include <iterator>
#include <unordered_set>
#include <unordered_map>
#include <regex>

#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/regex.hpp>

// GLOBAL VARS USED EVERYWHERE THRU THE CODE -----------------------------------
std::unordered_set<std::string> newfiles; // store here every newly created filepath

// UNIVERSAL HELPER FUNCTIONS --------------------------------------------------
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
	return false;
}

// CURL DOWNLOAD FILE PART -----------------------------------------------------
#define CURL_STATICLIB // required when using static libcurl
#include <curl/curl.h>
struct myprogress {
	double lastruntime;
	CURL *curl;
};
/* this is how the CURLOPT_XFERINFOFUNCTION callback works */
static int xferinfo(void *p,
										curl_off_t dltotal, curl_off_t dlnow,
										curl_off_t ultotal, curl_off_t ulnow)
{
	struct myprogress *myp = (struct myprogress *)p;
	CURL *curl = myp->curl;
	double curtime = 0;

	curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &curtime);

	/* under certain circumstances it may be desirable for certain functionality
		 to only run every N seconds, in order to do this the transaction time can
		 be used */
	if((curtime - myp->lastruntime) >= 1) {
		myp->lastruntime = curtime;
		std::cout<<".";
		//fprintf(stderr, "TOTAL TIME: %f \r\n", curtime);
	}
	//fprintf(stderr, "UP: %" CURL_FORMAT_CURL_OFF_T " of %" CURL_FORMAT_CURL_OFF_T
	//				"  DOWN: %" CURL_FORMAT_CURL_OFF_T " of %" CURL_FORMAT_CURL_OFF_T
	//				"\r\n",
	//				ulnow, ultotal, dlnow, dltotal);

	return 0;
}
static int older_progress(void *p,
													double dltotal, double dlnow,
													double ultotal, double ulnow)
{
	return xferinfo(p,
									(curl_off_t)dltotal,
									(curl_off_t)dlnow,
									(curl_off_t)ultotal,
									(curl_off_t)ulnow);
}
static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream){return fwrite(ptr, size, nmemb, (FILE *)stream);}
int download(const std::string& src, const std::string& dst){
	struct myprogress prog;
	CURL *curl_handle;
	CURLcode code = CURLE_OK;
	curl_handle = curl_easy_init();
	if(curl_handle){
		prog.lastruntime = 0;
		prog.curl = curl_handle;
		curl_easy_setopt(curl_handle, CURLOPT_URL, src.c_str());
		curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, false);
		curl_easy_setopt(curl_handle, CURLOPT_PROGRESSFUNCTION, older_progress);
		curl_easy_setopt(curl_handle, CURLOPT_PROGRESSDATA, &prog);
		curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data);
		curl_easy_setopt(curl_handle, CURLOPT_FAILONERROR, true);
		curl_easy_setopt(curl_handle, CURLOPT_XFERINFOFUNCTION, xferinfo);
		curl_easy_setopt(curl_handle, CURLOPT_XFERINFODATA, &prog);
		curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 0L);
		FILE *pagefile;
		pagefile = fopen(dst.c_str(), "wb");
		if(pagefile){
			curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, pagefile);
			code = curl_easy_perform(curl_handle);
			fclose(pagefile);
			if(code)return code;
		}
	curl_easy_cleanup(curl_handle);
	}
	return code;
}
// MAIN ------------------------------------------------------------------------
int err1(const std::string& s){std::cout << "*** Error: " << s << std::endl;return 1;}
void log(const std::string& s){std::cout<<s<<std::endl;}
void log(const std::unordered_map<std::string, std::vector<std::string> >& map, std::string title="", std::string indent=""){
	if(!title.empty())std::cout<<indent<<title<<std::endl;
	int keymaxlength = 0;
	for(const auto &elem: map){
		if(keymaxlength<elem.first.size())keymaxlength=int(elem.first.size());
	}
	for(const auto &elem: map){
		std::string key = elem.first;
		key.insert(elem.first.size(),keymaxlength-elem.first.size(),' ');
		std::cout<<indent<<"  "<<key<<" "<<elem.second.at(0)<<" "<<elem.second.at(1)<<std::endl;
	}
}
void revert(){
}
int uninstall(const boost::filesystem::path& pwd,
							const boost::filesystem::path& db,
							const std::vector<std::string>& arg){

	if(arg.size()==0){
		log("Uninstalling all packages:");
		boost::filesystem::directory_iterator end_iter;
		for(boost::filesystem::directory_iterator dir_iter(db) ; dir_iter != end_iter ; ++dir_iter){
			log("  "+dir_iter->path().filename().string());
			boost::filesystem::path md5sumstxt(dir_iter->path()/"content.txt");
			std::ifstream f(md5sumstxt.string());
			if(!f.good())return err1("open "+md5sumstxt.string());
			std::string s;
			while(std::getline(f,s)){
				try{
					boost::filesystem::remove(pwd/s);
				}catch(std::exception const& e){
					return err1(e.what());
				}
			}
			f.close();
			try{
				boost::filesystem::remove_all(dir_iter->path());
			}catch(std::exception const& e){
				return err1(e.what());
			}
		}
	}
	return 0;
}
int install_recursively(const boost::filesystem::path& pwd,
												const boost::filesystem::path& db,
												const boost::filesystem::path& archive,
												const std::string& compressapp,
												std::unordered_map<std::string,std::vector<std::string> >& map,
												std::string package){
	std::string s;

	// Проверяем, есть ли вообще пакет в репах из sources.txt
	std::unordered_map<std::string,std::vector<std::string> >::const_iterator it = map.find(package);
	if(it==map.end())
		return err1("Package not found in repos in sources.txt: "+package);

	// Удаляем устаревший пакет, если надо
	boost::filesystem::path unpacked = db/boost::filesystem::path(package);
	boost::filesystem::path controltxt(unpacked/"control.txt");
	boost::filesystem::path contenttxt(unpacked/"content.txt");
	std::ifstream fcontroltxt(controltxt.string());
	bool install = true;
	if(fcontroltxt.good()){
		std::string version;
		while (std::getline(fcontroltxt,s)){
			if (!std::strncmp(s.c_str(), "Version: ", strlen("Version: "))){
				version = s.substr(std::string("Version: ").size());
				break;
			}
		}
		fcontroltxt.close();
		fcontroltxt.clear();
		if ((it->second.at(0)).compare(version)){
			log("Uninstall old version "+version);
			std::ifstream fcontenttxt(contenttxt.string());
			if(!fcontenttxt.good())return err1("Open "+contenttxt.string());
			while(std::getline(fcontenttxt,s)){
				try{
					boost::filesystem::remove(pwd/s);
				}catch(std::exception const& e){
					return err1(e.what());
				}
			}
			fcontenttxt.close();
			try{
				boost::filesystem::remove_all(unpacked);
			}catch(std::exception const& e){
				return err1(e.what());
			}
		}else{
			log("Already installed "+package+" "+version);
			install = false;
		}
	}

	// Производим установку пакета (если надо)
	if (install) {
		// Выкачиваем
		std::string from = it->second.at(1);
		std::string to   = (archive/boost::filesystem::path(package+"_"+it->second.at(0)+".zip")).string();
		log("Donwload "+from);
		if(download(from,to)>0) return err1("Download");

		// Распаковываем
		std::string cmd;
	#if defined(_WIN32) || defined(WIN32)
		cmd = compressapp+" x \""+to+"\" -o\""+unpacked.string()+"\" -y > nul";
	#else
		cmd = compressapp+" x \""+to+"\" -o\""+unpacked.string()+"\" -y > /dev/null";
	#endif
		log("Unpack "+cmd);
		if (std::system(cmd.c_str())!=0)return err1("Unpack");

		// Переносим содержимое в песочницу и заполняем content.txt
		boost::filesystem::path content(unpacked/"CONTENT");
		std::vector<boost::filesystem::path> paths;
		std::string relativepaths;
		std::copy(boost::filesystem::recursive_directory_iterator(content.string()),
							boost::filesystem::recursive_directory_iterator(),
							std::back_inserter(paths));
		for(const auto& path: paths){
			if(!boost::filesystem::is_directory(path)){
				boost::filesystem::path relative((path.string()).substr(content.string().size()+1,path.string().size()));
				relativepaths+=relative.string()+"\n";
				boost::filesystem::path moveto(pwd/relative);
				boost::filesystem::path dir = moveto.parent_path();
				if(!boost::filesystem::is_directory(dir))
					try{boost::filesystem::create_directories(dir);}catch(std::exception const& e){return err1(e.what());}
				if(boost::filesystem::exists(moveto)){
					return err1("File already exists: "+moveto.string());
				}else{
					try{boost::filesystem::remove(moveto);}catch(std::exception const& e){return err1(e.what());}
				}
				try{boost::filesystem::rename(path,moveto);}catch(std::exception const& e){return err1(e.what());}
			}
		}
		std::ofstream fcontenttxt(contenttxt.string());
		if(fcontenttxt.bad())return err1("Open "+contenttxt.string());
		fcontenttxt<<relativepaths;
		fcontenttxt.close();
	}

	// Идём дальше по рекурсивно по дереву зависимостей
	fcontroltxt.open(controltxt.string());
	if(fcontroltxt.bad())return err1("Open "+controltxt.string());
	std::string depends;
	while (std::getline(fcontroltxt,s)){
		if (!std::strncmp(s.c_str(), "Depends: ", strlen("Depends: "))){
			depends = s.substr(std::string("Depends: ").size());
			break;
		}
	}
	fcontroltxt.close();
	if(!depends.empty()){
		std::istringstream issdepends(depends);
		std::vector<std::string> dependantpackages{std::istream_iterator<std::string>{issdepends},
																							 std::istream_iterator<std::string>{}};
		for(const auto& dependantpackage: dependantpackages){
			install_recursively(pwd,db,archive,compressapp,map,dependantpackage);
		}
	}

	return 0;
}
int install(const boost::filesystem::path& wd,
						const boost::filesystem::path& xpt,
						const boost::filesystem::path& sourcestxt,
						const boost::filesystem::path& db,
						const boost::filesystem::path& session,
						const std::string& compressapp,
						const std::vector<std::string>& arg){
	boost::filesystem::path packages(session/"packages");
	boost::filesystem::path urls(session/"urls");
	boost::filesystem::path archive(session/"archive");
	try{
		boost::filesystem::create_directories(urls);
		boost::filesystem::create_directories(packages);
		boost::filesystem::create_directories(archive);
	}catch(std::exception const& e){
		return err1(e.what());
	}

	log("Parsing: "+sourcestxt.string());
	std::vector<std::string> repos;
	std::string tag = (arg.size()>=3 && arg.at(arg.size()-2)=="@") ? arg.at(arg.size()-1) : "";
	std::vector<std::string> mapped;
	std::ifstream f(sourcestxt.string());
	if(!f.good())return err1("open "+sourcestxt.string());
	std::string s;        int n=0;
	while(std::getline(f, s)){n++;s=s.substr(0,s.find("#"));if(s.empty())continue;
		if(!std::strncmp(s.c_str(), "map ", strlen("map "))){
			if(!mapped.empty())continue;
			std::istringstream iss(s);
			std::vector<std::string> tokens{std::istream_iterator<std::string>{iss},std::istream_iterator<std::string>{}};
			if (tokens.size()<3) return err1("map must have at least 3 words: "+sourcestxt.string()+":"+std::to_string(n));
			if (tokens.at(1)==tag){
				mapped = tokens;
				std::vector<std::string>(mapped.begin()+2, mapped.end()).swap(mapped);
			}
			continue;
		}
		if(!std::strncmp(s.c_str(), "repo ", strlen("repo "))){
			std::istringstream iss(s);
			std::vector<std::string> tokens{std::istream_iterator<std::string>{iss},std::istream_iterator<std::string>{}};
			if(tokens.size()<2)return err1("repo must have at least 2 words: "+sourcestxt.string()+":"+std::to_string(n));
			if(tokens.size()==2)
				repos.push_back(tokens.at(1));
			else {
				bool hasany=false;
				std::string usetag="";
				for(auto const& token: tokens){
					if(token=="any")hasany=true;
					if(token==tag)
						usetag=tag;
					else
						if(std::find(mapped.begin(), mapped.end(), token) != mapped.end())
							usetag=token;
					if (usetag!="")break;
				}
				if(usetag!=""){
					if(hasany)repos.push_back(tokens.at(1)+"/any");
					repos.push_back(tokens.at(1)+"/"+usetag);
				}
			}
		}
	}

	log("Downloading from remotes:");
	for(auto const& repo: repos){
		std::string saveas = repo+".txt";
		boost::replace_all(saveas, "http://", ""); //
		boost::replace_all(saveas, "\\\\",    ""); // http://127.0.0.1:8080/repos/project/6.6.6/any
		boost::replace_all(saveas, ":",      "-"); //        ->
		boost::replace_all(saveas, "/",      "-"); //        packages-127.0.0.1-8080-repos-project-6.6.6-any.txt
		boost::replace_all(saveas, "\\",     "-"); //
		std::string from = repo+"/packages.txt";
		std::string to   = (packages/saveas).string();
		int code = download(from, to);
		if (code!=0) return err1("curl code="+std::to_string(code)+"\nfrom="+from+"\nto="+to);
		to = (urls / boost::filesystem::path(saveas)).string();
		std::ofstream f(to);
		f << repo;
		f.close();
	}

	log("Creating map of all available packages:");
	std::unordered_map<std::string, std::vector<std::string> > map;
	log("  Parsing files:");
	boost::filesystem::directory_iterator end_iter;
	for(boost::filesystem::directory_iterator dir_iter(packages) ; dir_iter != end_iter ; ++dir_iter){
		log("    ~"+(urls / dir_iter->path().filename()).string());
		std::string url;
		std::ifstream furl((urls / dir_iter->path().filename()).string());std::getline(furl,url);furl.close();
		if(url.empty())return err1("reading session/urls");
		log("    ~"+dir_iter->path().string());
        std::ifstream fpackages(dir_iter->path().string());
		std::string s;                    int n=0;
		while(std::getline(fpackages,s)){   ++n;
			boost::trim(s);
			if(s.empty())continue;
			std::string filename = s;
			std::string package;
			std::string version;
            boost::smatch match;
            if(boost::regex_search(s, match, boost::regex("([a-zA-Z0-9.-]+)_([a-zA-Z0-9.-]+).zip"))){
				package = match[1];
				version = match[2];
            }else
                return err1("parsing line "+std::to_string(n)+": `"+s+"`");
			if(map.count(package)>0){
				return err1("same name package in two different repositories:\n1>"+url+"/"+filename+"\n2>"+map.find(package)->second.at(1));
			}
			map.insert(std::pair<std::string, std::vector<std::string> >(package,std::vector<std::string>{version,url+"/"+filename}));
		}
		f.close();
	}log(map,"Map:","    ");

	for(const auto& package:arg){
		if(package=="@")break;
		if(install_recursively(wd,db,archive,compressapp,map,package))
			return 1;
	}

	return 0;
}
int create(const boost::filesystem::path& pwd, const boost::filesystem::path& compressapp, const std::string& arg){
	boost::filesystem::path packagesource = boost::filesystem::canonical(arg);
	boost::filesystem::path content(packagesource/"CONTENT");
	if(!boost::filesystem::is_directory(packagesource))return err1("not a directory: "+packagesource.string());
	if(!boost::filesystem::is_directory(content))      return err1("not a directory: "+content.string());
	if(!boost::filesystem::is_directory(packagesource))      return err1("not a directory: "+packagesource.string());

	boost::filesystem::path controltxt(packagesource/"control.txt");
	std::ifstream fcontroltxt(controltxt.string());
	if(!fcontroltxt.good())return err1("open "+controltxt.string());

	std::string name = (packagesource.filename()).string();
	std::string version;
	std::string depends;
	std::string s;
	while (std::getline(fcontroltxt,s)){
		if (!std::strncmp(s.c_str(), "Version: ", strlen("Version: "))) version = s.substr(std::string("Version: ").size());
		if (!std::strncmp(s.c_str(), "Depends: ", strlen("Depends: "))) depends = s.substr(std::string("Depends: ").size());
	}
	fcontroltxt.close();
	if (name.empty())    return err1("package name");
	if (version.empty()) return err1("package version");

	boost::filesystem::path zip(pwd/(name+"_"+version+".zip"));
	if(boost::filesystem::exists(zip))return err1("file already exists: "+zip.string());

		std::string cmd = compressapp.string()+" a \""+zip.string()+"\" \"./"+arg+"/*\"";
	if (std::system(cmd.c_str()) != 0)return err1(cmd);
	return 0;
}
int main(int argc, char* argv[]){
	std::string exedir = boost::filesystem::system_complete(boost::filesystem::path(argv[0])).parent_path().string();
	boost::filesystem::path wd;
	int shift;
	if (argc > 2 && !strcmp(argv[1],"wd")){
		if (!boost::filesystem::is_directory(std::string(argv[2]))){std::cout << "*** Error! Not a directory: " << argv[2] << std::endl; return 1;}
		wd = std::string(argv[2]);
		shift = 2; // parse argv "plus two" as if pwd was not passed
	}else{
		wd = ".";
		shift = 0;
	}
	wd = wd.make_preferred().string();
    std::string compressapp;
#if defined(_WIN32) || defined(WIN32)
    compressapp = (boost::filesystem::system_complete(boost::filesystem::path(argv[0])).parent_path()/boost::filesystem::path("7za.exe")).string();
#else
    compressapp = "7za";
#endif
	if(argc==shift+3&&!std::strcmp(argv[shift+1],"create"))return create(wd, compressapp, std::string(argv[shift+2]));

	boost::filesystem::path xpt(wd/"etc"/"xpt");
	boost::filesystem::path db(xpt/"db");
	boost::filesystem::path session(xpt/"session");
	boost::filesystem::path sourcestxt(xpt/"sources.txt");
	try{
		if(!boost::filesystem::exists(xpt))    boost::filesystem::create_directories(xpt);
		if(!boost::filesystem::exists(db))     boost::filesystem::create_directories(db);
		if( boost::filesystem::exists(session))boost::filesystem::remove_all(session);
																					 boost::filesystem::create_directories(session);
	}catch(std::exception const& e){
		return err1(e.what());
	}
	if(!boost::filesystem::exists(sourcestxt)){
		std::ofstream fsourcestxt(sourcestxt.string());
		fsourcestxt.close();
	}
	std::ofstream argvs((session / boost::filesystem::path("argvs.txt")).string());
	std::copy(argv, argv+argc, std::ostream_iterator<std::string>(argvs," "));
	argvs.close();
	if(argc>shift+2&&!std::strcmp(argv[shift+1],"install")) return install(wd,xpt,sourcestxt,db,session,compressapp,std::vector<std::string>(argv+2+shift,argv+argc));

	if(argc>1)err1("Unknown command line options! See usage:");
	usage();
	return 1;
}
