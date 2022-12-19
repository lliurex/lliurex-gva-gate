// SPDX-FileCopyrightText: 2022 Enrique M.G. <quiqueiii@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <security/pam_appl.h>
#include <security/pam_modules.h>

extern "C" int pam_sm_setcred( pam_handle_t* pamh, int flags, int argc, const char** argv );
extern "C" int pam_sm_acct_mgmt(pam_handle_t* pamh, int flags, int argc, const char** argv);
extern "C" int pam_sm_authenticate( pam_handle_t* pamh, int flags,int argc, const char** argv );

int pam_sm_setcred( pam_handle_t* pamh, int flags, int argc, const char** argv )
{

}

int pam_sm_acct_mgmt(pam_handle_t* pamh, int flags, int argc, const char** argv)
{

}

int pam_sm_authenticate( pam_handle_t* pamh, int flags,int argc, const char** argv )
{

}
