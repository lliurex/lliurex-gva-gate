// SPDX-FileCopyrightText: 2022 Enrique M.G. <quiqueiii@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LLX_GVA_GATE
#define LLX_GVA_GATE

#include "filedb.hpp"

#include <variant.hpp>

#include <syslog.h>

#include <cstdio>
#include <functional>
#include <string>

#define LLX_GVA_GATE_DB "/tmp/llx-gva-gate.db"
#define LLX_GVA_GATE_MAGIC  "LLX-GVA-GATE"

namespace lliurex
{
    enum class Validator {
        Database,
        Users,
        User,
        Groups,
        Group,
        Authenticate
    };

    class Gate
    {
        public:


        Gate();
        Gate(std::function<void(int priority,std::string message)> cb);

        virtual ~Gate();

        bool exists_db();
        edupals::variant::Variant read_db();
        void write_db(edupals::variant::Variant data);

        void create_db();
        std::string machine_token();

        void update_db(edupals::variant::Variant data);

        void lock_db_read();
        void lock_db_write();
        void unlock_db();

        edupals::variant::Variant get_groups();
        edupals::variant::Variant get_users();

        bool authenticate(std::string user,std::string password);

        bool validate(edupals::variant::Variant data,Validator validator);

        void test_read();
        void test_write();

        void set_logger(std::function<void(int priority,std::string message)> cb);

        protected:

        void log(int priority, std::string message);

        FILE* dbase;

        std::function<void(int priority,std::string message)> log_cb;

        FileDB userdb;
        FileDB shadowdb;

    };
}

#endif
