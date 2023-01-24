// SPDX-FileCopyrightText: 2022 Enrique M.G. <quiqueiii@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "libllxgvagate.hpp"

#include <variant.hpp>

#include <iostream>
#include <string>

using namespace lliurex;

using namespace edupals;
using namespace edupals::variant;

using namespace std;

int main(int argc,char* argv[])
{
    clog<<"LliureX GVA Gate cli tool"<<endl;

    string cmd;

    if (argc>1) {
        cmd = argv[1];
    }

    if (cmd == "create") {
        Gate gate;

        clog<<"database:"<<gate.exists_db()<<endl;
        gate.create_db();
    }

    if (cmd == "groups") {
        Gate gate;
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
        clog<<"updating database..."<<endl;
        Gate gate;
        gate.update_db();
    }

    return 0;
}
