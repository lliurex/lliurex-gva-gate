// SPDX-FileCopyrightText: 2022 Enrique M.G. <quiqueiii@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "libllxgvagate.hpp"
#include "http.hpp"

#include <variant.hpp>
#include <json.hpp>
#include <bson.hpp>

#include <sys/file.h>
#include <unistd.h>

#include <iostream>
#include <fstream>
#include <experimental/filesystem>
#include <stdexcept>
#include <sstream>
#include <chrono>
#include <thread>

using namespace lliurex;
using namespace edupals;
using namespace edupals::variant;

using namespace std;
namespace stdfs=std::experimental::filesystem;

Gate::Gate() : Gate(nullptr)
{
}

Gate::Gate(function<void(int priority,string message)> cb) : log_cb(cb), dbase(nullptr)
{
    log(LOG_DEBUG,"Gate constructor\n");

    if (!exists_db()) {
        log(LOG_DEBUG,"Database does not exists, creating an empty one...\n");
        dbase = fopen(LLX_GVA_GATE_DB,"wb");
        fclose(dbase);
    }

    dbase = fopen(LLX_GVA_GATE_DB,"r+");
}

Gate::~Gate()
{
    log(LOG_DEBUG,"Gate destructor\n");

    fclose(dbase);
}

bool Gate::exists_db()
{
    const stdfs::path dbpath {LLX_GVA_GATE_DB};

    return stdfs::exists(dbpath);
}

Variant Gate::read_db()
{
    lock_db_read();

    fseek(dbase,0,SEEK_SET);

    stringstream ss;
    char buffer[128];
    size_t len;

    L0:
    len = fread(buffer,1,128,dbase);

    if (len > 0) {
        ss.write((const char*)buffer,len);
        goto L0;
    }

    unlock_db();

    Variant value = json::load(ss);

    return value;
}

void Gate::write_db(Variant data)
{
    stringstream ss;
    json::dump(data,ss);

    lock_db_write();

    fseek(dbase,0,SEEK_SET);
    int fd = fileno(dbase);
    ftruncate(fd,0);

    int status = fwrite(ss.str().c_str(),ss.str().size(),1,dbase);

    if (status == 0) {
        //TODO: raise exception here?
        log(LOG_ERR,"failed to write on DB\n");
    }

    unlock_db();
}

void Gate::create_db()
{
    log(LOG_DEBUG,"Creating an empty database\n");
    Variant database = Variant::create_struct();

    database["magic"] = LLX_GVA_GATE_MAGIC;
    database["machine-token"] = "";

    database["users"] = Variant::create_array(0);

    write_db(database);
}

string Gate::machine_token()
{
    Variant database = read_db();

    if (!validate(database,Validator::Database)) {
        log(LOG_ERR,"Bad database\n");
        return "";
    }

    return database["machine-token"].get_string();
}

void Gate::update_db(Variant data)
{
    Variant database = read_db();

    if (!validate(database,Validator::Database)) {
        log(LOG_ERR,"Bad database\n");
        return;
    }

    string login = data["user"]["login"].get_string();
    int32_t uid = data["user"]["uid"].get_int32();

    Variant tmp = Variant::create_array(0);

    for (size_t n=0;n<database["users"].count();n++) {
        Variant user = database["users"][n];

        if (user["login"].get_string() != login and user["uid"].get_int32() != uid) {
            tmp.append(user);
        }
    }

    tmp.append(data["user"]);

    database["users"] = tmp;

    write_db(database);
}

void Gate::lock_db_read()
{
    int fd = fileno(dbase);

    int status = flock(fd,LOCK_SH);

    if (status!=0) {
        stringstream ss;
        ss<<" Failed to aquire read lock:"<<errno;
        throw runtime_error(ss.str());
    }
}

void Gate::lock_db_write()
{
    int fd = fileno(dbase);

    int status = flock(fd,LOCK_EX);

    if (status!=0) {
        stringstream ss;
        ss<<" Failed to aquire write lock:"<<errno;
        throw runtime_error(ss.str());
    }
}

void Gate::unlock_db()
{
    int fd = fileno(dbase);

    int status = flock(fd,LOCK_UN);

    if (status!=0) {
        stringstream ss;
        ss<<" Failed to release lock:"<<errno;
        throw runtime_error(ss.str());
    }
}

static Variant find_group(Variant groups, string name, int32_t gid)
{
    Variant ret;

    for (size_t n=0;n<groups.count();n++) {
        Variant group = groups[n];

        if (group["name"].get_string() == name and group["gid"].get_int32()) {
            return group;
        }
    }

    return ret;
}

static void append_member(Variant group,string name)
{
    bool found = false;
    for (size_t n=0;n<group.count();n++) {
        if (group[n].get_string() == name) {
            found = true;
            break;
        }
    }

    if (!found) {
        group.append(name);
    }
}

Variant Gate::get_groups()
{
    Variant groups;
    Variant database = read_db();

    if (!validate(database,Validator::Database)) {
        log(LOG_ERR,"Bad database\n");
        return groups;
    }

    groups = Variant::create_array(0);

    for (size_t n=0;n<database["users"].count();n++) {
        Variant user = database["users"][n];

        Variant main_group = user["gid"];
        string mgname = user["gid"].keys()[0];
        int32_t mgid = user["gid"][mgname];
        main_group = find_group(groups,mgname,mgid);

        if (main_group.none()) {
            main_group = Variant::create_struct();
            main_group["name"] = mgname;
            main_group["gid"] = mgid;
            main_group["members"] = Variant::create_array(0);
            groups.append(main_group);
        }

        for (string& gname : user["groups"].keys()) {
            int32_t gid = user["groups"][gname];

            Variant group = find_group(groups,gname,gid);

            if (group.none()) {
                group = Variant::create_struct();
                group["name"] = gname;
                group["gid"] = gid;
                group["members"] = Variant::create_array(0);
                append_member(group["members"],user["login"].get_string());
                groups.append(group);
            }
            else {
                append_member(group["members"],user["login"].get_string());
            }
        }
    }

    return groups;
}

Variant Gate::get_users()
{
    Variant users;
    Variant database = read_db();

    if (!validate(database,Validator::Database)) {
        log(LOG_ERR,"Bad database\n");
        return users;
    }

    users = Variant::create_array(0);

    for (size_t n=0;n<database["users"].count();n++) {
        Variant user = database["users"][n];

        Variant ent = Variant::create_struct();
        ent["name"] = user["login"];
        ent["uid"] = user["uid"];
        string mgname = user["gid"].keys()[0];
        ent["gid"] = user["gid"][mgname];
        ent["dir"] = user["home"];
        ent["shell"] = user["shell"];
        string gecos = user["surname"].get_string() + "," +user["name"].get_string();
        ent["gecos"] = gecos;

        users.append(ent);
    }

    return users;
}

void Gate::set_logger(function<void(int priority,string message)> cb)
{
    this->log_cb = cb;
}

bool Gate::authenticate(string user,string password)
{
    http::Client client("http://127.0.0.1:5000");

    http::Response response;

    response = client.post("authenticate",{ {"user",user},{"passwd",password}});

    if (response.status==200) {
        Variant data = response.parse();
        //clog<<data<<endl;

        if (!validate(data,Validator::Authenticate)) {
            log(LOG_ERR,"Bad Authenticate response\n");
            return false;
        }

        update_db(data);
    }

    return true;
}

void Gate::log(int priority, string message)
{
    if (log_cb) {
        log_cb(priority,message);
    }
}

bool Gate::validate(Variant data,Validator validator)
{
    switch (validator) {

        case Validator::Groups:
            if (!data.is_struct()) {
                return false;
            }

            for (string& key : data.keys()) {
                if (!data[key].is_int32()) {
                    return false;
                }
            }

            return true;

        break;

        case Validator::Database:
            if (!data.is_struct()) {
                return false;
            }

            if (!data["magic"].is_string()) {
                return false;
            }

            if (data["magic"].get_string()!=LLX_GVA_GATE_MAGIC) {
                return false;
            }

            if (!data["machine-token"].is_string()) {
                return false;
            }

            return validate(data["users"],Validator::Users);
        break;

        case Validator::Users:
            if (!data.is_array()) {
                return false;
            }

            for (size_t n=0;n<data.count();n++) {
                return validate(data[n],Validator::User);
            }

            return true;
        break;

        case Validator::User:
            if (!data["login"].is_string()) {
                return false;
            }

            if (!data["uid"].is_int32()) {
                return false;
            }

            if (!validate(data["gid"],Validator::Groups)) {
                return false;
            }

            if (!data["name"].is_string()) {
                return false;
            }

            if (!data["surname"].is_string()) {
                return false;
            }

            if (!data["home"].is_string()) {
                return false;
            }

            if (!data["shell"].is_string()) {
                return false;
            }

            return validate(data["groups"],Validator::Groups);
        break;

        case Validator::Authenticate:
            if (!data.is_struct()) {
                return false;
            }

            if (!data["machine-token"].is_string()) {
                return false;
            }

            return validate(data["user"],Validator::User);
        break;

        default:
            return false;
    }
}

void Gate::test_read()
{
    clog<<"reading...";
    lock_db_read();
    clog<<"locked...";
    std::this_thread::sleep_for(std::chrono::milliseconds(4000));
    clog<<"read...";
    unlock_db();
    clog<<"done"<<endl;
}

void Gate::test_write()
{
    clog<<"writting...";
    lock_db_write();
    clog<<"locked...";
    std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    clog<<"write...";
    unlock_db();
    clog<<"done"<<endl;
}
