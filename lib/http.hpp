// SPDX-FileCopyrightText: 2022 Enrique M.G. <quiqueiii@gmail.com>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef LLX_GVA_GATE_HTTP
#define LLX_GVA_GATE_HTTP

#include <variant.hpp>

#include <ostream>
#include <sstream>
#include <string>
#include <map>
#include <cstdint>

namespace lliurex
{
    namespace http
    {
        class Response
        {
            public:

            uint64_t status;
            std::stringstream content;

            //Response(uint64_t status,std::stringstream content);
            edupals::variant::Variant parse();

        };

        class Client
        {
            protected:

            std::string server;

            public:

            Client(std::string url);

            Response get(std::string what);
            Response post(std::string what,std::map<std::string,std::string> fields);


        };
    }

}

#endif
