// SPDX-FileCopyrightText: 2026 Enrique M.G. <quique@necos.es>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "common.hpp"
#include "libllxgvagate.hpp"

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

extern "C" enum nss_status _nss_llxgvaborder_setpwent(int stayopen);
extern "C" enum nss_status _nss_llxgvaborder_endpwent(void);
extern "C" enum nss_status _nss_llxgvaborder_getpwent_r(struct passwd* result, char* buffer, size_t buflen, int* errnop);
extern "C" enum nss_status _nss_llxgvaborder_getpwuid_r(uid_t uid, struct passwd* result, char* buffer, size_t buflen, int* errnop);
extern "C" enum nss_status _nss_llxgvaborder_getpwnam_r(const char* name, struct passwd* result, char* buffer, size_t buflen, int* errnop);

namespace lliurex
{

    std::vector<lliurex::Passwd> users;

    uint32_t start_uid = 70000;
    uint32_t default_gid = 65534;

    std::mutex pmtx;

    int pindex = -1;
}

/**
 * 16-bit FNV-1a
 */
uint16_t hash16(const std::string& input) {

    const uint16_t FNV_OFFSET_BASIS = 0x01000193;
    const uint16_t FNV_PRIME = 0x010001b3;

    uint16_t hash = FNV_OFFSET_BASIS;

    for (char c : input) {

        hash ^= (uint16_t)c;
        hash *= FNV_PRIME;
    }

    return hash;
}


enum nss_status _nss_llxgvaborder_setpwent(int stayopen)
{
    std::lock_guard<std::mutex> lock(lliurex::pmtx);
    lliurex::pindex = 0;

    return NSS_STATUS_SUCCESS;
}

enum nss_status _nss_llxgvaborder_endpwent(void)
{
    return NSS_STATUS_SUCCESS;
}

enum nss_status _nss_llxgvaborder_getpwent_r(struct passwd* result, char* buffer, size_t buflen, int* errnop)
{
    std::lock_guard<std::mutex> lock(lliurex::pmtx);

    if (lliurex::pindex == lliurex::users.size()) {
        return NSS_STATUS_NOTFOUND;
    }

    lliurex::Passwd& pwd = lliurex::users[lliurex::pindex];

    int status = lliurex::push_passwd(pwd,result,buffer,buflen);

    if (status == -1) {
        *errnop = ERANGE;
        return NSS_STATUS_TRYAGAIN;
    }

    lliurex::pindex++;

    return NSS_STATUS_SUCCESS;
}

enum nss_status _nss_llxgvaborder_getpwuid_r(uid_t uid, struct passwd* result, char* buffer, size_t buflen, int* errnop)
{
    std::lock_guard<std::mutex> lock(lliurex::pmtx);

    for (lliurex::Passwd& user : lliurex::users) {
        if (user.uid == uid) {
            int status = lliurex::push_passwd(user,result,buffer,buflen);

            if (status == -1) {
                *errnop = ERANGE;
                return NSS_STATUS_TRYAGAIN;
            }

            return NSS_STATUS_SUCCESS;
        }
    }

    return NSS_STATUS_NOTFOUND;
}

enum nss_status _nss_llxgvaborder_getpwnam_r(const char* name, struct passwd* result, char* buffer, size_t buflen, int* errnop)
{
    std::lock_guard<std::mutex> lock(lliurex::pmtx);

    bool found = false;
    lliurex::Passwd pwd;

    for (lliurex::Passwd& user : lliurex::users) {
        if (user.name.compare(name) == 0) {
            //user found, return it
            pwd = user;
            found = true;
            break;
        }
    }

    //user not found, create it
    if (!found) {
        pwd.name = name;
        pwd.uid = lliurex::start_uid + hash16(pwd.name);
        pwd.gid = lliurex::default_gid;
        pwd.gecos = "";
        pwd.dir = "/var/run/llx-gva-gate/border/home/" + pwd.name;
        pwd.shell = "/bin/bash";
    }

    int status = lliurex::push_passwd(pwd,result,buffer,buflen);

    if (status == -1) {
        *errnop = ERANGE;
        return NSS_STATUS_TRYAGAIN;
    }

    return NSS_STATUS_SUCCESS;
}
