// SPDX-FileCopyrightText: 2022 Enrique M.G. <quiqueiii@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "libllxgvagate.hpp"

#include <variant.hpp>
#include <console.hpp>

#include <iostream>
#include <string>

using namespace lliurex;

using namespace edupals;
using namespace edupals::variant;

using namespace std;

bool verbose = false;

void log(int priority,string message)
{
    if (priority >= LOG_DEBUG and verbose==false) {
        return;
    }

    if (priority <= LOG_ERR) {
        clog<<console::fg::red<<console::style::bold<<message<<console::reset::all;
    }
    else {
        clog<<message;
    }

    clog.flush();
}

int main(int argc,char* argv[])
{

    string cmd;

    if (argc>1) {
        cmd = argv[1];
    }

    if (cmd == "create") {
        Gate gate(log);

        gate.create_db();
    }

    if (cmd == "groups") {
        Gate gate(log);
        Variant groups = gate.get_groups();
        for (int n=0;n<groups.count();n++) {
            cout<<groups[n]["name"].get_string()<<":"<<groups[n]["gid"]<<":";

            Variant members = groups[n]["members"];

            int m = 0;

            L0:
            cout<<members[m].get_string();
            m++;
            if (m<members.count()) {
                cout<<",";
                goto L0;
            }

            cout<<endl;
        }
    }

    if (cmd == "update") {
    }

    if (cmd == "login" and argc>3) {
        Gate gate(log);
        gate.authenticate(argv[2],argv[3]);
    }

    if (cmd == "test-read") {
        Gate gate(log);
        gate.test_read();
    }

    if (cmd == "test-write") {
        Gate gate(log);
        gate.test_write();
    }

    return 0;
}
