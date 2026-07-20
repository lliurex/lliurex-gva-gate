// SPDX-FileCopyrightText: 2026 Enrique M.G. <quique@necos.es>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "libllxgvagate.hpp"

#include <unistd.h>
#include <termios.h>
#include <sysexits.h>
#include <pwd.h>
#include <grp.h>
#include <sys/wait.h>
#include <syslog.h>

#include <iostream>
#include <string>
#include <vector>
#include <filesystem>

using namespace std;

void create_home(string basepath,string username,uid_t uid, gid_t gid)
{
    try {
        std::filesystem::create_directory(basepath);

        int status = chmod(basepath.c_str(), 0755);

        if (status != 0) {
            cerr<<"failed to set 0755 permissions to "<<basepath<<endl;
        }

        string homepath = basepath + "/" + username;

        std::filesystem::create_directory(homepath);

        status = chmod(homepath.c_str(), 0750);

        if (status != 0) {
            cerr<<"failed to set 0750 permissions to "<<homepath<<endl;
        }

        status = chown(homepath.c_str(),uid,gid);

        if (status != 0) {
            cerr<<"failed to set owner to "<<homepath<<endl;
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

    //create_home();

    /*
    syslog(LOG_INFO,"arg count: %d",argc);
    for (int n=0;n<argc;n++) {
        syslog(LOG_INFO,"args: %s",argv[n]);
    }
    */

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
