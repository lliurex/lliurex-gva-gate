// SPDX-FileCopyrightText: 2022 Enrique M.G. <quiqueiii@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LLX_GVA_GATE
#define LLX_GVA_GATE

#define LLX_GVA_GATE_DB "/tmp/llx-gva-gate.db"

namespace lliurex
{
    class Gate
    {

        public:

        bool exists_db();
        void create_db();
        void lock_db();
        void unlock_db();
    };
}

#endif
