// SPDX-FileCopyrightText: 2022 Enrique M.G. <quiqueiii@gmail.com>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef LLX_GVA_GATE
#define LLX_GVA_GATE

#include "filedb.hpp"

#include <variant.hpp>

#include <syslog.h>
#include <pwd.h>

#include <cstdio>
#include <functional>
#include <string>
#include <exception>

#define LLX_GVA_GATE_DB_PATH "/var/lib/llx-gva-gate/"

#define LLX_GVA_GATE_USER_DB_MAGIC "LLX-USERDB"
#define LLX_GVA_GATE_USER_DB_FILE "user.db"
#define LLX_GVA_GATE_USER_DB_PATH LLX_GVA_GATE_DB_PATH LLX_GVA_GATE_USER_DB_FILE

#define LLX_GVA_GATE_SHADOW_DB_MAGIC "LLX-SHADOWDB"
#define LLX_GVA_GATE_SHADOW_DB_FILE "shadow.db"
#define LLX_GVA_GATE_SHADOW_DB_PATH LLX_GVA_GATE_DB_PATH LLX_GVA_GATE_SHADOW_DB_FILE

namespace lliurex
{
    enum class Validator {
        UserDatabase,
        ShadowDatabase,
        Shadows,
        Shadow,
        Users,
        User,
        Groups,
        Group,
        Authenticate
    };

    enum LookupStatus {
        Found,
        NotFound,
        InvalidPassword,
        ExpiredPassword
    };

    enum class AuthMethod {
            Local = 0,
            ADI = 1,
            ID = 2
    };

    namespace exception
    {
        class GateError: public std::exception
        {
            public:
            std::string what_message;
            std::string message;
            uint32_t code;

            GateError(std::string message,uint32_t code)
            {
                this->message = message;
                this->code = code;
                what_message = "[" + std::to_string(code) + "] " + message;
            }

            const char* what() const throw()
            {
                return what_message.c_str();
            }
        };
    }

    class Gate
    {
        public:

        enum AuthStatus {
            Allowed = 0,

            UserNotFound,
            InvalidPassword,
            ExpiredPassword,

            /* General auth error */
            Unauthorized,
            InteractionRequired,

            ServerNotFound = 10,
            InvalidResponse,
            BannedApp,
            BadArguments,
            AdiNotFound,

            /* General error */
            Error = 20

        };

        enum AuthMode {
            Default = 0,
            Remote = 1,
            Local = 2,
            All = 4
        };

        Gate();
        Gate(std::function<void(int priority,std::string message)> cb);

        virtual ~Gate();

        bool exists_db(bool root = false);
        void load_config();

        void create_db();

        edupals::variant::Variant get_user_db();
        edupals::variant::Variant get_shadow_db();

        void update_db(edupals::variant::Variant data);
        void update_shadow_db(std::string user,std::string password);
        int lookup_password(std::string user,std::string password);

        edupals::variant::Variant get_groups();
        edupals::variant::Variant get_users();
        edupals::variant::Variant get_cache();
        void purge_shadow_db();

        int authenticate(std::string user,std::string password);

        bool validate(edupals::variant::Variant data,Validator validator,std::string& what);

        void set_logger(std::function<void(int priority,std::string message)> cb);

        std::string salt(std::string username);
        std::string hash(std::string password,std::string salt);

        bool get_pwnam(std::string user_name, struct passwd* user_info);

        protected:

        int auth_exec(std::string method, std::string user, std::string password);
        void log(int priority, std::string message);
        std::string truncate_domain(std::string user);

        std::function<void(int priority,std::string message)> log_cb;

        FileDB userdb;
        FileDB shadowdb;

        std::string server;
        AuthMode auth_mode;

        // used for pwd pointer storage
        std::string pw_name;
        std::string pw_dir;
        std::string pw_shell;
        std::string pw_gecos;

        std::vector<AuthMethod> auth_methods;
    };
}

#endif
