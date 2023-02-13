// SPDX-FileCopyrightText: 2022 Enrique M.G. <quiqueiii@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "libllxgvagate.hpp"

#include <cstdint>
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

using namespace edupals;
using namespace edupals::variant;

using namespace std;

extern "C" enum nss_status _nss_gvagate_setgrent(void);
extern "C" enum nss_status _nss_gvagate_endgrent(void);
extern "C" enum nss_status _nss_gvagate_getgrent_r(struct group* result, char* buffer, size_t buflen, int* errnop);
extern "C" enum nss_status _nss_gvagate_getgrgid_r(gid_t gid, struct group* result, char* buffer, size_t buflen, int* errnop);
extern "C" enum nss_status _nss_gvagate_getgrnam_r(const char* name, struct group* result, char *buffer, size_t buflen, int* errnop);

extern "C" enum nss_status _nss_gvagate_setpwent(int stayopen);
extern "C" enum nss_status _nss_gvagate_endpwent(void);
extern "C" enum nss_status _nss_gvagate_getpwent_r(struct passwd* result, char* buffer, size_t buflen, int* errnop);
extern "C" enum nss_status _nss_gvagate_getpwuid_r(uid_t uid, struct passwd* result, char* buffer, size_t buflen, int* errnop);
extern "C" enum nss_status _nss_gvagate_getpwnam_r(const char* name, struct passwd* result, char* buffer, size_t buflen, int* errnop);

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

    struct Passwd
    {
        std::string name;
        uint64_t uid;
        uint64_t gid;

        std::string gecos;

        std::string dir;
        std::string shell;
    };

    template<class T>
    class NSSContext
    {
        public:

            NSSContext() : index(-1)
            {
            }

            ~NSSContext()
            {
            }

            void reset()
            {
                index = -1;
            }

            void next()
            {
                index++;
            }

            bool end() const
            {
                return (index == entries.size());
            }

            T& current() const
            {
                return entries[index];
            }

            std::mutex& mutex()
            {
                return mtx;
            }

        protected:

        std::mutex mtx;
        std::vector<T> entries;
        int index = -1;
        std::chrono::time_point<std::chrono::steady_clock> timestamp;
    };

    NSSContext<Passwd> ctx_passwd;
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

    if (push_string(source.name,&ptr,&buflen) == -1) {
        return -1;
    }

    result->gr_passwd = ptr;

    if (push_string("x",&ptr,&buflen) == -1) {
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
    lliurex::Gate gate;
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

/*!
    Open database
*/
nss_status _nss_gvagate_setgrent(void)
{
    std::lock_guard<std::mutex> lock(lliurex::mtx);

    lliurex::index = -1;

    int db_status = update_db();
    if (db_status == -1) {
        return NSS_STATUS_UNAVAIL;
    }

    lliurex::index = 0;

    return NSS_STATUS_SUCCESS;
}

/*!
    End database
*/
nss_status _nss_gvagate_endgrent(void)
{
    return NSS_STATUS_SUCCESS;
}

/*!
    Read entry
*/
nss_status _nss_gvagate_getgrent_r(struct group* result, char* buffer, size_t buflen, int* errnop)
{
    std::lock_guard<std::mutex> lock(lliurex::mtx);

    if (lliurex::index == lliurex::groups.size()) {
        return NSS_STATUS_NOTFOUND;
    }

    lliurex::Group& grp = lliurex::groups[lliurex::index];

    int status = push_group(grp,result,buffer,buflen);
    if (status == -1) {
        *errnop = ERANGE;
        return NSS_STATUS_TRYAGAIN;
    }

    lliurex::index++;
    return NSS_STATUS_SUCCESS;
}

nss_status _nss_gvagate_getgrgid_r(gid_t gid, struct group* result, char* buffer, size_t buflen, int* errnop)
{
    std::lock_guard<std::mutex> lock(lliurex::mtx);

    int db_status = update_db();
    if (db_status == -1) {
        *errnop = ENOENT;
        return NSS_STATUS_UNAVAIL;
    }

    for (lliurex::Group& grp : lliurex::groups) {
        if (grp.gid==gid) {

            int status = push_group(grp,result,buffer,buflen);
            if (status == -1) {
                *errnop = ERANGE;
                return NSS_STATUS_TRYAGAIN;
            }

            return NSS_STATUS_SUCCESS;
        }
    }

    // not found
    *errnop = ENOENT;
    return NSS_STATUS_NOTFOUND;
}

nss_status _nss_gvagate_getgrnam_r(const char* name, struct group* result, char *buffer, size_t buflen, int* errnop)
{
    std::lock_guard<std::mutex> lock(lliurex::mtx);

    int db_status = update_db();
    if (db_status == -1) {
        *errnop = ENOENT;
        return NSS_STATUS_UNAVAIL;
    }

    for (lliurex::Group& grp : lliurex::groups) {
        if (grp.name==std::string(name)) {

            int status = push_group(grp,result,buffer,buflen);
            if (status == -1) {
                *errnop = ERANGE;
                return NSS_STATUS_TRYAGAIN;
            }

            return NSS_STATUS_SUCCESS;
        }
    }

    // not found
    *errnop = ENOENT;
    return NSS_STATUS_NOTFOUND;
}

enum nss_status _nss_gvagate_setpwent(int stayopen)
{
    return NSS_STATUS_SUCCESS;
}

enum nss_status _nss_gvagate_endpwent(void)
{
    return NSS_STATUS_SUCCESS;
}

enum nss_status _nss_gvagate_getpwent_r(struct passwd* result, char* buffer, size_t buflen, int* errnop)
{
    return NSS_STATUS_SUCCESS;
}

enum nss_status _nss_gvagate_getpwuid_r(uid_t uid, struct passwd* result, char* buffer, size_t buflen, int* errnop)
{
    return NSS_STATUS_SUCCESS;
}

enum nss_status _nss_gvagate_getpwnam_r(const char* name, struct passwd* result, char* buffer, size_t buflen, int* errnop)
{
    return NSS_STATUS_SUCCESS;
}
