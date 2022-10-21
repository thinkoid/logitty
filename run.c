/* -*- mode: c; -*- */

#include <assert.h>
#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <utmp.h>

#include <grp.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <security/pam_appl.h>

#include "run.h"

#define UNUSED(x) ((void)(x))

typedef int(*pam_action_t)(struct pam_handle *, int);

static int
pam_conv(int n,
        const struct pam_message **msg,
        struct pam_response **reply,
        void *data)
{
        int i, ok;
        char *username, *password;

        assert(msg);
        assert(reply);

        *reply = malloc(n * sizeof *reply);
        if (0 == *reply)
                return PAM_BUF_ERR;

        memset(*reply, 0, n * sizeof *reply);

        ok = PAM_SUCCESS;
        for(i = 0; i < n; ++i) {
                switch(msg[i]->msg_style) {
                case PAM_PROMPT_ECHO_ON:
                        username = ((char **)data)[0];
                        (*reply)[i].resp = strdup(username);
                        break;

                case PAM_PROMPT_ECHO_OFF:
                        password = ((char **)data)[1];
                        (*reply)[i].resp = strdup(password);
                        break;

                case PAM_ERROR_MSG:
                        ok = PAM_CONV_ERR;
                        break;

                default:
                        break;
                }

                if (PAM_SUCCESS != ok)
                        break;
        }

        if (PAM_SUCCESS != ok) {
                for (i = 0; i < n; ++i) {
                        void **p = (void **)&(*reply)[i].resp;
                        if (*p) {
                                free(*p);
                                *p = 0;
                        }
                }

                free(*reply);
                *reply = 0;
        }

        return ok;
}

static const char *
pam_diag(size_t n)
{
        static const char *arr[] = {
                "success", /* PAM_SUCCESS 0 */
                "dlopen() failure", /* PAM_OPEN_ERR 1 */
                "symbol not found", /* PAM_SYMBOL_ERR 2 */
                "error in service module", /* PAM_SERVICE_ERR 3 */
                "system error", /* PAM_SYSTEM_ERR 4 */
                "memory buffer error", /* PAM_BUF_ERR 5 */
                "permission denied", /* PAM_PERM_DENIED 6 */
                "authentication failure", /* PAM_AUTH_ERR 7 */
                "cannot access authentication data", /* PAM_CRED_INSUFFICIENT 8 */
                "authentication info unavailable", /* PAM_AUTHINFO_UNAVAIL 9 */
                "user not known", /* PAM_USER_UNKNOWN 10 */
                "max retries", /* PAM_MAXTRIES 11 */
                "new authentication token required.", /* PAM_NEW_AUTHTOK_REQD 12 */
                "user account has expired", /* PAM_ACCT_EXPIRED 13 */
                "session error", /* PAM_SESSION_ERR 14 */
                "credentials unavailable", /* PAM_CRED_UNAVAIL 15 */
                "credentials expired", /* PAM_CRED_EXPIRED 16 */
                "credentials error", /* PAM_CRED_ERR 17 */
                "no module specific data", /* PAM_NO_MODULE_DATA 18 */
                "conversation error", /* PAM_CONV_ERR 19 */
                "authentication token error", /* PAM_AUTHTOK_ERR 20 */
                "authentication token recovery error", /* PAM_AUTHTOK_RECOVERY_ERR 21 */
                "authentication token lock busy", /* PAM_AUTHTOK_LOCK_BUSY 22 */
                "authentication token aging disabled", /* PAM_AUTHTOK_DISABLE_AGING 23 */
                "error try again", /* PAM_TRY_AGAIN 24 */
                "ignore underlying account module", /* PAM_IGNORE 25 */
                "critical error", /* PAM_ABORT 26 */
                "authentication token expired", /* PAM_AUTHTOK_EXPIRED 27 */
                "module unknown", /* PAM_MODULE_UNKNOWN 28 */
                "PAM bad item", /* PAM_BAD_ITEM 29 */
                "conversation function not avail", /* PAM_CONV_AGAIN 30 */
                "PAM incomplete", /* PAM_INCOMPLETE 31 */
        };

        if (n < sizeof arr / sizeof *arr)
                return arr[n];

        return "unknown PAM error";
}

static int
do_setup_env(const char *var, const char *value, int overwrite, int strict)
{
        if (setenv(var, value, overwrite)) {
                if (strict) {
                        fprintf(stderr, "failed to setenv %s=%s\n", var, value);
                        return 1;
                }
        }

        return 0;
}

static int
setup_env_bare(const struct passwd* pwd)
{
        const char *s;

        s = getenv("TERM");
        if (0 == s || 0 == s[0])
                s = "linux";

        if (do_setup_env("TERM", s, 1, 1))
                return 1;

        s = getenv("LANG");
        if (0 == s || 0 == s[0])
                s = "C";

        if (do_setup_env("LANG", s, 1, 1))
                return 1;

        if (do_setup_env("HOME",    pwd->pw_dir,   1, 1) ||
            do_setup_env("PWD",     pwd->pw_dir,   1, 1) ||
            do_setup_env("SHELL",   pwd->pw_shell, 1, 1) ||
            do_setup_env("USER",    pwd->pw_name,  1, 1) ||
            do_setup_env("LOGNAME", pwd->pw_name,  1, 1)) {
                return 1;
        }

        return do_setup_env("PATH", "/usr/local/sbin:/usr/bin:/bin", 1, 1);
}

static int
setup_env_pam(char **envs)
{
        for (char** env = envs; *env; ++env) {
                if (putenv(*env)) {
                        fprintf(stderr, "putenv fail : %s\n", strerror(errno));
                        return 1;
                }
        }

        return 0;
}

static struct pam_handle *
setup_pam(const char *username, const char *password)
{
        struct pam_handle *pamh = 0;

        const char* creds[] = { username, password };
        struct pam_conv pamc = { pam_conv, creds };

        int status;

        if (PAM_SUCCESS != (status = pam_start("tui", 0, &pamc, &pamh)) ||
            PAM_SUCCESS != (status = pam_authenticate(pamh, 0)) ||
            PAM_SUCCESS != (status = pam_acct_mgmt(pamh, 0)) ||
            PAM_SUCCESS != (status = pam_setcred(pamh, PAM_ESTABLISH_CRED))) {
                fprintf(stderr, "PAM : %s\n", pam_diag(status));
                goto err;
        }

        if (PAM_SUCCESS != (status = pam_open_session(pamh, 0))) {
                fprintf(stderr, "PAM : %s\n", pam_diag(status));
                goto nosession;
        }

        return pamh;

nosession:
        if (PAM_SUCCESS != (status = pam_setcred(pamh, PAM_DELETE_CRED))) {
                fprintf(stderr, "PAM : %s\n", pam_diag(status));
        }

err:
        if (pamh) {
                if (PAM_SUCCESS != (status = pam_end(pamh, 0))) {
                        fprintf(stderr, "PAM : %s\n", pam_diag(status));
                }
        }

        return 0;
}

static int
destroy_pam(struct pam_handle *pamh)
{
        int status;

        if (0 == pamh)
                return 0;

        if (PAM_SUCCESS != (status = pam_close_session(pamh, 0)) ||
            PAM_SUCCESS != (status = pam_setcred(pamh, PAM_DELETE_CRED)) ||
            PAM_SUCCESS != (status = pam_end(pamh, 0))) {
                fprintf(stderr, "PAM : %s\n", pam_diag(status));
                return 1;
        }

        return 0;
}

static int
register_utmp(struct utmp *p, const char *username, pid_t pid)
{
        const char *s;
        int ret = 1;

	p->ut_type = USER_PROCESS;
	p->ut_pid = pid;

        s = ttyname(STDIN_FILENO);
        if (sizeof(p->ut_line) < strlen(s) - strlen("/dev/") + 1) {
                fprintf(stderr, "insufficient space (utmp::ut_line)\n");
                return ret;
        }
	strcpy(p->ut_line, s + strlen("/dev/"));

        if (sizeof(p->ut_id) < strlen(s) - strlen("/dev/tty") + 1) {
                fprintf(stderr, "insufficient space (utmp::ut_id)\n");
                return ret;
        }
	strcpy(p->ut_id, ttyname(STDIN_FILENO) + strlen("/dev/tty"));

	time((long int *)&p->ut_time);

        if (sizeof(p->ut_user) < strlen(username) + 1) {
                fprintf(stderr, "insufficient space (utmp::ut_user)\n");
                return ret;
        }
	strncpy(p->ut_user, username, UT_NAMESIZE);

	memset(p->ut_host, 0, UT_HOSTSIZE);
	p->ut_addr = 0;

	setutent();
	if ((ret = !pututline(p)))
                fprintf(stderr, "pututline fail : %s\n", strerror(errno));
        endutent();

        return ret;
}

static int
unregister_utmp(struct utmp *p)
{
        int ret;

	p->ut_type = DEAD_PROCESS;
	memset(p->ut_line, 0, UT_LINESIZE);

	p->ut_time = 0;
	memset(p->ut_user, 0, UT_NAMESIZE);

        setutent();
	if ((ret = !pututline(p)))
                fprintf(stderr, "pututline fail : %s\n", strerror(errno));
	endutent();

        return ret;
}

static int
do_run(struct passwd *passwd, char **argv, char **envs)
{
        int pid, ret, status;
        struct utmp utmpent;

        UNUSED(argv);

        if (0 == (pid = fork())) {
                if (initgroups(passwd->pw_name, passwd->pw_gid)) {
                        fprintf(stderr, "initgroups : %s\n", strerror(errno));
                        return 1;
                }

                if (setgid(passwd->pw_gid) || seteuid(passwd->pw_uid)) {
                        fprintf(stderr, "setup uid, gid : %s\n", strerror(errno));
                        return 1;
                }

                if (setup_env_bare(passwd) || setup_env_pam(envs))
                        exit(1);

                if (chdir(passwd->pw_dir)) {
                        fprintf(stderr, "cd error : %s\n", strerror(errno));
                        exit(1);
                }

                /* exec some */
                execvp(argv[0], argv);

                exit(0);
        }

        ret = register_utmp(&utmpent, passwd->pw_name, pid);
        waitpid(pid, &status, 0);

        if (0 == ret)
                unregister_utmp(&utmpent);

        return status;
}

/**********************************************************************/

int run(const char *username, char *password, char **argv)
{
        struct passwd *passwd;
        struct pam_handle *pamh;

        passwd = getpwnam(username);
        if (0 == passwd || 0 == passwd->pw_shell || 0 == *passwd->pw_shell) {
                fprintf(stderr, "getpwnam error : %s\n", strerror(errno));
                return 1;
        }

        if (0 == (pamh = setup_pam(username, password)))
                return 1;

        memset(password, 0, strlen(password));

        int status = do_run(passwd, argv, pam_getenvlist(pamh));
        destroy_pam(pamh);

        return status;
}
