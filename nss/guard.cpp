// SPDX-FileCopyrightText: 2026 Enrique M.G. <quique@necos.es>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "common.hpp"

#include <process.hpp>
#include <variant.hpp>

#include <nss.h>
#include <grp.h>
#include <pwd.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <syslog.h>

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

extern "C" enum nss_status _nss_llxgvaguard_gethostbyname_r (const char *name, struct hostent *result,
        char *buffer, size_t buflen, int *errnop, int *h_errnop);

extern "C" enum nss_status _nss_llxgvaguard_gethostbyname2_r (const char *name, int af, struct hostent *result,
        char *buffer, size_t buflen, int *errnop, int *h_errnop);

extern "C" enum nss_status _nss_llxgvaguard_gethostbyname3_r (const char *name, int af, struct hostent *result,
        char *buffer, size_t buflen, int *errnop, int *h_errnop, int32_t *ttlp, char **canonp);

enum nss_status _nss_llxgvaguard_gethostbyname3_r (const char *name, int af, struct hostent *result,
        char *buffer, size_t buflen, int *errnop, int *h_errnop, int32_t *ttlp, char **canonp)
{
    syslog(LOG_INFO,"DNS gethostbyname3:%s\n",name);

    return NSS_STATUS_NOTFOUND;
}

enum nss_status _nss_llxgvaguard_gethostbyname2_r (const char *name, int af, struct hostent *result,
        char *buffer, size_t buflen, int *errnop, int *h_errnop)
{
    syslog(LOG_INFO,"DNS gethostbyname2:%s\n",name);

    return _nss_llxgvaguard_gethostbyname3_r (name, af, result, buffer, buflen, errnop, h_errnop, NULL, NULL);
}

enum nss_status _nss_llxgvaguard_gethostbyname_r (const char *name, struct hostent *result,
        char *buffer, size_t buflen, int *errnop, int *h_errnop)
{
    syslog(LOG_INFO,"DNS gethostbyname\n");
}
