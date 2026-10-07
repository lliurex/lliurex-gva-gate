// SPDX-FileCopyrightText: 2026 Enrique M.G. <quique@necos.es>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "common.hpp"

#include <process.hpp>

#include <nss.h>
#include <grp.h>
#include <pwd.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>

#include <cstdint>
#include <cstdlib>
#include <cstddef>
#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <mutex>
#include <chrono>
#include <functional>
#include <map>

using namespace edupals;
using namespace edupals::variant;
using namespace edupals::system;

enum nss_status _nss_llxgvaguard_gethostbyname_r(
    const char *name, struct hostent *result,
    char *buffer, size_t buflen, int *errnop, int *h_errnop)
{
    syslog(LOG_INFO, "DNS lookup attempt: %s", name);

    /*
    *errnop = ENOENT;
    *h_errnop = HOST_NOT_FOUND;
    return NSS_STATUS_NOTFOUND;
    */

    return NSS_STATUS_SUCCESS;
}
