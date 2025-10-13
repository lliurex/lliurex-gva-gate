// SPDX-FileCopyrightText: 2025 Enrique M.G. <quique@necos.es>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "observer.hpp"

using namespace lliurex;
using namespace std;

Observer::Observer() : current(0)
{

}

Observer::~Observer()
{

}

bool Observer::changed()
{
    return false;
}

void Observer::push()
{

}
