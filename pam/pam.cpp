// SPDX-FileCopyrightText: 2022 Enrique M.G. <quiqueiii@gmail.com>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "libllxgvagate.hpp"

#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include <security/pam_ext.h>

#include <syslog.h>
#include <unistd.h>
#include <sys/wait.h>

#include <iostream>
#include <string>

using namespace lliurex;
using namespace std;

static void log(int priority,string message)
{
    syslog(priority,"%s",message.c_str());
}

static void cleanup (pam_handle_t* pamh,void* data,int error_status)
{
    free(data);
}

PAM_EXTERN int pam_sm_setcred( pam_handle_t* pamh, int flags, int argc, const char** argv )
{
    pam_syslog(pamh,LOG_DEBUG,"pam_sm_setcred(%d)\n",flags);

    if (flags & PAM_ESTABLISH_CRED) {
        pam_syslog(pamh,LOG_DEBUG,"PAM_ESTABLISH_CRED\n");
    }

    if (flags & PAM_DELETE_CRED) {
        pam_syslog(pamh,LOG_DEBUG,"PAM_DELETE_CRED\n");
    }

    if (flags & PAM_REINITIALIZE_CRED) {
        pam_syslog(pamh,LOG_DEBUG,"PAM_REINITIALIZE_CRED\n");
    }

    if (flags & PAM_REFRESH_CRED) {
        pam_syslog(pamh,LOG_DEBUG,"PAM_REFRESH_CRED\n");
    }

    return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_authenticate( pam_handle_t* pamh, int flags,int argc, const char** argv )
{
    pam_syslog(pamh,LOG_DEBUG,"pam_sm_authenticate\n");

    int status;
    int chkpwd = -1;
    const char* service;
    const char* user;
    const char* tty;
    const char* password;

    status = pam_get_item(pamh, PAM_SERVICE, (const void**)(const void*)&service);

    if (status != PAM_SUCCESS) {
        pam_syslog(pamh,LOG_ERR,"cannot retrieve service\n");
        return PAM_AUTH_ERR;
    }

    status = pam_get_user(pamh, &user, NULL);

    if (status != PAM_SUCCESS) {
        pam_syslog(pamh,LOG_ERR,"cannot retrieve user\n");
        return PAM_AUTH_ERR;
    }

    status = pam_get_item(pamh, PAM_TTY,(const void **)(const void *)&tty);

    if (status != PAM_SUCCESS) {
        pam_syslog(pamh,LOG_ERR,"cannot retrieve tty\n");
        return PAM_AUTH_ERR;
    }

    status = pam_get_authtok(pamh, PAM_AUTHTOK, &password , NULL);

    if (status != PAM_SUCCESS) {
        pam_syslog(pamh,LOG_ERR,"cannot retrieve password\n");
        return PAM_AUTH_ERR;
    }

    try {
        pam_syslog(pamh,LOG_INFO,"user:%s  tty:%s  service:%s\n",user,tty,service);

        if (geteuid() == 0) {
            Gate gate(log);
            gate.create_db();
            /*
            if(!gate.open()) {
                pam_syslog(pamh,LOG_ERR,"Can't access gate databases\n");
                return PAM_AUTH_ERR;
            }
            */
            // loads config: server address, auth_mode
            gate.load_config();

            chkpwd = gate.authenticate(user,password);
            pam_syslog(pamh,LOG_INFO,"User %s authentication returned %d\n",user,chkpwd);

        }
        else {

            pid_t child;

            child = fork();

            if (child == 0) {
                // child
                execl("/bin/llx-gva-gate","chkpwd",user,password,(char*)0);

                pam_syslog(pamh,LOG_ERR,"Failed to spawn llx-gva-gate process\n");
                return PAM_AUTH_ERR;
            }
            else {
                // parent

                pid_t pid = waitpid(child,&chkpwd,0);

                if (WIFEXITED(chkpwd) == 0) {
                    pam_syslog(pamh,LOG_ERR,"Something went wrong on llx-gva-gate \n");
                    return PAM_AUTH_ERR;
                }

                chkpwd = WEXITSTATUS(chkpwd);
            }

        }

        pam_syslog(pamh,LOG_INFO,"local look-up:%d\n",chkpwd);

        void* data = malloc(sizeof(int));
        *((int*)data) = chkpwd;
        pam_set_data(pamh,"llxgvagate.auth.status",data,cleanup);

        switch (chkpwd) {
            case Gate::Allowed:
            case Gate::ExpiredPassword:
            //case Gate::UserNotAllowed:
                return PAM_SUCCESS;
            break;

            case Gate::UserNotFound:
                return PAM_USER_UNKNOWN;
            break;

            case Gate::InvalidPassword:
                return PAM_AUTH_ERR;
            break;

            default:
                return PAM_AUTH_ERR;
        }

    }
    catch(std::exception& e) {
        syslog(LOG_ERR,"%s\n",e.what());
        return PAM_AUTH_ERR;
    }

    return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_acct_mgmt(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
    pam_syslog(pamh,LOG_DEBUG,"pam_sm_acct_mgmt\n");
    int status;
    const char* user;
    const void* data;

    status = pam_get_user(pamh, &user, NULL);

    if (status != PAM_SUCCESS) {
        pam_syslog(pamh,LOG_ERR,"cannot retrieve user\n");
        return PAM_AUTH_ERR;
    }

    status = pam_get_data(pamh,"llxgvagate.auth.status",&data);

    if (status == PAM_SUCCESS) {
        status = *((int *)data);
        pam_syslog(pamh,LOG_INFO,"Status:%d\n",status);

        if (status == Gate::ExpiredPassword) {
            pam_syslog(pamh,LOG_INFO,"Password for %s has expired\n",user);
            pam_info(pamh,"Password has expired\n");

            return PAM_ACCT_EXPIRED;
        }

        /*
        if (status == Gate::UserNotAllowed) {
            pam_syslog(pamh,LOG_INFO,"User %s is not allowed\n",user);
            pam_info(pamh,"User is not allowed to login\n");

            return PAM_PERM_DENIED;
        }
        */
    }

    pam_info(pamh,"Welcome to GVA\n");
    pam_syslog(pamh,LOG_INFO,"Granting access to %s\n",user);
    return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_open_session(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
    pam_syslog(pamh,LOG_DEBUG,"lliurex-gva-gate::pam_sm_open_session\n");
    return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_close_session(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
    pam_syslog(pamh,LOG_DEBUG,"lliurex-gva-gate::pam_sm_close_session\n");
    return PAM_SUCCESS;
}
