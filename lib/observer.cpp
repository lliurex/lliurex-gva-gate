// SPDX-FileCopyrightText: 2025 Enrique M.G. <quique@necos.es>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "observer.hpp"

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <climits>
#include <stdexcept>

#define GVA_GATE_SHARED "/net.lliurex.gvagate.db"

using namespace lliurex;
using namespace std;

Observer::Observer() : current(UINT_MAX), counter_ptr(nullptr)
{
    open();
}

Observer::~Observer()
{
    if (counter_ptr) {
        munmap(counter_ptr, 4);
    }
}

void Observer::open()
{
    int fd = shm_open(GVA_GATE_SHARED, O_RDONLY, S_IRUSR);

    if(fd < 0) {

        if (errno == ENOENT) {
            // this may happen
            return;
        }
        throw runtime_error("Failed to open shared memory object");
    }

    uint32_t *ptr = (uint32_t *) mmap(nullptr, 4, PROT_READ, MAP_SHARED, fd, 0);

    if (!ptr) {
        close(fd);
        throw runtime_error("Failed to map shared memory");
    }

    counter_ptr = ptr;

    close(fd);
}

bool Observer::changed()
{
    if (!counter_ptr) {
        open();
    }

    if (counter_ptr) {
        if (*counter_ptr != current) {
            current = *counter_ptr;

            return true;
        }
    }
    else {

        /* generate a first time change response */
        if (current == UINT_MAX) {
            current = 0;

            return true;
        }
    }

    return false;
}

void Observer::create()
{
    int fd = shm_open(GVA_GATE_SHARED, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

    if(fd < 0) {
        throw runtime_error("Failed to create or open a shared memory");
    }

    ftruncate(fd, 4);

    close(fd);
}

void Observer::push()
{
    int fd = shm_open(GVA_GATE_SHARED, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

    if(fd < 0) {
        throw runtime_error("Failed to create or open a shared memory");
    }

    ftruncate(fd, 4);

    uint32_t *ptr = (uint32_t *) mmap(nullptr, 4, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

    if (!ptr) {
        close(fd);
        throw runtime_error("Failed to map shared memory");
    }

    *ptr = *ptr + 1;

    close(fd);

    munmap(ptr,4);
}
