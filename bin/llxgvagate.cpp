// SPDX-FileCopyrightText: 2022 Enrique M.G. <quiqueiii@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "libllxgvagate.hpp"

#include <iostream>

using namespace lliurex;

using namespace std;

int main(int argc,char* argv[])
{
    clog<<"LliureX GVA Gate cli tool"<<endl;

    Gate gate;

    clog<<"database:"<<gate.exists_db()<<endl;
    gate.create_db();

    return 0;
}
