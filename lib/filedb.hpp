// SPDX-FileCopyrightText: 2022 Enrique M.G. <quiqueiii@gmail.com>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef LLX_GVA_GATE_FILEDB
#define LLX_GVA_GATE_FILEDB

#include <variant.hpp>
#include <cstdio>
#include <stdexcept>
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
                bool opened = false;

                switch (mode) {
                    case LockMode::Read:
                        opened = target->open(true);

                        break;
                    case LockMode::Write:
                        opened = target->open();

                        break;
                }

                if (!opened) {
                    throw std::runtime_error("Failed to open FileDB");
                }

                try {
                    switch (mode) {
                        case LockMode::Read:
                            target->lock_read();

                            break;
                        case LockMode::Write:
                            target->lock_write();

                            break;
                    }
                }
                catch (...) {
                    /* do not leave the file open if the lock fails */
                    target->close();
                    throw;
                }
            }

            ~AutoLock()
            {
                try {
                    unlock();
                }
                catch (...) {
                    /* a destructor must not throw */
                }

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
