// SPDX-FileCopyrightText: 2022 Enrique M.G. <quiqueiii@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "libllxgvagate.hpp"

#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include <security/pam_ext.h>

#include <syslog.h>

#include <iostream>
#include <string>

using namespace lliurex;
using namespace std;

static void log(int priority,string message)
{
    syslog(priority,"%s",message.c_str());
}

PAM_EXTERN int pam_sm_setcred( pam_handle_t* pamh, int flags, int argc, const char** argv )
{
    syslog(LOG_DEBUG,"lliurex-gva-gate::pam_sm_setcred\n");
    return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_acct_mgmt(pam_handle_t* pamh, int flags, int argc, const char** argv)
{
    syslog(LOG_DEBUG,"lliurex-gva-gate::pam_sm_acct_mgmt\n");
    return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_authenticate( pam_handle_t* pamh, int flags,int argc, const char** argv )
{
    syslog(LOG_DEBUG,"lliurex-gva-gate::pam_sm_authenticate\n");

    int status;
    const char* service;
    const char* user;
    const char* tty;
    const char* password;

    status = pam_get_item(pamh, PAM_SERVICE, (const void**)(const void*)&service);

    if (status != PAM_SUCCESS) {
        syslog(LOG_ERR,"cannot retrieve service\n");
        return PAM_AUTH_ERR;
    }

    status = pam_get_user(pamh, &user, NULL);

    if (status != PAM_SUCCESS) {
        syslog(LOG_ERR,"cannot retrieve user\n");
        return PAM_AUTH_ERR;
    }

    status = pam_get_item(pamh, PAM_TTY,(const void **)(const void *)&tty);

    if (status != PAM_SUCCESS) {
        syslog(LOG_ERR,"cannot retrieve tty\n");
        return PAM_AUTH_ERR;
    }

    status = pam_get_authtok(pamh, PAM_AUTHTOK, &password , NULL);

    if (status != PAM_SUCCESS) {
        syslog(LOG_ERR,"cannot retrieve password\n");
        return PAM_AUTH_ERR;
    }

    try {
        syslog(LOG_INFO,"user:%s  tty:%s  service:%s\n",user,tty,service);
        Gate gate(log);
        gate.open();

        if (gate.authenticate(string(user),string(password))) {
            syslog(LOG_INFO,"User authenticated\n");
            return PAM_SUCCESS;
        }
        else {
            syslog(LOG_ERR,"Failed to login\n");
            return PAM_AUTH_ERR;
        }
    }
    catch(std::exception& e) {
        syslog(LOG_ERR,"%s\n",e.what());
        return PAM_AUTH_ERR;
    }

    return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_open_session(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
    syslog(LOG_DEBUG,"lliurex-gva-gate::pam_sm_open_session\n");
    return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_close_session(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
    syslog(LOG_DEBUG,"lliurex-gva-gate::pam_sm_close_session\n");
    return PAM_SUCCESS;
}
