// SPDX-FileCopyrightText: 2022 Enrique M.G. <quiqueiii@gmail.com>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

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

    enum class LockMode
    {
        Read,
        Write
    };

    class FileDB
    {
        public:

        FileDB();
        FileDB(std::string path,std::string magic);
        virtual ~FileDB();

        bool exists();
        bool is_open();

        void create(DBFormat format, uint32_t mode = 775);
        bool open(bool read_only = false);
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

            AutoLock(LockMode mode, FileDB* target) : target(target)
            {
                switch (mode) {
                    case LockMode::Read:
                        target->open(true);
                        target->lock_read();

                        break;
                    case LockMode::Write:
                        target->open();
                        target->lock_write();

                        break;
                }
            }

            ~AutoLock()
            {
                unlock();
                target->close();
            }

            void unlock()
            {
                target->unlock();
            }

        protected:

            FileDB* target;
    };
}

#endif
