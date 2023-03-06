// SPDX-FileCopyrightText: 2022 Enrique M.G. <quiqueiii@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "libllxgvagate.hpp"
#include "filedb.hpp"
#include "http.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/syslog.h>
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

Gate::Gate(function<void(int priority,string message)> cb) : log_cb(cb)
{
    log(LOG_DEBUG,"Gate constructor\n");

    userdb = FileDB(LLX_GVA_GATE_USER_DB_PATH,LLX_GVA_GATE_USER_DB_MAGIC);
    tokendb = FileDB(LLX_GVA_GATE_TOKEN_DB_PATH,LLX_GVA_GATE_TOKEN_DB_MAGIC);
    shadowdb = FileDB(LLX_GVA_GATE_SHADOW_DB_PATH,LLX_GVA_GATE_SHADOW_DB_MAGIC);

}

Gate::~Gate()
{
    log(LOG_DEBUG,"Gate destructor\n");
}

bool Gate::exists_db()
{
    bool status = userdb.exists() and tokendb.exists() and shadowdb.exists();

    return status;
}

void Gate::open()
{

    if (!userdb.exists()) {
        log(LOG_ERR,"User database does not exists\n");
    }
    else {
        if (!userdb.is_open()) {
            userdb.open();
        }
    }

    if (!tokendb.exists()) {
        log(LOG_ERR,"Token database does not exists\n");
    }
    else {
        if (!tokendb.is_open()) {
            tokendb.open();
        }
    }

    if (!shadowdb.exists()) {
        log(LOG_ERR,"Shadow database does not exists\n");
    }
    else {
        if (!shadowdb.is_open()) {
            shadowdb.open();
        }
    }

}

void Gate::create_db()
{
    log(LOG_DEBUG,"Creating an empty database\n");

    // checking db dir first
    const stdfs::path dbdir {LLX_GVA_GATE_DB_PATH};
    stdfs::create_directories(dbdir);


    // user db
    if (!userdb.exists()) {
        userdb.create(DBFormat::Json,S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);

        userdb.open();

        userdb.lock_write();
        Variant user_data = Variant::create_struct();
        user_data["users"] = Variant::create_array(0);
        userdb.write(user_data);
        userdb.unlock();
    }

    // token db
    if (!tokendb.exists()) {
        tokendb.create(DBFormat::Json,S_IRUSR | S_IRGRP | S_IWUSR);

        tokendb.open();

        tokendb.lock_write();
        Variant token_data = Variant::create_struct();
        token_data["machine-token"] = "";
        tokendb.write(token_data);
        tokendb.unlock();
    }

    // shadow db
    if (!shadowdb.exists()) {
        shadowdb.create(DBFormat::Json,S_IRUSR | S_IRGRP | S_IWUSR);
    }

}

string Gate::machine_token()
{
    AutoLock lock(LockMode::Read,&tokendb);

    Variant data = tokendb.read();

    //TODO: validate here

    return data["machine-token"].get_string();
}

void Gate::update_db(Variant data)
{

    AutoLock user_lock(LockMode::Write,&userdb);
    AutoLock token_lock(LockMode::Write,&tokendb);

    Variant token_data = Variant::create_struct();
    token_data["machine-token"] = data["machine-token"];
    tokendb.write(token_data);

    Variant user_data = userdb.read();
    //TODO: validate

    string login = data["user"]["login"].get_string();
    int32_t uid = data["user"]["uid"].get_int32();

    Variant tmp = Variant::create_array(0);

    for (size_t n=0;n<user_data["users"].count();n++) {
        Variant user = user_data["users"][n];

        if (user["login"].get_string() != login and user["uid"].get_int32() != uid) {
            tmp.append(user);
        }
    }

    tmp.append(data["user"]);

    user_data["users"] = tmp;

    userdb.write(user_data);

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

    AutoLock lock(LockMode::Read,&userdb);

    Variant database = userdb.read();

/*
    if (!validate(database,Validator::Database)) {
        log(LOG_ERR,"Bad database\n");
        return groups;
    }
*/
    groups = Variant::create_array(0);

    for (size_t n=0;n<database["users"].count();n++) {
        Variant user = database["users"][n];

        Variant main_group = user["gid"];
        string mgname = user["gid"]["name"].get_string();
        int32_t mgid = user["gid"]["gid"].get_int32();
        main_group = find_group(groups,mgname,mgid);

        if (main_group.none()) {
            main_group = Variant::create_struct();
            main_group["name"] = mgname;
            main_group["gid"] = mgid;
            main_group["members"] = Variant::create_array(0);
            groups.append(main_group);
        }

        for (size_t n=0;n<user["groups"].count();n++) {
            string gname = user["groups"][n]["name"].get_string();
            int32_t gid = user["groups"][n]["gid"].get_int32();

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

    AutoLock lock(LockMode::Read,&userdb);
    Variant database = userdb.read();

/*
    if (!validate(database,Validator::Database)) {
        log(LOG_ERR,"Bad database\n");
        return users;
    }
*/
    users = Variant::create_array(0);

    for (size_t n=0;n<database["users"].count();n++) {
        Variant user = database["users"][n];

        Variant ent = Variant::create_struct();
        ent["name"] = user["login"];
        ent["uid"] = user["uid"];
        ent["gid"] = user["gid"]["gid"];
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
    else {
        log(LOG_WARNING,"Server returned status: " + std::to_string(response.status) + "\n");
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
            if (!data.is_array()) {
                return false;
            }

            for (size_t n=0;n<data.count();n++) {
                if (!validate(data[n],Validator::Group)) {
                    return false;
                }
            }

            return true;

        break;

        case Validator::Group:
            if (!data.is_struct()) {
                return false;
            }

            if (!data["name"].is_string()) {
                return false;
            }

            if (!data["gid"].is_int32()) {
                return false;
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

            if (!validate(data["gid"],Validator::Group)) {
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
    /*
    clog<<"reading...";
    lock_db_read();
    clog<<"locked...";
    std::this_thread::sleep_for(std::chrono::milliseconds(4000));
    clog<<"read...";
    unlock_db();
    clog<<"done"<<endl;
    */
}

void Gate::test_write()
{
    /*
    clog<<"writting...";
    lock_db_write();
    clog<<"locked...";
    std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    clog<<"write...";
    unlock_db();
    clog<<"done"<<endl;
    */
}
