// SPDX-FileCopyrightText: 2022 Enrique M.G. <quiqueiii@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "libllxgvagate.hpp"

#include <variant.hpp>
#include <json.hpp>
#include <bson.hpp>

#include <sys/file.h>
#include <systemd/sd-journal.h>

#include <iostream>
#include <fstream>
#include <experimental/filesystem>
#include <stdexcept>
#include <sstream>

using namespace lliurex;
using namespace edupals;
using namespace edupals::variant;

using namespace std;
namespace stdfs=std::experimental::filesystem;

Gate::Gate() : dbase(nullptr)
{
    sd_journal_print(LOG_DEBUG,"Gate constructor\n");

    if (!exists_db()) {
        sd_journal_print(LOG_DEBUG,"Database does not exists, creating an empty one...\n");
        dbase = fopen(LLX_GVA_GATE_DB,"wb");
        fclose(dbase);
    }

    dbase = fopen(LLX_GVA_GATE_DB,"r+");
}

Gate::~Gate()
{
    sd_journal_print(LOG_DEBUG,"Gate destructor\n");

    fclose(dbase);
}

bool Gate::exists_db()
{
    const stdfs::path dbpath {LLX_GVA_GATE_DB};

    return stdfs::exists(dbpath);
}

void Gate::create_db()
{
    Variant database = Variant::create_struct();

    database["magic"] = "LLX-GVA-GATE";
    database["groups"] = Variant::create_array(0);

    Variant grp = Variant::create_struct();
    grp["name"] = "students";
    grp["gid"] = 10000;
    grp["members"] = Variant::create_array(0);
    grp["members"].append("alpha");
    grp["members"].append("bravo");
    grp["members"].append("charlie");
    database["groups"].append(grp);

    grp = Variant::create_struct();
    grp["name"] = "teachers";
    grp["gid"] = 10001;
    grp["members"] = Variant::create_array(0);
    grp["members"].append("delta");
    grp["members"].append("echo");
    grp["members"].append("foxtrot");
    database["groups"].append(grp);

    fseek(dbase,0,SEEK_SET);

    stringstream ss;
    json::dump(database,ss);

    lock_db_write();

    int status = fwrite(ss.str().c_str(),ss.str().size(),1,dbase);

    if (status == 0) {
        sd_journal_print(LOG_ERR,"failed to write on DB\n");
    }

    unlock_db();
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

    return value["groups"];
}
