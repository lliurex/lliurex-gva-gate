// SPDX-FileCopyrightText: 2022 Enrique M.G. <quique@necos.es>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef LLX_GVA_GATE_EXEC
#define LLX_GVA_GATE_EXEC

#include <variant.hpp>

#include <ostream>
#include <sstream>
#include <string>
#include <map>
#include <cstdint>

namespace lliurex
{
    class Exec
    {
        public:

        Exec(std::string runtime);

        edupals::variant::Variant run(std::string user,std::string password);

        protected:

        std::string runtime;
    };
}

#endif
