// SPDX-FileCopyrightText: 2022 Enrique M.G. <quiqueiii@gmail.com>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "libllxgvagate.hpp"
#include "observer.hpp"

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

int global_auth_status;

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
    string service;
    string user;
    string password;
    const char* tmpstr;

    edupals::variant::Variant user_passwd;
    bool external = false;

    status = pam_get_item(pamh, PAM_SERVICE, (const void**)(const void*)&tmpstr);

    if (status != PAM_SUCCESS) {
        pam_syslog(pamh,LOG_ERR,"Cannot retrieve service\n");
        return PAM_AUTH_ERR;
    }
    else {
        service = tmpstr;
        pam_syslog(pamh,LOG_INFO,"service:%s\n",service.c_str());
    }

    status = pam_get_user(pamh, &tmpstr, NULL);

    if (status != PAM_SUCCESS) {
        pam_syslog(pamh,LOG_ERR,"Cannot retrieve user\n");
        return PAM_AUTH_ERR;
    }
    else {
        user = tmpstr;
        pam_syslog(pamh,LOG_INFO,"user:%s\n",user.c_str());
    }

    status = pam_get_authtok(pamh, PAM_AUTHTOK, &tmpstr , NULL);

    if (status != PAM_SUCCESS) {
        pam_syslog(pamh,LOG_ERR,"Cannot retrieve password\n");
        return PAM_AUTH_ERR;
    }
    else {
        password = tmpstr;
    }

    try {
        pam_syslog(pamh,LOG_INFO,"user:%s service:%s\n",user.c_str(),service.c_str());

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

            chkpwd = gate.authenticate(user,password, user_passwd);
            pam_syslog(pamh,LOG_INFO,"User %s authentication returned %d\n",user.c_str(),chkpwd);

        }
        else {

            external = true;

            pid_t child;

            child = fork();

            if (child == 0) {
                // child
                execl("/bin/llx-gva-gate","/bin/llx-gva-gate","chkpwd",user.c_str(),password.c_str(),(char*)0);

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

        global_auth_status = chkpwd;
        void* data = &global_auth_status;
        pam_set_data(pamh,"llxgvagate.auth.status",data,cleanup);

        switch (chkpwd) {
            case Gate::Allowed:
            case Gate::ExpiredPassword:
            //case Gate::UserNotAllowed:

                if (!external) {
                    if (user_passwd["user"]["login"].get_string() != user) {
                        string muser = user_passwd["user"]["login"].get_string();

                        pam_syslog(pamh, LOG_INFO, "Remapping user from %s to %s\n", user.c_str(), muser.c_str());
                        pam_set_item(pamh, PAM_USER, (const void*)muser.c_str());
                    }
                }

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

    /* initialize database shared memory counter but do not return error if fail */
    if (geteuid() == 0) {
        try {
            Observer::create();
        }
        catch(std::exception& e) {
            syslog(LOG_ERR,"%s\n",e.what());
        }
    }

    return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_acct_mgmt(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
    pam_syslog(pamh,LOG_DEBUG,"pam_sm_acct_mgmt\n");
    int status;
    string user;
    const void* data;
    string service;
    const char* tmpstr;

    status = pam_get_user(pamh, &tmpstr, NULL);

    if (status != PAM_SUCCESS) {
        pam_syslog(pamh,LOG_ERR,"Cannot retrieve user\n");
        return PAM_AUTH_ERR;
    }
    else {
        user = tmpstr;
    }

    status = pam_get_data(pamh,"llxgvagate.auth.status",&data);

    if (status == PAM_SUCCESS) {
        status = *((int *)data);
        pam_syslog(pamh,LOG_INFO,"Acct status:%d\n",status);

        if (status == Gate::ExpiredPassword) {
            pam_syslog(pamh,LOG_INFO,"Password for %s has expired\n",user.c_str());
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
    else {
        pam_syslog(pamh,LOG_ERR,"Failed to retrieve auth.status key\n");
    }

    status = pam_get_item(pamh, PAM_SERVICE, (const void**)(const void*)&tmpstr);

    if (status == PAM_SUCCESS) {
        service = tmpstr;

        if (service != "sudo") {
            pam_info(pamh,"Welcome to GVA\n");
        }
    }

    pam_syslog(pamh,LOG_INFO,"Granting access to %s\n",user.c_str());
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
