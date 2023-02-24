// SPDX-FileCopyrightText: 2022 Enrique M.G. <quiqueiii@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LLX_GVA_GATE_HASH
#define LLX_GVA_GATE_HASH

#include <string>
#include <vector>
#include <cstdint>
#include <iostream>

namespace lliurex
{
    namespace hash
    {
        std::vector<uint8_t> sha1(std::string input);

        std::ostream& operator<<(std::ostream& os,std::vector<uint8_t> value);

    }
}

#endif
