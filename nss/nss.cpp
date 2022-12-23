// SPDX-FileCopyrightText: 2022 Enrique M.G. <quiqueiii@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "libllxgvagate.hpp"

#include <nss.h>
#include <grp.h>
#include <systemd/sd-journal.h>

#include <cstddef>
#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <mutex>
#include <chrono>

extern "C" enum nss_status _nss_gvagate_setgrent(void);
extern "C" enum nss_status _nss_gvagate_endgrent(void);
extern "C" enum nss_status _nss_gvagate_getgrent_r(struct group* result, char* buffer, size_t buflen, int* errnop);
extern "C" enum nss_status _nss_gvagate_getgrgid_r(gid_t gid, struct group* result, char* buffer, size_t buflen, int* errnop);
extern "C" enum nss_status _nss_gvagate_getgrnam_r(const char* name, struct group* result, char *buffer, size_t buflen, int* errnop);

namespace lliurex
{
    struct Group
    {
        std::string name;
        uint64_t gid;
        std::vector<std::string> members;
    };

    std::mutex mtx;
    std::vector<lliurex::Group> groups;
    int index = -1;
    std::chrono::time_point<std::chrono::steady_clock> timestamp;

    bool debug = false;
}

static int push_string(string in,char** buffer, size_t* remain)
{
    size_t fsize = in.size() + 1;
    if (fsize > *remain) {
        return -1;
    }

    std::memcpy(*buffer,in.c_str(),fsize);

    *remain -= fsize;
    *buffer += fsize;

    return 0;
}

static int push_group(lliurex::Group& source, struct group* result, char* buffer, size_t buflen)
{
    char* ptr = buffer;

    result->gr_gid = source.gid;

    result->gr_name = ptr;
    result->gr_passwd = 0;

    if (push_string(source.name,&ptr,&buflen) == -1) {
        return -1;
    }

    vector<char*> tmp;

    for (string member : source.members) {
        char* q = ptr;
        if (push_string(member,&ptr,&buflen) == -1) {
            return -1;
        }
        tmp.push_back(q);
    }

    if ( (sizeof(char*)*(tmp.size()+1)) > buflen) {
        return -1;
    }

    result->gr_mem = (char**) ptr;

    int n = 0;

    for (n = 0;n<tmp.size();n++) {
        result->gr_mem[n] = tmp[n];
    }

    result->gr_mem[n] = 0;

    return 0;
}

int update_db()
{
    Gate gate;
    Variant groups = gate.get_groups();

    lliurex::groups.clear();
    for (int n=0;n<groups.count();n++) {
        lliurex::Group grp;

        grp.name = groups[n]["name"].get_string();
        grp.gid = (uint64_t)groups[n]["gid"].to_int64();

        Variant members = groups[n]["members"];

        for (int m=0;m<members.count();m++) {
            grp.members.push_back(members[m].get_string());
        }

        lliurex::groups.push_back(grp);
    }

    return 0;
}

nss_status _nss_gvagate_setgrent(void)
{

}

nss_status _nss_gvagate_endgrent(void)
{

}

nss_status _nss_gvagate_getgrent_r(struct group* result, char* buffer, size_t buflen, int* errnop)
{

}

nss_status _nss_gvagate_getgrgid_r(gid_t gid, struct group* result, char* buffer, size_t buflen, int* errnop)
{

}

nss_status _nss_gvagate_getgrnam_r(const char* name, struct group* result, char *buffer, size_t buflen, int* errnop)
{

}
