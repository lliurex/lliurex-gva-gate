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

    //validate here

    return database["machine-token"].get_string();
}

void Gate::update_db(Variant data)
{
    Variant src_data = read_db();
    Variant src_groups = src_data["group"];

    Variant groups = data;

    for (int n=0;n<groups.count();n++) {
        Variant group = groups[n];
        string name = group["name"];
        int gid = group["gid"];

        bool match = false;

        for (int m=0;m<src_groups.count();m++) {
            Variant src_group = src_groups[m];

            string src_name = src_group["name"];
            int src_gid = src_group["gid"];

            if (gid == src_gid and name == src_name) {
                log(LOG_DEBUG,"match "+name+" gid:"+std::to_string(gid));
                match = true;

                // user look-up
                Variant members = group["members"];
                for (int i=0;i<members.count();i++) {
                    string user = members[i];

                    Variant src_members = src_group["members"];

                    bool user_match = false;
                    for (int j=0;j<src_members.count();j++) {
                        string src_user = src_members[j];

                        if (user == src_user) {
                            user_match = true;
                            break;
                        }
                    }

                    if (!user_match) {
                        src_members.append(user);
                    }
                }

                break;
            }

        }

        if (!match) {
            log(LOG_INFO,"adding a new group:" + group["name"]);
            src_groups.append(group);
        }
    }

    write_db(src_data);

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

Variant Gate::get_groups()
{

    Variant value = read_db();

    return value["group"];
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

    clog<<response.status<<endl;
    clog<<response.content.str()<<endl;

    if (response.status==200) {
        Variant data = response.parse();
        clog<<data<<endl;
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
        case Validator::Members:
            if (!data.is_array()) {
                return false;
            }

            for (size_t n=0;n<data.count();n++) {
                if (!data[n].is_string()) {
                    return false;
                }
            }

            return true;
        break;

        case Validator::Groups:
            if (!data.is_array()) {
                return false;
            }

            for (size_t n=0;n<data.count();n++) {
                bool value = validate(data[n],Validator::Group);
                if (!value) {
                    return false;
                }
            }

            return true;

        break;

        case Validator::Group:
            if (!data.is_struct()) {
                return false;
            }

            if (!data["gid"].is_int32()) {
                return false;
            }

            if (!data["name"].is_string()) {
                return false;
            }

            return validate(data["members"],Validator::Members);
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

        case Validator::Login:
            if (!data.is_struct()) {
                return false;
            }

            if (!data["user"].is_string()) {
                return false;
            }

            if (!data["success"].is_boolean()) {
                return false;
            }

            if (data["success"].get_boolean() == false) {
                return true;
            }

            return validate(data["group"],Validator::Groups);
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
