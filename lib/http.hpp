// SPDX-FileCopyrightText: 2022 Enrique M.G. <quiqueiii@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LLX_GVA_GATE_HTTP
#define LLX_GVA_GATE_HTTP

#include <variant.hpp>

#include <ostream>
#include <string>

namespace lliurex
{
    class HttpClient
    {
        public:

        HttpClient(std::string url);

        edupals::variant::Variant request(std::string what);

        std::string server;
    };


}

#endif
