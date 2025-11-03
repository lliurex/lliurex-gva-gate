// SPDX-FileCopyrightText: 2022 Enrique M.G. <quiqueiii@gmail.com>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "libllxgvagate.hpp"
#include "filedb.hpp"
#include "exec.hpp"
#include "observer.hpp"

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

#define LLX_GVA_GATE_DEFAULT_EXPIRATION 7 * 1440
#define LLX_GVA_GATE_MAX_EXPIRATION     30 * 1440

Gate::Gate() : Gate(nullptr)
{
}

Gate::Gate(function<void(int priority,string message)> cb) : log_cb(cb),
    auth_methods({AuthMethod::Local}), expiration(LLX_GVA_GATE_DEFAULT_EXPIRATION)
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

bool Gate::exists_db(bool root)
{
    bool status = userdb.exists();

    if (root) {
        status = status and shadowdb.exists();
    }

    return status;
}

void Gate::create_db()
{

    log(LOG_DEBUG,"Creating databases...\n");
    try {
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
            userdb.close();

            // creates session shared counter
            Observer::create();
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
            shadowdb.close();
        }
    }
    catch (std::exception& e) {
        log(LOG_ERR,"Something went bad creating database\n");
        log(LOG_ERR, e.what());
    }

}

Variant Gate::get_user_db()
{
    AutoLock lock(LockMode::Read,&userdb);

    return userdb.read();
}

Variant Gate::get_shadow_db()
{
    AutoLock shadow_lock(LockMode::Read,&shadowdb);

    return shadowdb.read();
}

void Gate::update_db(Variant data)
{

    AutoLock user_lock(LockMode::Write,&userdb);

    Variant user_data = userdb.read();
    string what;
    if (!validate(user_data,Validator::UserDatabase, what)) {
        log(LOG_ERR,"Bad user database\n");
        throw exception::GateError("Bad user database:\n" + what + "\n",0);
    }

    string login = data["login"].get_string();
    int32_t uid = data["uid"].get_int32();

    Variant tmp = Variant::create_array(0);

    for (size_t n=0;n<user_data["users"].count();n++) {
        Variant user = user_data["users"][n];

        if (user["login"].get_string() != login and user["uid"].get_int32() != uid) {
            tmp.append(user);
        }
    }

    tmp.append(data);

    user_data["users"] = tmp;

    userdb.write(user_data);

    //updates shared counter
    Observer::push();

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
            shadow["expire"] = (60*expiration) + (int32_t)std::time(nullptr);
            found = true;
            break;
        }
    }

    if (!found) {
        Variant shadow = Variant::create_struct();
        shadow["name"] = name;
        shadow["key"] = hash(password,salt(name));
        shadow["expire"] = (60*expiration) + (int32_t)std::time(nullptr);
        database["passwords"].append(shadow);
    }

    shadowdb.write(database);

}

void Gate::purge_user_db()
{
    AutoLock user_lock(LockMode::Write,&userdb);

    Variant database = Variant::create_struct();
    database["users"] = Variant::create_array(0);

    userdb.write(database);
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

    string username;
    string domain;

    truncate_domain(user,username,domain);

    //Validate here

    for (size_t n=0;n<database["passwords"].count();n++) {
        Variant shadow = database["passwords"][n];

        if (shadow["name"].get_string() == username) {
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
    string what;

    if (!validate(database,Validator::UserDatabase, what)) {
        log(LOG_ERR,"Bad user database\n");
        throw exception::GateError("Bad user database\n:"+ what+ "\n",0);
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

    //Variant user_data = userdb.read();
    string what;
    if (!validate(database,Validator::UserDatabase, what)) {
        log(LOG_ERR,"Bad user database\n");
        throw exception::GateError("Bad user database\n:" + what + "\n",0);
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
        string what;
        if (validate(data, Validator::Authenticate, what)) {
            status = data["status"].get_int32();
            log(LOG_DEBUG,"status:" + std::to_string(status) + "\n");

            if (status == Gate::Allowed) {
                data["user"]["method"] = method;
                update_db(data["user"]);
                update_shadow_db(user,password);
            }

        }
        else {
            log(LOG_ERR,"Bad Authenticate response:\n" + what + "\n");
            status = Gate::Error;
        }

    }
    catch(std::exception& e) {
        log(LOG_ERR,string(e.what()) + "\n");
        status = Gate::Error;
    }

    return status;
}

bool Gate::truncate_domain(string user, string& username, string& domain)
{
    std::size_t found = user.find("@");

    if (found != std::string::npos) {
        username = user.substr(0,found);
        domain = user.substr(found + 1, user.size());

        return true;
    }
    else {
        username =user;
        domain = "";

        return false;
    }

}

int Gate::authenticate(string user,string password)
{
    int status = Gate::Error;

    string username;
    string domain;

    bool has_domain = truncate_domain(user,username,domain);
    log(LOG_DEBUG,"username:"+username+"\n");

    if (has_domain) {
        log(LOG_DEBUG,"domain:"+domain+"\n");
    }

    for (AuthMethod method : auth_methods) {

        if (status == Gate::Error or status == Gate::UserNotFound) {
            switch (method) {

                case AuthMethod::Local: {
                    log(LOG_INFO,"Trying with local cache\n");
                    try {
                        status = lookup_password(username,password);
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
                break;

                case AuthMethod::CDC:
                    status = auth_exec("cdc",user,password);
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

bool Gate::validate(Variant data,Validator validator, string& what)
{
    switch (validator) {

        case Validator::Groups:
            if (!data.is_array()) {
                what = "Groups type is not Array";
                return false;
            }

            for (size_t n=0;n<data.count();n++) {
                string cwhat;
                if (!validate(data[n],Validator::Group, cwhat)) {
                    what = "Bad Group format:" + cwhat;
                    return false;
                }
            }

            return true;

        break;

        case Validator::Group:
            if (!data.is_struct()) {
                what = "Group type is not Struct";
                return false;
            }

            if (!data["name"].is_string()) {
                what = "Expected field name with type String";
                return false;
            }

            if (!data["gid"].is_int32()) {
                what = "Expected field gid with type Int32";
                return false;
            }

            return true;
        break;

        case Validator::UserDatabase:
            if (!data.is_struct()) {
                what = "UserDatabse type is not Struct";
                return false;
            }

            return validate(data["users"],Validator::Users, what);
        break;

        case Validator::ShadowDatabase:
            if (!data.is_struct()) {
                what = "ShadowDatabase type is not Struct";
                return false;
            }

            return validate(data["passwords"],Validator::Shadows, what);
        break;

        case Validator::Shadows:
            if (!data.is_array()) {
                what = "Shadows type is not Array";
                return false;
            }

            for (size_t n=0;n<data.count();n++) {
                string cwhat;
                if (!validate(data[n],Validator::Shadow, cwhat)) {
                    what = "Bad Shadow format:" + cwhat;
                    return false;
                }
            }

            return true;
        break;

        case Validator::Shadow:
            if (!data.is_struct()) {
                what = "Shadow type is not a Struct";
                return false;
            }

            if (!data["name"].is_string()) {
                what = "Expected field name with type String";
                return false;
            }

            if (!data["key"].is_string()) {
                what = "Expected field key with type String";
                return false;
            }

            if (!data["expire"].is_int32()) {
                what = "Expected field expire with type Int32";
                return false;
            }

            return true;
        break;

        case Validator::Users:
            if (!data.is_array()) {
                what = "Users type is not a Struct";
                return false;
            }

            for (size_t n=0;n<data.count();n++) {
                return validate(data[n],Validator::User, what);
            }

            return true;
        break;

        case Validator::User:
            if (!data["login"].is_string()) {
                what = "Expected field login with type String";
                return false;
            }

            if (!data["uid"].is_int32()) {
                what = "Expected field uid with type Int32";
                return false;
            }

            if (!validate(data["gid"],Validator::Group, what)) {
                return false;
            }

            if (!data["name"].is_string()) {
                what = "Expected field name with type String";
                return false;
            }

            if (!data["surname"].is_string()) {
                what = "Expected field surname with type String";
                return false;
            }

            if (!data["home"].is_string()) {
                what = "Expected field home with type String";
                return false;
            }

            if (!data["shell"].is_string()) {
                what = "Expected field shell with type String";
                return false;
            }
            return validate(data["groups"],Validator::Groups, what);
        break;

        case Validator::Authenticate:
            if (!data.is_struct()) {
                what = "Authenticate type is not a Struct";
                return false;
            }

            if (!data["status"].is_int32()) {
                what = "Expected field status with type Int32";
                return false;
            }

            if (!data["user"].is_struct()) {
                what = "Expected field user with type Struct";
                return false;
            }

            return validate(data["user"],Validator::User,what);
        break;

        default:
            what = "Unknown validation";
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

bool Gate::get_pwnam(string user_name, struct passwd* user_info)
{
    if (!user_info) {
        return false;
    }

    Variant users = get_users();


    for (size_t n=0;n<users.count();n++) {
        Variant user = users[n];

        if (user["name"].get_string() == user_name) {

            pw_name = user["name"].get_string();
            pw_dir = user["dir"].get_string();
            pw_shell = user["shell"].get_string();
            pw_gecos = user["gecos"].get_string();

            user_info->pw_name = (char*) pw_name.c_str();
            user_info->pw_uid = user["uid"].get_int32();
            user_info->pw_gid = user["gid"].get_int32();
            user_info->pw_dir = (char*)pw_dir.c_str();
            user_info->pw_shell = (char*)pw_shell.c_str();
            user_info->pw_gecos = (char*)pw_gecos.c_str();

            return true;
        }
    }

    return false;
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

            if (cfg["expiration"].is_int32()) {
                
                expiration = cfg["expiration"].get_int32();

                bool range_check = false;

                if (expiration < 0) {
                    expiration = 0;
                    range_check = true;
                }

                if (expiration > LLX_GVA_GATE_MAX_EXPIRATION) {
                    expiration = LLX_GVA_GATE_MAX_EXPIRATION;
                    range_check = true;
                }

                if (range_check) {
                    log(LOG_WARNING,"Expiration property is out of range [0,43200] minutes\n");
                    log(LOG_WARNING,"Expiration has been set to:"+std::to_string(expiration)+" minutes\n");
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

                        if (method == "cdc") {
                            auth_methods.push_back(AuthMethod::CDC);
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
            log(LOG_DEBUG,string(e.what()) + "\n");
        }
    }
}
