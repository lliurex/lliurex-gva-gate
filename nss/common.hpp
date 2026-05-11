// SPDX-FileCopyrightText: 2026 Enrique M.G. <quique@necos.es>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef LLX_GVA_GATE_NSS
#define LLX_GVA_GATE_NSS

#include <nss.h>
#include <grp.h>
#include <pwd.h>

#include <string>
#include <vector>
#include <cstdint>
#include <cstring>

namespace lliurex
{
    struct Group
    {
        std::string name;
        uint64_t gid;
        std::vector<std::string> members;
    };

    struct Passwd
    {
        std::string name;
        uint64_t uid;
        uint64_t gid;

        std::string gecos;

        std::string dir;
        std::string shell;
    };

    int push_string(std::string in,char** buffer, size_t* remain)
    {
        size_t fsize = in.size() + 1;
        if (fsize > *remain) {
            return -1;
        }

        std::memcpy(*buffer,in.c_str(),fsize);

        *remain -= fsize;
        *buffer += fsize;

        return 0;
    }

    int push_group(lliurex::Group& source, struct group* result, char* buffer, size_t buflen)
    {
        char* ptr = buffer;

        result->gr_gid = source.gid;

        result->gr_name = ptr;

        if (push_string(source.name,&ptr,&buflen) == -1) {
            return -1;
        }

        result->gr_passwd = ptr;

        if (push_string("x",&ptr,&buflen) == -1) {
            return -1;
        }

        std::vector<char*> tmp;

        for (std::string member : source.members) {
            char* q = ptr;
            if (push_string(member,&ptr,&buflen) == -1) {
                return -1;
            }
            tmp.push_back(q);
        }

        if ( (sizeof(char*)*(tmp.size()+1)) > buflen) {
            return -1;
        }

        result->gr_mem = (char**) ptr;

        int n = 0;

        for (n = 0;n<tmp.size();n++) {
            result->gr_mem[n] = tmp[n];
        }

        result->gr_mem[n] = 0;

        return 0;
    }

    int push_passwd(lliurex::Passwd& source, struct passwd* result, char* buffer, size_t buflen)
    {
        char* ptr = buffer;

        result->pw_uid = source.uid;
        result->pw_gid = source.gid;

        result->pw_name = ptr;

        if (push_string(source.name,&ptr,&buflen) == -1) {
            return -1;
        }

        result->pw_passwd = ptr;

        if (push_string("x",&ptr,&buflen) == -1) {
            return -1;
        }

        result->pw_gecos = ptr;

        if (push_string(source.gecos,&ptr,&buflen) == -1) {
            return -1;
        }

        result->pw_dir = ptr;

        if (push_string(source.dir,&ptr,&buflen) == -1) {
            return -1;
        }

        result->pw_shell = ptr;

        if (push_string(source.shell,&ptr,&buflen) == -1) {
            return -1;
        }

        return 0;
    }
}

#endif
