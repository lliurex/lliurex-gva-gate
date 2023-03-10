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
#include <ctime>

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

bool Gate::open()
{
    //TODO: think a strategy
    bool user = false;
    bool token = false;
    bool shadow = false;

    if (!userdb.exists()) {
        log(LOG_ERR,"User database does not exists\n");
    }
    else {
        if (!userdb.is_open()) {
            user = userdb.open();
        }
    }

    if (!tokendb.exists()) {
        log(LOG_ERR,"Token database does not exists\n");
    }
    else {
        if (!tokendb.is_open()) {
            token = tokendb.open();
        }
    }

    if (!shadowdb.exists()) {
        log(LOG_ERR,"Shadow database does not exists\n");
    }
    else {
        if (!shadowdb.is_open()) {
            shadow = shadowdb.open();
        }
    }

    return user and token; //we don't care about shadow right now
}

void Gate::create_db()
{
    //TODO: handle exceptions
    log(LOG_DEBUG,"Creating databases...\n");

    // checking db dir first
    const stdfs::path dbdir {LLX_GVA_GATE_DB_PATH};
    stdfs::create_directories(dbdir);

    // user db
    if (!userdb.exists()) {
        log(LOG_DEBUG,"Creating user database\n");
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
        log(LOG_DEBUG,"Creating token database\n");
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
        log(LOG_DEBUG,"Creating shadow database\n");
        shadowdb.create(DBFormat::Json,S_IRUSR | S_IRGRP | S_IWUSR);

        shadowdb.open();

        shadowdb.lock_write();
        Variant shadow_data = Variant::create_struct();
        shadow_data["passwords"] = Variant::create_array(0);
        shadowdb.write(shadow_data);
        shadowdb.unlock();
    }

}

string Gate::machine_token()
{
    AutoLock lock(LockMode::Read,&tokendb);

    Variant data = tokendb.read();

    if (!validate(data,Validator::TokenDatabase)) {
        log(LOG_ERR,"Bad token database\n");
        throw exception::GateError("Bad token database\n",0);
    }

    return data["machine-token"].get_string();
}

void Gate::update_db(Variant data)
{

    AutoLock user_lock(LockMode::Write,&userdb);
    AutoLock token_lock(LockMode::Write,&tokendb);

    std::this_thread::sleep_for(std::chrono::milliseconds(4000));
    Variant token_data = Variant::create_struct();
    token_data["machine-token"] = data["machine-token"];
    tokendb.write(token_data);

    Variant user_data = userdb.read();
    if (!validate(user_data,Validator::UserDatabase)) {
        log(LOG_ERR,"Bad user database\n");
        throw exception::GateError("Bad user database\n",0);
    }

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

void Gate::update_shadow_db(string name,string password)
{
    AutoLock shadow_lock(LockMode::Write,&shadowdb);

    Variant database = shadowdb.read();

    bool found = false;

    for (size_t n=0;n<database["passwords"].count();n++) {
        Variant shadow = database["passwords"][n];

        if (shadow["name"].get_string() == name) {
            shadow["key"] = hash(name,password);
            shadow["expire"] = 60 + (int32_t)std::time(nullptr);
            found = true;
            break;
        }
    }

    if (!found) {
        Variant shadow = Variant::create_struct();
        shadow["name"] = name;
        shadow["key"] = hash(name,password);
        shadow["expire"] = 60 + (int32_t)std::time(nullptr);
        database["passwords"].append(shadow);
    }

    shadowdb.write(database);

}

int Gate::lookup_password(string user,string password)
{
    AutoLock shadow_lock(LockMode::Read,&shadowdb);
    int status = 0;
    Variant database = shadowdb.read();

    //Validate here

    for (size_t n=0;n<database["passwords"].count();n++) {
        Variant shadow = database["passwords"][n];

        if (shadow["name"].get_string() == user) {
            //TODO: improve this!
            string stored_hash = shadow["hash"].get_string();
            string computed_hash = hash(user,password);

            if (stored_hash == computed_hash) {
                status = 1;
                break;
            }
        }
    }

    return status;
}

Variant Gate::get_groups()
{
    Variant groups;

    AutoLock lock(LockMode::Read,&userdb);

    Variant database = userdb.read();

    if (!validate(database,Validator::UserDatabase)) {
        log(LOG_ERR,"Bad user database\n");
        throw exception::GateError("Bad user database\n",0);
    }

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

    Variant user_data = userdb.read();
    if (!validate(database,Validator::UserDatabase)) {
        log(LOG_ERR,"Bad user database\n");
        throw exception::GateError("Bad user database\n",0);
    }

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

    try {
        response = client.post("authenticate",{ {"user",user},{"passwd",password}});
    }
    catch(std::exception& e) {
        log(LOG_ERR,"Post error:" + string(e.what()));
        throw exception::GateError("Post error:" + string(e.what()),3);
    }

    if (response.status==200) {
        Variant data;
        try {
            data = response.parse();
        }
        catch(...) {
            log(LOG_ERR,"Failed to parse server repsonse\n");
            throw exception::GateError("Failed to parse server response",2);
        }

        if (!validate(data,Validator::Authenticate)) {
            log(LOG_ERR,"Bad Authenticate response\n");
            throw exception::GateError("Bad server response",1);
        }

        update_db(data);
        update_shadow_db(user,password);
    }
    else {
        log(LOG_WARNING,"Server returned status: " + std::to_string(response.status) + "\n");
        return false;
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

        case Validator::UserDatabase:
            if (!data.is_struct()) {
                return false;
            }

            return validate(data["users"],Validator::Users);
        break;

        case Validator::TokenDatabase:
            if (!data.is_struct()) {
                return false;
            }

            if (!data["machine-token"].is_string()) {
                return false;
            }

            return true;
        break;

        case Validator::ShadowDatabase:
            if (!data.is_struct()) {
                return false;
            }

            return validate(data["passwords"],Validator::Shadows);
        break;

        case Validator::Shadows:
            if (!data.is_array()) {
                return false;
            }

            for (size_t n=0;n<data.count();n++) {
                if (!validate(data[n],Validator::Shadow)) {
                    return false;
                }
            }

            return true;
        break;

        case Validator::Shadow:
            if (!data.is_struct()) {
                return false;
            }

            if (!data["name"].is_string()) {
                return false;
            }

            if (!data["key"].is_string()) {
                return false;
            }

            if (!data["expire"].is_int32()) {
                return false;
            }

            return true;
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

string Gate::hash(string username,string password)
{
    char* data = crypt(password.c_str(),"$6$llxgvagate$"); //TODO: improve salt

    return string(data);
}
