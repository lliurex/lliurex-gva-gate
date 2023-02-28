// SPDX-FileCopyrightText: 2022 Enrique M.G. <quiqueiii@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "filedb.hpp"

#include <sys/file.h>
#include <unistd.h>

#include <experimental/filesystem>

using namespace lliurex;

using namespace std;
namespace stdfs=std::experimental::filesystem;

FileDB::FileDB(string path) : path(path), format(DBFormat::Json), db(nullptr)
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

void FileDB::create(DBFormat format)
{
    db = fopen(path.c_str(),"wb");
    lock_write();

    unlock();
    close();
}

void FileDB::open()
{

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

    return value;
}

void FileDB::write(edupals::variant::Variant data)
{
    stringstream ss;

    if (format == DBFormat::Json) {
        json::dump(data,ss);
    }

    if (format == DBFormat::Bson) {
        bson::dump(data,ss);
    }

    fseek(db,0,SEEK_SET);
    int fd = fileno(db);
    ftruncate(fd,0);

    int status = fwrite(ss.str().c_str(),ss.str().size(),1,db);

    if (status == 0) {
        //TODO: raise exception here?
    }

}

