// SPDX-FileCopyrightText: 2026 Enrique M.G. <quique@necos.es>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "common.hpp"
#include "filedb.hpp"
#include "libllxgvagate.hpp"

#include <nss.h>
#include <grp.h>
#include <pwd.h>
#include <fcntl.h>

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

    uint32_t start_uid = 90000;
    uint32_t default_gid = 65534;

    std::mutex pmtx;

    int pindex = -1;
}

Variant get_users()
{
    Variant data = Variant::create_array(0);

    lliurex::FileDB db("/tmp/llx-gva-border","LLX-BORDER");

    try {

        if (db.exists()) {
            db.open(true);
            db.lock_read();
            data = db.read();
            db.unlock();
            db.close();
        }
    }
    catch(...) {
        //nothing
        syslog(LOG_DEBUG,"failed to fetch user border cache\n");
    }

    return data;
}

lliurex::Passwd variant_to_passwd(Variant user)
{
    lliurex::Passwd pwd;

    pwd.name = user["name"].get_string();
    pwd.uid = user["uid"].get_int32();
    pwd.gid = user["gid"].get_int32();
    pwd.gecos = user["gecos"].get_string();
    pwd.dir = user["dir"].get_string();
    pwd.shell = user["shell"].get_string();

    return pwd;
}

Variant passwd_to_variant(lliurex::Passwd& pwd)
{

    Variant vpwd = Variant::create_struct();
    vpwd["name"] = pwd.name;
    vpwd["uid"] = (int32_t)pwd.uid;
    vpwd["gid"] = (int32_t)pwd.gid;
    vpwd["gecos"] = "";
    vpwd["dir"] = pwd.dir;
    vpwd["shell"] = pwd.shell;

    return vpwd;
}

enum nss_status _nss_llxgvaborder_setpwent(int stayopen)
{
    std::lock_guard<std::mutex> lock(lliurex::pmtx);
    lliurex::pindex = 0;

    lliurex::users.clear();
    Variant users = get_users();

    for (int n=0;n<users.count();n++) {
        lliurex::users.push_back(variant_to_passwd(users[n]));
    }

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
    Variant users = get_users();

    for (int n=0;n<users.count();n++) {
        if (users[n]["uid"].get_int32() == uid) {
            lliurex::Passwd pwd = variant_to_passwd(users[n]);
            int status = lliurex::push_passwd(pwd,result,buffer,buflen);

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
    Variant users = get_users();
    int32_t max_id = lliurex::start_uid;

    for (int n=0;n<users.count();n++) {
        if (users[n]["name"].get_string().compare(name) == 0) {
            pwd = variant_to_passwd(users[n]);
            found = true;
            syslog(LOG_DEBUG,"found user %s at border cache...\n",name);
        }

        int32_t uid = users[n]["uid"].get_int32();

        if (uid > max_id) {
            max_id = uid;
        }
    }

    //user not found, create it
    if (!found) {
        syslog(LOG_DEBUG,"adding user %s to border cache...\n",name);
        pwd.name = name;
        pwd.uid = max_id + 1;
        pwd.gid = lliurex::default_gid;
        pwd.gecos = "";
        pwd.dir = "/var/run/llx-gva-gate/border/home/" + pwd.name;
        pwd.shell = "/bin/llx-gva-border";


        lliurex::FileDB db("/tmp/llx-gva-border","LLX-BORDER");

        if (!db.exists()) {
            syslog(LOG_DEBUG,"creating border cache\n");
            db.create(lliurex::DBFormat::Bson, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);
            db.open();
            db.lock_write();
            Variant vusers = Variant::create_array(1);
            vusers[0] = passwd_to_variant(pwd);
            db.write(vusers);
            db.unlock();
            db.close();
        }
        else {
            db.open();
            db.lock_write();
            users.append(passwd_to_variant(pwd));
            db.write(users);
            db.unlock();
            db.close();
        }
    }

    int status = lliurex::push_passwd(pwd,result,buffer,buflen);

    if (status == -1) {
        *errnop = ERANGE;
        return NSS_STATUS_TRYAGAIN;
    }

    return NSS_STATUS_SUCCESS;
}
