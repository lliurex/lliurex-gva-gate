// SPDX-FileCopyrightText: 2025 Enrique M.G. <quique@necos.es>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "observer.hpp"

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define GVA_GATE_SHARED "/net.lliurex.gvagate.db"

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
    int fd = shm_open(GVA_GATE_SHARED, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

    if(fd < 0) {
        // boom
    }

    ftruncate(fd, 4);

    uint32_t *ptr = (uint32_t *) mmap(nullptr, 4, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

    if (!ptr) {
        // boom
    }

    *ptr = *ptr + 1;

    close(fd);

    munmap(ptr,4);
}
