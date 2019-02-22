#ifndef SURFSTORESERVER_HPP
#define SURFSTORESERVER_HPP

#include "inih/INIReader.h"
#include "SurfStoreTypes.hpp"
#include "logger.hpp"

using namespace std;

class SurfStoreServer {
public:
    SurfStoreServer(INIReader& t_config);

    void launch();

    const int NUM_THREADS = 8;
    unordered_map<string, string> hashBlocks; 
    FileInfoMap remoteMap;

protected:
    INIReader& config;
    int port;
    int blockSize;
};

#endif // SURFSTORESERVER_HPP
