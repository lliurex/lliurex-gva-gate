// SPDX-FileCopyrightText: 2022 Enrique M.G. <quiqueiii@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <nss.h>
#include <grp.h>

#include <cstddef>

extern "C" enum nss_status _nss_gvagate_setgrent(void);
extern "C" enum nss_status _nss_gvagate_endgrent(void);
extern "C" enum nss_status _nss_gvagate_getgrent_r(struct group* result, char* buffer, size_t buflen, int* errnop);
extern "C" enum nss_status _nss_gvagate_getgrgid_r(gid_t gid, struct group* result, char* buffer, size_t buflen, int* errnop);
extern "C" enum nss_status _nss_gvagate_getgrnam_r(const char* name, struct group* result, char *buffer, size_t buflen, int* errnop);

nss_status _nss_gvagate_setgrent(void)
{

}

nss_status _nss_gvagate_endgrent(void)
{

}

nss_status _nss_gvagate_getgrent_r(struct group* result, char* buffer, size_t buflen, int* errnop)
{

}

nss_status _nss_gvagate_getgrgid_r(gid_t gid, struct group* result, char* buffer, size_t buflen, int* errnop)
{

}

nss_status _nss_gvagate_getgrnam_r(const char* name, struct group* result, char *buffer, size_t buflen, int* errnop)
{

}
