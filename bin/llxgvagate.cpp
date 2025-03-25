// SPDX-FileCopyrightText: 2022 Enrique M.G. <quiqueiii@gmail.com>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "libllxgvagate.hpp"

#include <variant.hpp>
#include <console.hpp>
#include <cmd.hpp>

#include <unistd.h>
#include <termios.h>
#include <sysexits.h>

#include <iostream>
#include <string>
#include <chrono>

using namespace lliurex;

using namespace edupals;
using namespace edupals::variant;

using namespace std;

int debug_level = LOG_ERR;
std::chrono::time_point<std::chrono::steady_clock> timestamp;

void log(int priority,string message)
{
    std::chrono::time_point<std::chrono::steady_clock> now = std::chrono::steady_clock::now();

    double secs = std::chrono::duration_cast<std::chrono::milliseconds>(now - timestamp).count()/1000.0;

    if (priority > debug_level ) {
        return;
    }

    if (priority <= LOG_ERR) {
        clog<<console::fg::red<<console::style::bold;
    }

    if (priority == LOG_WARNING) {
        clog<<console::fg::yellow<<console::style::bold;
    }

    clog<<"["<<secs<<"] ";
    clog<<message<<console::reset::all;

    clog.flush();
}

void help()
{
    cout<<"Usage: llx-gva-gate [options] command"<<endl;

    cout<<endl;

    cout<<"options:"<<endl;
    cout<<"-h\t--help\tshow this help"<<endl;
    cout<<"-v\t--version\tshow version"<<endl;
    cout<<"-d[n]\t--debug [n]\tSets log level"<<endl;
    cout<<"\t--verbose\tsets maximum log level"<<endl;

    cout<<"commands:"<<endl;
    cout<<"create\t\tcreates database. (root)"<<endl;
    cout<<"groups\t\ttlist group database"<<endl;
    cout<<"users\t\ttlist user database"<<endl;
    cout<<"auth\t\tperfoms an user authentication process"<<endl;
    cout<<"cache list | purge"<<endl;
    cout<<"\t\tlist\tlists cached users and expiration time"<<endl;
    cout<<"\t\tpurge\tpurges cache database"<<endl;

}

int main(int argc,char* argv[])
{
    timestamp = std::chrono::steady_clock::now();

    cmd::ArgumentParser parser;
    cmd::ParseResult result;

    parser.add_option(cmd::Option('h',"help",cmd::ArgumentType::None));
    parser.add_option(cmd::Option('v',"version",cmd::ArgumentType::None));
    parser.add_option(cmd::Option('d',"debug",cmd::ArgumentType::Required));
    parser.add_option(cmd::Option("verbose",cmd::ArgumentType::None));

    result=parser.parse(argc,argv);

    if (!result.success()) {
        help();

        return EX_USAGE;
    }

    for (cmd::Option o:result.options) {
        if (o.short_name=='d') {
            debug_level = std::stoi(o.value);
        }

        if (o.long_name == "verbose") {
            debug_level = 7;
        }

        if (o.short_name=='h') {
            help();

            return EX_OK;
        }
    }

/*
    for (cmd::Option o:result.options) {
        clog<<"+ "<<o.short_name<<endl;
    }

    for (string s:result.args) {
        clog<<"* "<<s<<endl;
    }
*/

    string cmd;

    if (result.args.size()>1) {
        cmd = result.args[1];
    }

    if (cmd == "create") {

        if (getuid() != 0) {
            cerr<<"Root user expected"<<endl;
            return EX_NOPERM;
        }

        Gate gate(log);
        if (!gate.exists_db()) {
            gate.create_db();
        }
        else {
            clog<<"database already exists"<<endl;
        }

        return EX_OK;
    }

    if (cmd == "chkpwd") {
        if (isatty(STDIN_FILENO)) {
            cerr<<"This command can not be executed from terminal"<<endl;

            return EX_NOPERM;
        }

        if (result.args.size()<3) {
            return EX_USAGE;
        }

        Gate gate(log);
        gate.open();

        int status = gate.lookup_password(result.args[2],result.args[3]);

        return status;
    }

    if (cmd == "groups") {
        Gate gate(log);
        gate.open(true);
        Variant groups = gate.get_groups();
        for (int n=0;n<groups.count();n++) {
            cout<<groups[n]["name"].get_string()<<":"<<groups[n]["gid"]<<":";

            Variant members = groups[n]["members"];

            for (size_t n=0;n<members.count();n++) {
                cout<<members[n].get_string();

                if (n<(members.count()-1)) {
                    cout<<",";
                }
            }

            cout<<endl;
        }

        return EX_OK;
    }

    if (cmd == "users") {
        Gate gate(log);
        gate.open(true);
        Variant users = gate.get_users();
        for (int n=0;n<users.count();n++) {
            Variant passwd = users[n];

            cout<<passwd["name"].get_string()<<":x:";
            cout<<passwd["uid"].get_int32()<<":";
            cout<<passwd["gid"].get_int32()<<":";
            cout<<passwd["gecos"].get_string()<<":";
            cout<<passwd["dir"].get_string()<<":";
            cout<<passwd["shell"].get_string()<<endl;
        }

        return EX_OK;
    }

    if (cmd == "auth") {

        if (getuid() != 0) {
            cerr<<"Root user expected. Is setuid bit set?"<<endl;
            return EX_NOPERM;
        }

        Gate gate(log);
        if (!gate.exists_db()) {
            gate.create_db();
        }

        gate.open();
        gate.load_config();

        string user;
        string password;

        if (result.args.size()>2) {
            user = result.args[2];
        }
        else {
            cout<<"user:";
            cin>>user;
        }

        cout<<"password:";

        // disable input echo
        termios oldt;
        tcgetattr(STDIN_FILENO, &oldt);
        termios newt = oldt;
        newt.c_lflag &= ~ECHO;
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);

        cin>>password;

        // restore
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

        clog<<endl;

        int status = gate.authenticate(user,password);

        string message;
        switch (status) {
            case Gate::Error:
                message = "Error performing authentication";
            break;

            case Gate::Unauthorized:
                message = "Invalid user/password";
            break;

            case Gate::UserNotFound:
                message = "User not found";
            break;

            case Gate::InvalidPassword:
                message = "Invalid password";
            break;

            case Gate::ExpiredPassword:
                message = "Password has expired";
            break;

            case Gate::Allowed:
                message = "Authentication succeeded";
            break;

        }

        clog<<"status:"<<message<<endl;

        return (status>0) ? EX_OK : EX_DATAERR;
    }

    if (cmd == "cache") {
        string cmd2;

        if (result.args.size()>2) {
            cmd2 = result.args[2];
        }
        else {
            help();
            return EX_USAGE;
        }

        if (cmd2 == "list") {

            Gate gate(log);
            gate.open();

            Variant cache = gate.get_cache();

            for (int n=0;n<cache.count();n++) {
                Variant entry = cache[n];
                int32_t current = (int32_t)std::time(nullptr);
                int32_t expire = entry["expire"].get_int32();
                string output;

                if (expire<current) {
                    output = "expired";
                }
                else {
                    output = std::to_string(expire-current) + " seconds";
                }

                cout<<entry["name"].get_string()<<" "<<output<<endl;
            }

            return EX_OK;

        }
        else {
            if (cmd2 == "purge") {
                Gate gate(log);
                gate.open();

                gate.purge_shadow_db();

                return EX_OK;
            }
            else {
                cerr<<"see help for details"<<endl;
                return EX_USAGE;
            }
        }

    }

    help();
    return EX_OK;
}
