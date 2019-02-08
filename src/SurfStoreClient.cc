#include <sysexits.h>
#include <sys/stat.h>
#include <string>
#include <vector>
#include <algorithm>
#include <fstream>
#include <map>
#include <iostream>
#include <sys/types.h>
#include <dirent.h>
#include <assert.h>

#include "rpc/server.h"
#include "picosha2/picosha2.h"
#include "json/json.hpp"

#include "logger.hpp"
#include "SurfStoreTypes.hpp"
#include "SurfStoreClient.hpp"

using namespace std;
using json = nlohmann::json;

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

	ifstream indexfile(base_dir + "/index.json");
	json j = json::parse(indexfile);

	if (j.find(filename) != j.end()) {
		json entry = j[filename];
		int v = entry["version"];
		list<string> hl = entry["hashlist"];

		return make_tuple(v, hl);

	} else {
		int v = -1;
		list<string> blocklist;
		FileInfo ret = make_tuple(v, list<string>());
		return ret;
	}
}

void SurfStoreClient::set_local_fileinfo(string filename, FileInfo finfo)
{
	std::ifstream i(base_dir + "/index.json");
	json j;

	if (i) {
		i >> j;
	}

	json entry;
	entry["filename"] = filename.c_str();
	entry["version"] = get<0>(finfo);
	entry["hashlist"] = get<1>(finfo);

	j[filename.c_str()] = entry;

	std::ofstream ofs;
	ofs.open(base_dir + "/index.json", std::ofstream::out | std::ofstream::trunc);
	ofs.close();

	std::ofstream outfile(base_dir + "/index.json");
	outfile << j << std::endl;
}
