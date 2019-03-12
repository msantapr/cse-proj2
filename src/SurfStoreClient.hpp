#ifndef SURFSTORECLIENT_HPP
#define SURFSTORECLIENT_HPP

#include <string>
#include <list>

#include "inih/INIReader.h"
#include "rpc/client.h"

#include "logger.hpp"
#include "SurfStoreTypes.hpp"

using namespace std;

class SurfStoreClient {
public:
    SurfStoreClient(INIReader& t_config);
    ~SurfStoreClient();
    FileInfoMap localMap;

	void sync(); // sync the base_dir with the cloud

	const uint64_t RPC_TIMEOUT = 100; // milliseconds

protected:

    INIReader& config;
	string serverhost;
	int serverport;
	string base_dir;
	int blocksize;

	rpc::client * c;

	// helper functions to get/set from the local index file
	FileInfo get_local_fileinfo(string filename);
	void set_local_fileinfo(string filename, FileInfo finfo);

	// helper functions to get/set blocks to/from local files
	list<string> get_blocks_from_file(string filename);

	list<string> set_hashes_from_blocks(list<string> blocks);

	void create_file_from_blocklist(string filename, list<string> blocks);

	void read_directory(const std::string& name, vector<string> &files);

	FileInfoMap check_for_local_changes(vector<string> files);

	vector<FileInfoMap> find_potential_uploads_and_downloads(FileInfoMap &local, FileInfoMap &remote);

	FileInfoMap check_for_deleted_locals(vector<string> &files);

};

#endif // SURFSTORECLIENT_HPP
