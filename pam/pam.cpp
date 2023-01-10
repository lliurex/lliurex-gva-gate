// SPDX-FileCopyrightText: 2022 Enrique M.G. <quiqueiii@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "libllxgvagate.hpp"

#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include <security/pam_ext.h>

#include <iostream>

using namespace std;

PAM_EXTERN int pam_sm_setcred( pam_handle_t* pamh, int flags, int argc, const char** argv )
{
    clog<<"lliurex-gva-gate::pam_sm_setcred"<<endl;
    return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_acct_mgmt(pam_handle_t* pamh, int flags, int argc, const char** argv)
{
    clog<<"lliurex-gva-gate::pam_sm_acct_mgmt"<<endl;
    return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_authenticate( pam_handle_t* pamh, int flags,int argc, const char** argv )
{
    clog<<"lliurex-gva-gate::pam_sm_authenticate"<<endl;

    int status;
    const char* service;
    const char* user;
    const char* tty;
    const char* password;

    status = pam_get_item(pamh, PAM_SERVICE, (const void**)(const void*)&service);

    if (status != PAM_SUCCESS) {
        clog<<"cannot retrieve service"<<endl;
        return PAM_AUTH_ERR;
    }

    status = pam_get_user(pamh, &user, NULL);

    if (status != PAM_SUCCESS) {
        clog<<"cannot retrieve user"<<endl;
        return PAM_AUTH_ERR;
    }

    status = pam_get_item(pamh, PAM_TTY,(const void **)(const void *)&tty);

    if (status != PAM_SUCCESS) {
        clog<<"cannot retrieve tty"<<endl;
        return PAM_AUTH_ERR;
    }

    status = pam_get_authtok(pamh, PAM_AUTHTOK, &password , NULL);

    if (status != PAM_SUCCESS) {
        clog<<"cannot retrieve password"<<endl;
        return PAM_AUTH_ERR;
    }

    if (string(user) == "quique") {
        return PAM_SUCCESS;
    }

    return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_open_session(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
    return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_close_session(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
    return PAM_SUCCESS;
}
