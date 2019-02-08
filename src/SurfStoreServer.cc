#include <sysexits.h>
#include <string>

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
	srv.bind("get_block", [](string hash) {
		(void) hash; // prevent warning (remove before submitting)

		auto log = logger();
		log->info("get_block()");

		return string();
	});

	//TODO: store a block
	srv.bind("store_block", [](string hash, string data) {
		(void) hash; // prevent warning (remove before submitting)
		(void) data; // prevent warning (remove before submitting)

		auto log = logger();
		log->info("store_block()");

		return;
	});

	//TODO: download a FileInfo Map from the server
	srv.bind("get_fileinfo_map", []() {
		auto log = logger();
		log->info("get_fileinfo_map()");

		FileInfo file1;
		get<0>(file1) = 42;
		get<1>(file1) = { "h0", "h1", "h2" };

		FileInfo file2;
		get<0>(file2) = 20;
		get<1>(file2) = { "h3", "h4" };

		FileInfoMap fmap;
		fmap["file1.txt"] = file1;
		fmap["file2.dat"] = file2;

		return fmap;
	});

	//TODO: update the FileInfo entry for a given file
	srv.bind("update_file", [](string filename, FileInfo finfo) {
		(void) filename; // prevent warning (remove before submitting)
		(void) finfo; // prevent warning (remove before submitting)
	});

	// You may add additional RPC bindings as necessary

	srv.run();
}
