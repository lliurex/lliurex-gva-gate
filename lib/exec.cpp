// SPDX-FileCopyrightText: 2022 Enrique M.G. <quique@necos.es>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "exec.hpp"
#include "libllxgvagate.hpp"

#include <process.hpp>
#include <json.hpp>

#include <unistd.h>

#include <sstream>

#define LIB_EXEC_PATH "/usr/lib/gva-gate/libgva"

using namespace lliurex;
using namespace edupals;
using namespace edupals::variant;
using namespace edupals::system;
using namespace std;

Exec::Exec(string runtime):runtime(runtime)
{
}

Variant Exec::run(string user, string password)
{
    Variant response = Variant::create_struct();
    response["status"] = Gate::Error;

    string filename = LIB_EXEC_PATH;
    stringstream data;
    stringstream args;
    int out_fd;
    int in_fd;
    int status;

    args<<user<<" "<<password<<" "<<runtime<<"\n";

    try {
        Process child = Process::spawn(filename,{},&out_fd,&in_fd,nullptr);

        for (char c:args.str()) {
            write(in_fd,&c,1);
        }
        close(in_fd);

        char buffer;
        while (read(out_fd,&buffer,1) > 0) {
            data<<buffer;
        }
        close(out_fd);
        status = child.wait();

        response["status"] = status;
        if (status == Gate::Allowed) {
            Variant exec_response = json::load(data);
            response["response"] = exec_response;
        }
    }
    catch (std::exception& e) {
        throw runtime_error(e.what());
    }

    return response;
}
