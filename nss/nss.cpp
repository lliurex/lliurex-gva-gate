// SPDX-FileCopyrightText: 2022 Enrique M.G. <quiqueiii@gmail.com>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "libllxgvagate.hpp"
#include "observer.hpp"

#include <nss.h>
#include <grp.h>
#include <pwd.h>

#include <cstdint>
#include <cstdlib>
#include <cstddef>
#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <mutex>
#include <chrono>
#include <functional>

using namespace edupals;
using namespace edupals::variant;

using namespace std;

extern "C" enum nss_status _nss_llxgvagate_setgrent(void);
extern "C" enum nss_status _nss_llxgvagate_endgrent(void);
extern "C" enum nss_status _nss_llxgvagate_getgrent_r(struct group* result, char* buffer, size_t buflen, int* errnop);
extern "C" enum nss_status _nss_llxgvagate_getgrgid_r(gid_t gid, struct group* result, char* buffer, size_t buflen, int* errnop);
extern "C" enum nss_status _nss_llxgvagate_getgrnam_r(const char* name, struct group* result, char *buffer, size_t buflen, int* errnop);

extern "C" enum nss_status _nss_llxgvagate_setpwent(int stayopen);
extern "C" enum nss_status _nss_llxgvagate_endpwent(void);
extern "C" enum nss_status _nss_llxgvagate_getpwent_r(struct passwd* result, char* buffer, size_t buflen, int* errnop);
extern "C" enum nss_status _nss_llxgvagate_getpwuid_r(uid_t uid, struct passwd* result, char* buffer, size_t buflen, int* errnop);
extern "C" enum nss_status _nss_llxgvagate_getpwnam_r(const char* name, struct passwd* result, char* buffer, size_t buflen, int* errnop);

namespace lliurex
{
    struct Group
    {
        std::string name;
        uint64_t gid;
        std::vector<std::string> members;
    };

    struct Passwd
    {
        std::string name;
        uint64_t uid;
        uint64_t gid;

        std::string gecos;

        std::string dir;
        std::string shell;
    };

    std::mutex mtx;
    std::mutex pmtx;

    std::vector<lliurex::Group> groups;
    std::vector<lliurex::Passwd> users;

    int index = -1;
    int pindex = -1;

    bool debug = false;

    lliurex::Observer observer;
}

static void log(int priority,string message)
{
    syslog(priority,"%s",message.c_str());
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

static int push_passwd(lliurex::Passwd& source, struct passwd* result, char* buffer, size_t buflen)
{
    char* ptr = buffer;

    result->pw_uid = source.uid;
    result->pw_gid = source.gid;

    result->pw_name = ptr;

    if (push_string(source.name,&ptr,&buflen) == -1) {
        return -1;
    }

    result->pw_passwd = ptr;

    if (push_string("x",&ptr,&buflen) == -1) {
        return -1;
    }

    result->pw_gecos = ptr;

    if (push_string(source.gecos,&ptr,&buflen) == -1) {
        return -1;
    }

    result->pw_dir = ptr;

    if (push_string(source.dir,&ptr,&buflen) == -1) {
        return -1;
    }

    result->pw_shell = ptr;

    if (push_string(source.shell,&ptr,&buflen) == -1) {
        return -1;
    }

    return 0;
}

int update_db()
{

    lliurex::Gate gate(log);

    if (!gate.exists_db()) {
        // this may happen
        return -1;
    }

    try {

        if (!lliurex::observer.changed()) {
            return 0;
        }

        syslog(LOG_INFO,"loading user database\n");

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

        Variant users = gate.get_users();

        lliurex::users.clear();

        for (size_t n=0;n<users.count();n++) {
            lliurex::Passwd pwd;

            pwd.name = users[n]["name"].get_string();
            pwd.uid = users[n]["uid"].get_int32(); //warning!
            pwd.gid = users[n]["gid"].get_int32(); //warning!
            pwd.dir = users[n]["dir"].get_string();
            pwd.shell = users[n]["shell"].get_string();
            pwd.gecos = users[n]["gecos"].get_string();

            lliurex::users.push_back(pwd);
        }

    }
    catch (...) {
        syslog(LOG_ERR,"Failed to open user database\n");
        return -1;
    }

    return 0;
}

/*!
    Open database
*/
nss_status _nss_llxgvagate_setgrent(void)
{
    //syslog(LOG_INFO,"%s\n",__func__);
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
nss_status _nss_llxgvagate_endgrent(void)
{
    return NSS_STATUS_SUCCESS;
}

/*!
    Read entry
*/
nss_status _nss_llxgvagate_getgrent_r(struct group* result, char* buffer, size_t buflen, int* errnop)
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

nss_status _nss_llxgvagate_getgrgid_r(gid_t gid, struct group* result, char* buffer, size_t buflen, int* errnop)
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

nss_status _nss_llxgvagate_getgrnam_r(const char* name, struct group* result, char *buffer, size_t buflen, int* errnop)
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

enum nss_status _nss_llxgvagate_setpwent(int stayopen)
{
    //syslog(LOG_DEBUG,"%s\n",__func__);
    std::lock_guard<std::mutex> lock(lliurex::pmtx);

    lliurex::pindex = -1;

    int db_status = update_db();
    if (db_status == -1) {
        return NSS_STATUS_UNAVAIL;
    }

    lliurex::pindex = 0;

    return NSS_STATUS_SUCCESS;
}

enum nss_status _nss_llxgvagate_endpwent(void)
{
    return NSS_STATUS_SUCCESS;
}

enum nss_status _nss_llxgvagate_getpwent_r(struct passwd* result, char* buffer, size_t buflen, int* errnop)
{
    //syslog(LOG_INFO,"%s\n",__func__);
    std::lock_guard<std::mutex> lock(lliurex::pmtx);

    if (lliurex::pindex == lliurex::users.size()) {
        return NSS_STATUS_NOTFOUND;
    }

    lliurex::Passwd& pwd = lliurex::users[lliurex::pindex];
    //syslog(LOG_INFO,"* %s\n",pwd.name.c_str());

    int status = push_passwd(pwd,result,buffer,buflen);
    if (status == -1) {
        *errnop = ERANGE;
        return NSS_STATUS_TRYAGAIN;
    }

    lliurex::pindex++;

    return NSS_STATUS_SUCCESS;
}

enum nss_status _nss_llxgvagate_getpwuid_r(uid_t uid, struct passwd* result, char* buffer, size_t buflen, int* errnop)
{
    std::lock_guard<std::mutex> lock(lliurex::pmtx);

    int db_status = update_db();
    if (db_status == -1) {
        return NSS_STATUS_UNAVAIL;
    }

    for (lliurex::Passwd& pwd : lliurex::users) {

        if (pwd.uid == uid) {

            int status = push_passwd(pwd,result,buffer,buflen);
            if (status == -1) {
                *errnop = ERANGE;
                return NSS_STATUS_TRYAGAIN;
            }
            return NSS_STATUS_SUCCESS;
        }
    }

    return NSS_STATUS_NOTFOUND;
}

enum nss_status _nss_llxgvagate_getpwnam_r(const char* name, struct passwd* result, char* buffer, size_t buflen, int* errnop)
{
    std::lock_guard<std::mutex> lock(lliurex::pmtx);

    int db_status = update_db();
    if (db_status == -1) {
        return NSS_STATUS_UNAVAIL;
    }

    for (lliurex::Passwd& pwd : lliurex::users) {

        if (pwd.name.compare(name) == 0) {

            int status = push_passwd(pwd,result,buffer,buflen);
            if (status == -1) {
                *errnop = ERANGE;
                return NSS_STATUS_TRYAGAIN;
            }
            return NSS_STATUS_SUCCESS;
        }
    }

    return NSS_STATUS_NOTFOUND;
}
