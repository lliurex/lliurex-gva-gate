// SPDX-FileCopyrightText: 2025 Enrique M.G. <quique@necos.es>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef LLX_GVA_GATE_OBSERVER
#define LLX_GVA_GATE_OBSERVER

#include <cstdint>
#include <string>

namespace lliurex
{
    class Observer
    {
        private:

        uint32_t current;
        uint32_t* counter_ptr;

        void open();

        public:

        Observer();
        virtual ~Observer();

        uint32_t value() const
        {
            return current;
        }

        bool changed();

        static void create();
        static void push();
    };
}

#endif
