// SPDX-FileCopyrightText: 2022 Enrique M.G. <quiqueiii@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "libllxgvagate.hpp"

#include <variant.hpp>
#include <json.hpp>
#include <bson.hpp>

#include <iostream>
#include <fstream>
#include <experimental/filesystem>

using namespace lliurex;
using namespace edupals;
using namespace edupals::variant;

using namespace std;
namespace stdfs=std::experimental::filesystem;

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

    fstream fb;
    fb.open(LLX_GVA_GATE_DB,ios::out);
    json::dump(database,fb);
    fb.close();
}

void Gate::lock_db()
{

}

void Gate::unlock_db()
{

}

Variant Gate::get_groups()
{
    fstream fb;
    fb.open(LLX_GVA_GATE_DB,ios::in);
    Variant value = json::load(fb);
    fb.close();

    return value["groups"];
}
