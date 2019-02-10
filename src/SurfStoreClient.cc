#include <sysexits.h>
#include <sys/stat.h>
#include <string>
#include <vector>
#include <algorithm>
#include <fstream>
#include <map>
#include <iostream>
#include <sstream>
#include <fstream>
#include<string>

#include <sys/types.h>
#include <dirent.h>
#include <assert.h>


#include "rpc/server.h"
#include "picosha2/picosha2.h"

#include "logger.hpp"
#include "SurfStoreTypes.hpp"
#include "SurfStoreClient.hpp"

using namespace std;

SurfStoreClient::SurfStoreClient(INIReader& t_config)
    : config(t_config), c(nullptr)
{
    auto log = logger();

	// pull the address and port for the server
	string serverconf = config.Get("ssd", "server", "");
	if (serverconf == "") {
		log->error("The server was not found in the config file");
		exit(EX_CONFIG);
	}
	size_t idx = serverconf.find(":");
	if (idx == string::npos) {
		log->error("Config line {} is invalid", serverconf);
		exit(EX_CONFIG);
	}
	serverhost = serverconf.substr(0,idx);
	serverport = strtol(serverconf.substr(idx+1).c_str(), nullptr, 0);
	if (serverport <= 0 || serverport > 65535) {
		log->error("The port provided is invalid: {}", serverconf);
		exit(EX_CONFIG);
	}

    base_dir = config.Get("ss", "base_dir", "");
    blocksize = config.GetInteger("ss", "blocksize", 4096);

    log->info("Launching SurfStore client");
    log->info("Server host: {}", serverhost);
    log->info("Server port: {}", serverport);

	c = new rpc::client(serverhost, serverport);
}

SurfStoreClient::~SurfStoreClient()
{
	if (c) {
		delete c;
		c = nullptr;
	}
}

void SurfStoreClient::sync() {
    auto log = logger();

	log->info("Calling ping");
	c->call("ping");

	log->info("Calling get_block())");
	string block = c->call("get_block", "1234").as<string>();

	log->info("Calling get_fileinfo_map())");
	FileInfoMap map = c->call("get_fileinfo_map").as<FileInfoMap>();
	FileInfo file1 = map["file1.txt"];
	int v = get<0>(file1);
	log->info("got a version of {}", v);

	FileInfo file2 = map["file2.dat"];
	v = get<0>(file2);
	log->info("got a version of {}", v);
}

FileInfo SurfStoreClient::get_local_fileinfo(string filename) {
  auto log = logger();
  log->info ("get_local_fileinfo {}", filename);
  ifstream f(base_dir + "/index.txt");
  if (f.fail()) {
    int v = -1;
  	list<string> blocklist;
  	FileInfo ret = make_tuple(v, list<string>());
    return ret;
  }
  do {
    vector<string> parts;
    string x;
    getline(f,x);
    stringstream ss(x);
    string tok;
    while(getline(ss, tok, ' ')) {
      parts.push_back(tok);
    }
    if (parts.size() > 0 && parts[0] == filename) {
      list<string> hl (parts.begin() + 2, parts.end());
      int v = stoi(parts[1]);
      return make_tuple(v,hl);
    }
  } while(!f.eof());
  int v = -1;
	list<string> blocklist;
	FileInfo ret = make_tuple(v, list<string>());
  return ret;
}

void SurfStoreClient::set_local_fileinfo(string filename, FileInfo finfo) {
  auto log = logger();
  log->info("set local file info");
  std::ifstream f(base_dir + "/index.txt");
  std::ofstream out(base_dir + "/index.txt.new");
  int v = get<0>(finfo);
  list<string> hl = get<1>(finfo);
  bool set = false;
  do {
    string x;
    vector<string> parts;
    getline(f,x);
    stringstream ss(x);
    string tok;
    while(getline(ss, tok, ' ')) {
      parts.push_back(tok);
    }
    if (parts.size() > 0) {
        if ( parts[0] == filename) {
          set = true;
          out << filename << " "<<v<<" ";
          for (auto it : hl) out<<it<<" ";
          out.seekp(-1,ios_base::cur);
          out <<"\n";
        }
        else {
          out << x<<"\n";
        }
      }
    else break;
  } while(!f.eof());
  if (!set) {
    out << filename <<" "<< v<< " ";
    for (auto it : hl) out<<it<<" ";
    out.seekp(-1,ios_base::cur);
    out <<"\n";
  }
  out.close();
  f.close();
  string real = string(base_dir + "/index.txt");
  string bkp = string(base_dir + "/index.txt.new");

  remove(real.c_str());
  rename(bkp.c_str(),real.c_str());

}
