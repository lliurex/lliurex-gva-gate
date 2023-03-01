// SPDX-FileCopyrightText: 2022 Enrique M.G. <quiqueiii@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LLX_GVA_GATE_FILEDB
#define LLX_GVA_GATE_FILEDB

#include <variant.hpp>
#include <cstdio>
#include <string>

namespace lliurex
{
    enum class DBFormat
    {
        Json,
        Bson
    };

    class FileDB
    {
        public:

        FileDB();
        FileDB(std::string path,std::string magic);
        virtual ~FileDB();

        bool exists();
        bool is_open();

        void create(DBFormat format);
        void open();
        void close();

        void lock_read();
        void lock_write();
        void unlock();

        edupals::variant::Variant read();
        void write(edupals::variant::Variant data);


        protected:

        void guess_format();

        DBFormat format;
        std::string path;
        FILE* db;
        std::string magic;

    };

    class AutoLock
    {
        public:

            enum LockMode {
                Read,
                Write
            };

            AutoLock(LockMode mode, FileDB* target) : target(target)
            {
                switch (mode) {
                    case Read:
                        target->lock_read();
                        break;
                    case Write:
                        target->lock_write();
                        break;
                }
            }

            ~AutoLock()
            {
                target->unlock();
            }


        protected:

            FileDB* target;
    };
}

#endif
