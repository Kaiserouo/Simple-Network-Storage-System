/*
    Something that is either both used by server and client
*/

#ifndef UTILS_HPP
#define UTILS_HPP

#include "opencv2/opencv.hpp"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <pthread.h>
#include <net/if.h>
#include <unistd.h> 
#include <signal.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <iostream>
#include <string>
#include <memory>
#include <algorithm>
#include <functional>


using namespace std;

#define BUFF_SIZE 1024

// e.g. `put f1 f2 f3` -> vector[`put`, `f1`, `f2`, `f3`]
// basically .split() in python. why doesn't C++ standard have this.
vector<string> split(const string &args, const string &delim = " \n\t") {
    vector<string> tokens;
    int beg = args.find_first_not_of(delim, 0);
    while(beg != string::npos) {
        int end = args.find_first_of(delim, beg);
        tokens.push_back(args.substr(beg, end-beg));
        beg = args.find_first_not_of(delim, end);
    }
    return tokens;
}

// defines command that can be parsed
// client: something typed by prompt
// server: something sent by client
template<typename T>
struct Command {
    string name;

    Command(): name("") {}
    Command(const string &name): name(name) {}
    Command(const Command &cmd): name(cmd.name) {}
    Command(Command &&cmd): name(move(cmd.name)) {};

    bool sameName(const vector<string> &args) {
        return args[0] == name;
    };

    virtual bool argValid(const vector<string> &args) {
        return 1;
    }

    // usually only gives Connection a callback and return
    // since this would take server socket's time
    // Also should checks whether args are OK, and print error if necessary
    virtual void execCommand(const vector<string> &args, T conn) {
        return;
    };
};

#endif