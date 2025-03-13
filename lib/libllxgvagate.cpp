// SPDX-FileCopyrightText: 2022 Enrique M.G. <quiqueiii@gmail.com>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "libllxgvagate.hpp"
#include "filedb.hpp"
#include "http.hpp"
#include "exec.hpp"

#include <variant.hpp>
#include <json.hpp>
#include <bson.hpp>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/syslog.h>
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

Gate::Gate(function<void(int priority,string message)> cb) : log_cb(cb),
    server("http://127.0.0.1:5000"),
    auth_mode(Gate::Default),
    auth_methods({AuthMethod::Local})
{
    //log(LOG_DEBUG,"Gate with effective uid:"+std::to_string(geteuid()));
    //load_config();

    userdb = FileDB(LLX_GVA_GATE_USER_DB_PATH,LLX_GVA_GATE_USER_DB_MAGIC);
    shadowdb = FileDB(LLX_GVA_GATE_SHADOW_DB_PATH,LLX_GVA_GATE_SHADOW_DB_MAGIC);

}

Gate::~Gate()
{
    //log(LOG_DEBUG,"Gate destructor\n");
}

bool Gate::exists_db()
{
    bool status = userdb.exists() and shadowdb.exists();

    return status;
}

bool Gate::open(bool noroot)
{
    //TODO: think a strategy
    bool user = false;
    bool shadow = false;

    if (!userdb.exists()) {
        log(LOG_ERR,"User database does not exists\n");
    }
    else {
        if (!userdb.is_open()) {
            user = userdb.open(noroot);
        }
    }

    if (!shadowdb.exists()) {
        log(LOG_ERR,"Shadow database does not exists\n");
    }
    else {
        if (!shadowdb.is_open()) {
            shadow = shadowdb.open(noroot);
        }
    }

    if (noroot) {
        return user;
    }
    else {
        return user and shadow;
    }
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
        userdb.create(DBFormat::Bson,S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);

        userdb.open();

        userdb.lock_write();
        Variant user_data = Variant::create_struct();
        user_data["users"] = Variant::create_array(0);
        userdb.write(user_data);
        userdb.unlock();
    }

    // shadow db
    if (!shadowdb.exists()) {
        log(LOG_DEBUG,"Creating shadow database\n");
        shadowdb.create(DBFormat::Bson,S_IRUSR | S_IRGRP | S_IWUSR);

        shadowdb.open();

        shadowdb.lock_write();
        Variant shadow_data = Variant::create_struct();
        shadow_data["passwords"] = Variant::create_array(0);
        shadowdb.write(shadow_data);
        shadowdb.unlock();
    }

}

void Gate::update_db(Variant data)
{

    AutoLock user_lock(LockMode::Write,&userdb);

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
            shadow["key"] = hash(password,salt(name));
            shadow["expire"] = 60 + (int32_t)std::time(nullptr);
            found = true;
            break;
        }
    }

    if (!found) {
        Variant shadow = Variant::create_struct();
        shadow["name"] = name;
        shadow["key"] = hash(password,salt(name));
        shadow["expire"] = (10*60) + (int32_t)std::time(nullptr);
        database["passwords"].append(shadow);
    }

    shadowdb.write(database);

}

void Gate::purge_shadow_db()
{
    AutoLock shadow_lock(LockMode::Write,&shadowdb);

    Variant database = Variant::create_struct();
    database["passwords"] = Variant::create_array(0);

    shadowdb.write(database);
}

static string extract_salt(string key)
{
    vector<int> dollar;

    for (size_t n=0;n<key.size();n++) {
        if (key[n] == '$') {
            dollar.push_back(n);
        }
    }

    if (dollar.size() > 2) {
        return key.substr(dollar[1]+1,dollar[2]-dollar[1]-1);
    }

    return "";
}

int Gate::lookup_password(string user,string password)
{
    AutoLock shadow_lock(LockMode::Read,&shadowdb);
    int status = Gate::UserNotFound;
    Variant database = shadowdb.read();

    //Validate here

    for (size_t n=0;n<database["passwords"].count();n++) {
        Variant shadow = database["passwords"][n];

        if (shadow["name"].get_string() == user) {
            string stored_hash = shadow["key"].get_string();
            string stored_salt = extract_salt(stored_hash);
            string computed_hash = hash(password,stored_salt);

            if (stored_hash == computed_hash) {
                std::time_t now = std::time(nullptr);
                int32_t expire = shadow["expire"].get_int32();

                if (now<expire) {
                    status = Gate::Allowed;
                    break;
                }
                else {
                    status = Gate::ExpiredPassword;
                    break;
                }

            }
            else {
                status = Gate::InvalidPassword;
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

Variant Gate::get_cache()
{
    AutoLock shadow_lock(LockMode::Read,&shadowdb);
    Variant database = shadowdb.read();

    Variant cache = Variant::create_array(0);

    //Validate here

    for (size_t n=0;n<database["passwords"].count();n++) {
        Variant entry = database["passwords"][n];

        Variant tmp = Variant::create_struct();
        tmp["name"] = entry["name"];
        tmp["expire"] = entry["expire"];

        cache.append(tmp);
    }

    return cache;
}

void Gate::set_logger(function<void(int priority,string message)> cb)
{
    this->log_cb = cb;
}

int Gate::auth_exec(string method, string user, string password)
{
    int status = Gate::Error;

    Exec libgate(method);

    try {
        log(LOG_DEBUG,"exec " + method + "\n");
        Variant data = libgate.run(user,password);
        clog<<data<<endl;

        status = data["status"].get_int32();
        clog<<"exec status:"<<status<<endl;

        if (status == Gate::Allowed) {
            Variant response = data["response"];

            if (validate(response,Validator::Authenticate)) {
                log(LOG_DEBUG,"response validated\n");
                update_db(response);
                update_shadow_db(user,password);
            }
            else {
                log(LOG_ERR,"Bad Authenticate response\n");
                status = Gate::Error;
            }
        }

    }
    catch(std::exception& e) {
        log(LOG_ERR,string(e.what()) + "\n");
        status = Gate::Error;
    }

    return status;
}

int Gate::authenticate(string user,string password)
{
    int status = Gate::Error;

    for (AuthMethod method : auth_methods) {
        clog<<"trying with method:"<<(int)method<<endl;

        if (status == Gate::Error or status == Gate::UserNotFound) {
            switch (method) {

                case AuthMethod::Local: {
                    log(LOG_INFO,"Trying with local cache\n");
                    try {
                        status = lookup_password(user,password);
                    }
                    catch(std::exception& e) {
                        log(LOG_ERR,string(e.what()) + "\n");
                        status = Gate::Error;
                    }
                }
                break;

                case AuthMethod::ADI:
                    status = auth_exec("adi",user,password);
                break;

                case AuthMethod::ID:
                    status = auth_exec("id",user,password);
                    clog<<"status:"<<status<<endl;
                break;

                default:
                    log(LOG_ERR,"Unknown authentication method:" + std::to_string((int)method) + "\n");
            }
        }

    }

    return status;
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

            return validate(data["user"],Validator::User);
        break;

        default:
            return false;
    }
}

string Gate::salt(string username)
{
    string value;
    const int range = 'z' - 'A';
    const int start = 'A';
    int32_t rnd = (int32_t)std::time(nullptr);

    for (size_t n=0;n<10;n++) {
        int m = n % username.size();
        int32_t c = username[m] + rnd;
        c = c % range;
        value.push_back(start+c);
    }

    return value;
}

string Gate::hash(string password,string salt)
{
    salt = "$6$" + salt + "$";
    char* data = crypt(password.c_str(),salt.c_str());

    return string(data);
}

void Gate::load_config()
{
    const std::string cfg_path = "/etc/llx-gva-gate.cfg";

    fstream file;

    file.open(cfg_path, std::fstream::in);

    if (file.good()) {
        try {
            Variant cfg = json::load(file);

            file.close();

            if (cfg["server"].is_string()) {
                this->server = cfg["server"].get_string();
            }

            if (cfg["auth_mode"].is_string()) {
                string mode = cfg["auth_mode"].get_string();

                if (mode == "remote") {
                    auth_mode = Gate::Remote;
                }

                if (mode == "local") {
                    auth_mode = Gate::Local;
                }

                if (mode == "all") {
                    auth_mode = Gate::All;
                }
            }

            if (cfg["auth_methods"].is_array()) {
                auth_methods.clear();

                for (Variant m : cfg["auth_methods"].get_array()) {
                    if (m.is_string()) {
                        string method = m.get_string();

                        if (method == "local") {
                            auth_methods.push_back(AuthMethod::Local);
                        }

                        if (method == "adi") {
                            auth_methods.push_back(AuthMethod::ADI);
                        }

                        if (method == "id") {
                            auth_methods.push_back(AuthMethod::ID);
                        }
                    }
                }

                if (auth_methods.size() == 0) {
                    auth_methods.push_back(AuthMethod::Local);
                }
            }
        }
        catch (std::exception& e) {
            log(LOG_WARNING,"Failed to parse config file\n");
        }
    }
}
