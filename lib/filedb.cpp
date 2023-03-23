// SPDX-FileCopyrightText: 2022 Enrique M.G. <quiqueiii@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "filedb.hpp"

#include <variant.hpp>
#include <json.hpp>
#include <bson.hpp>

#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

#include <experimental/filesystem>
#include <iostream>
#include <fstream>

using namespace lliurex;
using namespace edupals;
using namespace edupals::variant;

using namespace std;
namespace stdfs=std::experimental::filesystem;

FileDB::FileDB() : db(nullptr)
{
}

FileDB::FileDB(string path,string magic) : path(path), format(DBFormat::Json), db(nullptr), magic(magic)
{

}

FileDB::~FileDB()
{
    close();
}

bool FileDB::exists()
{
    const stdfs::path dbpath {path};

    return stdfs::exists(dbpath);
}

bool FileDB::is_open()
{
    return (db != nullptr);
}

void FileDB::create(DBFormat format,uint32_t mode)
{
    this->format = format;

    db = fopen(path.c_str(),"wb");
    int fd = fileno(db);
    fchmod(fd,mode);

    lock_write();

    Variant data = Variant::create_struct();
    write(data);

    unlock();
    close();
}

bool FileDB::open(bool read_only)
{
    if (db == nullptr) {
        const char* rw = "r+";
        const char* ro = "r";
        const char* options = (read_only) ? ro : rw;
        db = fopen(path.c_str(),options);

        if (db) {
            uint32_t header;
            size_t status = fread(&header,4,1,db);

            if (status == 1) {
                fseek(db, 0, SEEK_END);

                if (ftell(db) == header) {
                    format = DBFormat::Bson;
                }
            }

            fseek(db,0,SEEK_SET);
        }
    }

    return (db != nullptr);
}

void FileDB::close()
{
    if (db != nullptr) {
        fclose(db);
        db = nullptr;
    }
}

void FileDB::lock_read()
{
    int fd = fileno(db);
    int status = flock(fd,LOCK_SH);

    if (status != 0) {
        //TODO
    }

}

void FileDB::lock_write()
{
    int fd = fileno(db);
    int status = flock(fd,LOCK_EX);

    if (status != 0) {
        //TODO
    }

}

void FileDB::unlock()
{
    int fd = fileno(db);
    int status = flock(fd,LOCK_UN);

    if (status != 0) {
        //TODO
    }

}

edupals::variant::Variant FileDB::read()
{
    fseek(db,0,SEEK_SET);

    stringstream ss;
    char buffer[128];
    size_t len;

    L0:
    len = fread(buffer,1,128,db);

    if (len > 0) {
        ss.write((const char*)buffer,len);
        goto L0;
    }

    Variant value;

    if (format == DBFormat::Json) {
         value = json::load(ss);
    }

    if (format == DBFormat::Bson) {
         value = bson::load(ss);
    }

    if (!value.is_struct()) {
        //TODO
    }

    if (value["magic"].get_string() != magic) {
        //TODO
    }

    return value["data"];
}

void FileDB::write(edupals::variant::Variant data)
{
    stringstream ss(std::stringstream::out | std::stringstream::binary);
    //std::stringbuf buffer;
    //std::ostream ss(&buffer);

    Variant header;
    header = Variant::create_struct();
    header["magic"] = magic;
    header["data"] = data;

    if (format == DBFormat::Json) {
        json::dump(header,ss);
    }

    if (format == DBFormat::Bson) {
        bson::dump(header,ss);
    }

    fseek(db,0,SEEK_SET);
    int fd = fileno(db);
    ftruncate(fd,0);

    int status = fwrite(ss.str().c_str(),ss.str().size(),1,db);

    if (status == 0) {
        //TODO: raise exception here?
    }

    syncfs(fd);

}

void FileDB::guess_format()
{
    uint32_t data;
    size_t len;

    fseek(db,0,SEEK_SET);
    len = fread(&data,sizeof(uint32_t),1,db);

    if (len != 1) {
        //TODO
    }

    if (data == '{') {
        format = DBFormat::Json;
    }
    else {
        // improve this
        format = DBFormat::Bson;
    }

}
