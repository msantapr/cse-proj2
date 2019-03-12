#include <sysexits.h>
#include <string>
#include <utility>

#include "rpc/server.h"

#include "logger.hpp"
#include "SurfStoreTypes.hpp"
#include "SurfStoreServer.hpp"

SurfStoreServer::SurfStoreServer(INIReader& t_config)
    : config(t_config)
{
    auto log = logger();

	// pull the address and port for the server
	string servconf = config.Get("ssd", "server", "");
	if (servconf == "") {
		log->error("Server line not found in config file");
		exit(EX_CONFIG);
	}
	size_t idx = servconf.find(":");
	if (idx == string::npos) {
		log->error("Config line {} is invalid", servconf);
		exit(EX_CONFIG);
	}
	port = strtol(servconf.substr(idx+1).c_str(), nullptr, 0);
	if (port <= 0 || port > 65535) {
		log->error("The port provided is invalid: {}", servconf);
		exit(EX_CONFIG);
	}
	blockSize = stoi(config.Get("ss", "blocksize", ""));
	log->info("blocksize: {}", blockSize);

}

void SurfStoreServer::launch()
{
    auto log = logger();

    log->info("Launching SurfStore server");
    log->info("Port: {}", port);

	rpc::server srv(port);

	srv.bind("ping", []() {
		auto log = logger();
		log->info("ping()");
		return;
	});

	//TODO: get a block for a specific hash 			
	// (for downloading files, just donwload the entire thing. use hashBlocks) -> helper in client to download files
	srv.bind("get_block", [this](string hash) {

		auto log = logger();
		log->info("get_block()");
		log->info("retrieving hash {}", hash);
		log->info("the retrieved value is {}", hashBlocks[hash]); 

		return hashBlocks[hash];		// return the string (non null terminated) of the data corresponding to this hash. client will handle transofrming data to file
	});

	//TODO: store a block 		 				
	// (for updating files on cloud, just upload the etnire thing not just the diff) use filemapInfo and hashblocks)
	// helper in client for uploading files
	srv.bind("store_block", [this](string hash, string data) {

		auto log = logger();
		log->info("store_block()");
		//log->info("data {}" , data);
		log->info("data size {}", data.size());
		hashBlocks.insert(make_pair(hash,data));

		return;
	});

	//TODO: download a FileInfo Map from the server 		(download it as the index.txt)
	srv.bind("get_fileinfo_map", [this]() {
		auto log = logger();
		log->info("get_fileinfo_map()");

		/*FileInfo file1;
		get<0>(file1) = 42;
		get<1>(file1) = { "h0", "h1", "h2" };

		FileInfo file2;
		get<0>(file2) = 20;
		get<1>(file2) = { "h3", "h4" };

		FileInfoMap fmap;
		fmap["file1.txt"] = file1;
		fmap["file2.dat"] = file2;*/

		return remoteMap;
	});

	//TODO: update the FileInfo entry for a given file    (call when you sync changes to cloud to update server map)
	srv.bind("update_file", [this](string filename, FileInfo finfo) { // need to include creation case
		auto log = logger();
		int parameter_version = get<0>(finfo);
		list<string> new_hashlist = get<1>(finfo);
		log->info("update_file()");

		try {
			FileInfo f = remoteMap.at(filename);
		}
		catch (const std::out_of_range& oor) {
			get<0>(remoteMap[filename])++;
			get<1>(remoteMap[filename]) = new_hashlist;
			return 1;
		}

		if( parameter_version != get<0>(remoteMap.at(filename)) ) { // check passed in parameter version 
			log->info("someone barely beat you");
			return 0;
		}

		get<0>(remoteMap[filename])++;
		get<1>(remoteMap[filename]) = new_hashlist;

		//printRemoteMap();
		return 1;
		
	});

	// You may add additional RPC bindings as necessary

	srv.run();
}

void SurfStoreServer::printRemoteMap() {
	auto log = logger();
  FileInfoMap :: iterator p;

  for (p = remoteMap.begin(); p != remoteMap.end(); p++) {
  	log->info("file name {}", p->first);
  	log->info("version {}", get<0>(p->second));
  	auto it = get<1>(p->second).begin();

  	for(it = get<1>(p->second).begin(); it != get<1>(p->second).end(); it++) {
  		log->info("hash {}", *it);
  		log->info("data {}", hashBlocks.at(*it));
  	}
  }

}
/*
 * client manages their own base_dir files. 
 * any updates to that clients files are managed in their local index.
 * situation 1: client makes a new file
 * use set_local_fileinfo to make a new entry to client index.
 * situation 2: client updates a file
 * use get_local_fileinfo to get that entry in client index.
 * update that fileInfo's version number and hash list
 * situation 3: 

*/


/*
 * if remote map and local map arent the same...
 * 1) if there is a new entry in the cloud not in client, just download that entry on sync
 * 2) if there is an entry with a new hashlist in the cloud not in client, downlod that entry on sync
 * 3) if there is an new on client not in cloud, upload that entry on cloud
 * 4) if there is an entry on client with different hashlist than cloud, upload entry on cloud (update version + data)



 * at the end of sync, redownload new server map and put onto local index */
