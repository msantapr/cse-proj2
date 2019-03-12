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
#include <string>

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
  log->info("syncing...");

  vector<string> files;   
  read_directory(base_dir, files);

  FileInfoMap potentials = check_for_local_changes(files);      // potential files to upload
  FileInfoMap remote = c->call("get_fileinfo_map").as<FileInfoMap>();      // get server map

  vector<FileInfoMap> stuff = find_potential_uploads_and_downloads(potentials,remote);
  FileInfoMap uploads = stuff[0];
  FileInfoMap downloads = stuff[1];

  FileInfoMap toDelete = check_for_deleted_locals(files);
  if(!toDelete.empty())
    uploads.insert(toDelete.begin(), toDelete.end());   // just put things into uploads since you're uploading deleted files

  FileInfoMap::iterator u;        // uploads iterator
  FileInfoMap::iterator d;        // downloads interator

  list<string>::iterator b;
  list<string>::iterator h;


  // upload items that are either not in the cloud or versions are potentially the same
  for(u = uploads.begin(); u != uploads.end(); u++) {

    list<string> blocks = get_blocks_from_file(u->first);       // data blocks of file
    //int version = get<0>(p->second);
    list<string> hashes = get<1>(u->second);
    list<string>::iterator b;
    list<string>::iterator h;
    
    int status = (c->call("update_file", u->first, u->second)).as<int>();
  
    if(status == 0) {
      downloads.insert(make_pair(u->first, u->second));    // dont upload, download due to version conflict
      continue;
    }

    for(b = blocks.begin(), h = hashes.begin(); b != blocks.end(), h != hashes.end(); b++, h++) {  
      c->call("store_block",*h, *b);    
    }

  }

  FileInfoMap new_remote = c->call("get_fileinfo_map").as<FileInfoMap>(); // safer to download map again

  // handle downloads
  for(d = downloads.begin(); d != downloads.end(); d++) {
        
    FileInfo fileToDownload = new_remote.at(d->first);        // wrap in try/catch for delete situation

    list<string> hashes = get<1>(fileToDownload);
    list<string>::iterator h;
    list<string> new_blocks;

    for(h = hashes.begin(); h != hashes.end(); h++) {
      string block = c->call("get_block",*h).as<string>();
      new_blocks.push_back(block);
    }

    create_file_from_blocklist(d->first, new_blocks);

  }


  // update our index after all modifications occurred
  FileInfoMap downloadRemote = c->call("get_fileinfo_map").as<FileInfoMap>(); 

  FileInfoMap::iterator q;

  for(q = downloadRemote.begin(); q != downloadRemote.end(); q++) {
    set_local_fileinfo(q->first, q->second);
  }


/*	log->info("Calling ping");
	c->call("ping");

  list<string> s = get_blocks_from_file("hello.txt");
  log->info("size: {}", s.size());

	log->info("Calling get_fileinfo_map())");
	FileInfoMap map = c->call("get_fileinfo_map").as<FileInfoMap>();
  c->call("store_block","hasher","asdf_pie");

  log->info("Calling get_block())");
  string block = c->call("get_block", "hasher").as<string>();
  log->info("get block return {}", block);

	FileInfo file1 = map["file1.txt"];
	int v = get<0>(file1);
	log->info("got a version of {}", v);

	FileInfo file2 = map["file2.dat"];
	v = get<0>(file2);
	log->info("got a version of {}", v); */
}

FileInfo SurfStoreClient::get_local_fileinfo(string filename) { // loop through using to locate changes
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

void SurfStoreClient::set_local_fileinfo(string filename, FileInfo finfo) {   // use iterator loop through the returned fmap to update local index
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

// also uses actual data blocks as opposed to hash values
list<string> SurfStoreClient::get_blocks_from_file(string filename) {
  auto log = logger();
  list<string> blocks;

  log->info("attempting to get blocks from file {}", filename);
  std::ifstream f(base_dir + "/" + filename);
  if(f.fail()) {
    log->info("{} does not exist or something bad happened", filename);
    blocks.push_back("garbage");
    return blocks;
  }

  f.seekg(0, f.end);
  int fileLength = f.tellg();     
  f.seekg(0, f.beg);

  unsigned int blockAmount = (fileLength/blocksize) + 1;

  log->info("about to read {} from {}", fileLength, filename);

  while(fileLength > 0) {
    char * buffer = new char[blocksize];
    f.read(buffer, blocksize);
    int charRead = f.gcount();
    //log->info("charRead is : {}", charRead);
    std::string s(buffer, charRead);
    blocks.push_back(s);
    fileLength -= charRead;
  }

  f.close();

  if(blockAmount != blocks.size()) {
    log->info("Err: expected {} blocks to return but received {} blocks instead", blockAmount, blocks.size());
    return blocks;
  }
  
  log->info("Success: returned {} blocks", blocks.size());
  return blocks;
}


// this assumes that blocks is the binary data, not the hash. do that conversion in sync
void SurfStoreClient::create_file_from_blocklist(string filename, list<string> blocks) {
  auto log = logger();
  log->info("creating {} with {} blocks", filename, blocks.size());
  // need to take care of case when updating rather than just creating

  std::ofstream out(base_dir + "/" + filename + ".new");

  while(!blocks.empty()) {
    out << blocks.front();
    blocks.pop_front();
  }

  out.close();

  string real = string(base_dir + "/" + filename);
  string bkp = string(base_dir + "/" + filename + ".new");

  remove(real.c_str());
  rename(bkp.c_str(),real.c_str());

}

list<string> SurfStoreClient::set_hashes_from_blocks(list<string> blocks) {
  auto log = logger();
  list<string> hashlist;

  log->info("creating hashlist");
  while(!blocks.empty()) {
    string key = picosha2::hash256_hex_string(blocks.front());
    hashlist.push_back(key);
    blocks.pop_front();
  }

  return hashlist;
}

void SurfStoreClient::read_directory(const std::string& name, vector<string> &files) {
    auto log = logger();
    log->info("reading client directory");
    DIR* dirp = opendir(name.c_str());

    struct dirent * dp;
    while ((dp = readdir(dirp)) != NULL) {

      if( (strcmp(dp->d_name, ".") == 0) || (strcmp(dp->d_name, "..") == 0) || (strcmp(dp->d_name, ".gitkeep") == 0) || (strcmp(dp->d_name, "index.txt") == 0)) // take out gitkeep later
        continue;

      files.push_back(dp->d_name);
    }

    log->info("Files in client directory are");
    for(auto i : files) 
      log->info("{}", i);
    
    closedir(dirp);
}

// computes hash for current files in client dir, returns a list of files that have changed since last sync exec & new files
FileInfoMap SurfStoreClient::check_for_local_changes(vector<string> files) {
  auto log = logger();
  FileInfoMap currentFiles;
  FileInfoMap changedFiles;

  for (auto i : files) {      // get all current files and their hash
    FileInfo file;
    get<0>(file) = 1;
    list<string> blocks = get_blocks_from_file(i);
    get<1>(file) = set_hashes_from_blocks(blocks); 
    currentFiles.insert(make_pair(i, file));
  }

  FileInfoMap::iterator p;
  // check for which one has changed in comparison to index file
  for (p = currentFiles.begin(); p != currentFiles.end(); p++) {
    FileInfo idxFile = get_local_fileinfo(p->first);

    if(get<0>(idxFile) == -1) { // means the file is not in the local index
      changedFiles.insert(make_pair(p->first, p->second));
      continue;
    }

    // compare hash of index file and the curent file

    if(get<1>(idxFile).size() != get<1>(p->second).size()) {  // file got bigger or smaller
      get<0>(p->second) = get<0>(idxFile);                    // use the version number in the index file
      changedFiles.insert(make_pair(p->first,p->second));    
      continue;
    }

    if(get<1>(idxFile) != get<1>(p->second)) {      // hashes are different
      get<0>(p->second) = get<0>(idxFile);                    // use the version number in the index file
      changedFiles.insert(make_pair(p->first,p->second));
      continue;
    }

  }
  
  return changedFiles;
}

// check local files to see if its in cloud. return fileinfomap of items to download. note: local is all the files that have been changed since last exec
vector<FileInfoMap> SurfStoreClient::find_potential_uploads_and_downloads(FileInfoMap &local, FileInfoMap &remote) {

  vector<FileInfoMap> info;
  FileInfoMap uploads;
  FileInfoMap downloads;

  FileInfoMap::iterator p;

  // compare locals to remote, gets uploads, and downloads of higher versions
  for(p = local.begin(); p != local.end(); p++) {
   
    try {
      FileInfo f = remote.at(p->first);

      if(get<0>(p->second) == get<0>(f))    // check if the version in local is equal to remote. note this is for a pre check on vesions. need to also verify on update since atomic
        uploads.insert(make_pair(p->first, p->second));
      else {
        auto itr = std::find(get<1>(f).begin(), get<1>(f).end(), "0");

        if( itr == (get<1>(f).end()) )
          continue;
        downloads.insert(make_pair(p->first, f)); 
      }

    }
    catch (const std::out_of_range& oor) {  // means the local file is not in the cloud...must delete
      uploads.insert(make_pair(p->first, p->second));
      continue;
    }

  }

  // compare remote to local, find items to download
  for(p = remote.begin() ; p != remote.end(); p++) {

    try {
      FileInfo f = local.at(p->first); 

      auto itr = std::find(get<1>(f).begin(), get<1>(f).end(), "0");

      if( itr == get<1>(f).end() )
        continue;
    }

    catch (const std::out_of_range& oor) {      // if file in remote is not in local, prepare download
      downloads.insert(make_pair(p->first, p->second));
      continue;
    }

  }

  info.push_back(uploads);
  info.push_back(downloads);
  return info;
}

FileInfoMap SurfStoreClient::check_for_deleted_locals(vector<string> &files) {
  
  auto log = logger();
  log->info ("checking for deleted locals");
  ifstream f(base_dir + "/index.txt");
  FileInfoMap deletedFiles; 

  if (f.fail()) {
    int v = -1;
    list<string> blocklist;
    FileInfo ret = make_tuple(v, list<string>());     // initial case, must check in sync for proper care
    return deletedFiles;
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
    if (parts.size() > 0) {
      list<string> hl (parts.begin() + 2, parts.end());
      int v = stoi(parts[1]);
      auto t = make_tuple(v,hl);
      FileInfo fi;     // make the file Info
      get<0>(fi) = v;
      get<1>(fi) = hl;    
      deletedFiles.insert(make_pair(parts[0],fi));
    }
  } while(!f.eof());

  for(auto iter : deletedFiles) {
    string name = iter.first;
    if( std::find(files.begin(), files.end(), name) != files.end() ) // found, erase from deletedFiles map
      deletedFiles.erase(iter.first);

    else   {           // not found, make a record to push into server
      list<string> empty;
      empty.push_back("0");
      get<1>(iter.second) = empty;
    }
  }

  return deletedFiles;
}

