// SPDX-FileCopyrightText: 2026 Enrique M.G. <quique@necos.es>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "libllxgvagate.hpp"

#include <unistd.h>
#include <termios.h>
#include <sysexits.h>
#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <syslog.h>

#include <iostream>
#include <string>
#include <vector>
#include <filesystem>

using namespace std;

void create_home(string path,uid_t uid, gid_t gid)
{
    try {
        int status;

        std::filesystem::path fs_path(path);
        std::string basepath = fs_path.parent_path().string();

        if (!std::filesystem::exists(basepath)) {
            std::filesystem::create_directory(basepath);

            status = chmod(basepath.c_str(), 0755);

            if (status != 0) {
                cerr<<"failed to set 0755 permissions to "<<basepath<<endl;
            }

        }

        if (!std::filesystem::exists(path)) {
            std::filesystem::create_directory(path);

            status = chmod(path.c_str(), 0750);

            if (status != 0) {
                cerr<<"failed to set 0750 permissions to "<<path<<endl;
            }

            status = chown(path.c_str(),uid,gid);

            if (status != 0) {
                cerr<<"failed to set owner to "<<path<<endl;
            }
        }

    }
    catch(std::exception& e) {
        cerr<<e.what()<<endl;
    }
}

int main(int argc,char* argv[])
{
    if (geteuid() != 0) {
        cerr<<"Root user expected. Is setuid bit set?"<<endl;
        return EX_NOPERM;
    }

    struct passwd* user_info;
    uid_t uid = getuid();

    if (uid < LLX_GVA_BORDER_MIN_UID) {
        return EX_NOPERM;
    }

    user_info = getpwuid(uid);

    if (!user_info) {
        return EX_NOPERM;
    }

    user_info = getpwnam(user_info->pw_name);

    if (user_info == nullptr) {
        //cerr<<"Failed to fetch data from user "<<user<<", is NSS configured?"<<endl;
        cerr<<"errno:"<<errno<<endl;
        return EX_DATAERR;
    }

    if (user_info->pw_uid == uid) {
        return EX_NOPERM;
    }

    create_home(user_info->pw_dir, user_info->pw_uid, user_info->pw_gid);

    pid_t shell = fork();

    if (shell == 0) {

        setgroups(0, nullptr);
        initgroups(user_info->pw_name, user_info->pw_gid);
        setgid(user_info->pw_gid);
        setegid(user_info->pw_gid);
        setuid(user_info->pw_uid);
        seteuid(user_info->pw_uid);

        setenv("HOME", user_info->pw_dir, 1);
        setenv("SHELL", user_info->pw_shell, 1);
        setenv("USER", user_info->pw_name, 1);
        setenv("LOGNAME", user_info->pw_name, 1);
        setenv("PATH", "/usr/bin/", 1);

        string shell = user_info->pw_shell;
        char* args[33];

        int margc = std::min(argc,32);

        for (int n=0;n<margc;n++) {
            args[n] = argv[n];
        }

        args[0] = (char *)shell.c_str();
        args[margc] = nullptr;

        //execl(user_info->pw_shell, user_info->pw_shell, nullptr);
        execv(shell.c_str(),args);
    }
    else {
        int status;

        wait(&status);
        return status;
    }

}
