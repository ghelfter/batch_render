/**************************************************
 *                  Galen Helfter
 *               ghelfter@gmail.com
 *                  ssh_session.c
 **************************************************/

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h> /* Assumes a Unix system */
#include <termios.h>

#include "ssh_session.h"

#define USERNAME_BUFF_LEN 80

static void ssh_connection_chomp(char *buffer, int buffer_len, char delim);

void ssh_connection_print_error(int code)
{
    switch(code)
    {
        case SSH_SESSION_GOOD:
            fprintf(stdout, "No error.\n");
            break;
        case SSH_SESSION_BADALLOC:
            fprintf(stdout, "Failed memory allocation.\n");
            break;
        case SSH_SESSION_NULLPTR:
            fprintf(stdout, "Passed a NULL pointer as an argument.\n");
            break;
        case SSH_SESSION_TERM_FAIL:
            fprintf(stdout, "Terminal operation failed.\n");
            break;
        case SSH_SESSION_CONNECT_FAIL:
            fprintf(stdout, "Failed SSH connection to host.\n");
            break;
        case SSH_SESSION_NOT_KNOWN:
            fprintf(stdout, "Unknown host.\n");
            break;
        case SSH_SESSION_FAILED_AUTH:
            fprintf(stdout, "Failed authentication with host.\n");
            break;
        case SSH_SESSION_SCP_FAIL:
            fprintf(stdout, "Failed SCP operation.\n");
            break;
        default:
            fprintf(stdout, "Unknown error code.\n");
    };
}

int instantiate_ssh_connection(ssh_session *session, const char *hostname)
{
    int retcode = SSH_SESSION_GOOD;

    struct termios old_term, new_term;
    ssh_session internal_session;
    int retval = 0;
    char username[USERNAME_BUFF_LEN];
    char password[USERNAME_BUFF_LEN];

    if(session == NULL || hostname == NULL)
    {
        retcode = SSH_SESSION_NULLPTR;
        goto CLEANUP;
    }

    internal_session = ssh_new();

    if(internal_session == NULL)
    {
        retcode = SSH_SESSION_BADALLOC;
        goto CLEANUP;
    }

    /* Acquire username and password from caller */
    fprintf(stdout, "Username: ");
    fgets(username, USERNAME_BUFF_LEN, stdin);
    ssh_connection_chomp(username, USERNAME_BUFF_LEN, '\n');

    /* Clear terminal for password */
    if(tcgetattr(fileno(stdin), &old_term) != 0)
    {
        retcode = SSH_SESSION_TERM_FAIL;
        goto CLEANUP;
    }

    new_term = old_term;
    new_term.c_lflag &= ~ECHO;

    if(tcsetattr(fileno(stdin), TCSAFLUSH, &new_term) != 0)
    {
        retcode = SSH_SESSION_TERM_FAIL;
        goto CLEANUP;
    }

    fprintf(stdout, "Password: ");
    fgets(password, USERNAME_BUFF_LEN, stdin);
    ssh_connection_chomp(password, USERNAME_BUFF_LEN, '\n');
    fputc('\n', stdout);

    if(tcsetattr(fileno(stdin), TCSAFLUSH, &old_term) != 0)
    {
        retcode = SSH_SESSION_TERM_FAIL;
        goto CLEANUP;
    }

    /* Connect to the host with credentials */
    ssh_options_set(internal_session, SSH_OPTIONS_HOST, hostname);

    retval = ssh_connect(internal_session);
    if(retval != SSH_OK)
    {
        retcode = SSH_SESSION_CONNECT_FAIL;
        goto CLEANUP;
    }

    retval = ssh_userauth_password(internal_session, NULL, password);

    /* Clear password in memory */
    memset(password, 0x00, USERNAME_BUFF_LEN);

    if(retval != SSH_AUTH_SUCCESS)
    {
        retcode = SSH_SESSION_FAILED_AUTH;
        goto CLEANUP;
    }

CLEANUP:
    /* Clear password in memory again */
    memset(password, 0x00, USERNAME_BUFF_LEN);

    if(internal_session != NULL && retcode != SSH_SESSION_GOOD)
    {
        ssh_disconnect(internal_session);
        ssh_free(internal_session);
        internal_session = NULL;
    }

    if(retcode == SSH_SESSION_GOOD)
    {
        *session = internal_session;
    }

    return retcode;
}

int terminate_ssh_connection(ssh_session *session)
{
    int retcode = SSH_SESSION_GOOD;

    ssh_disconnect(*session);
    ssh_free(*session);

    *session = NULL;

    return retcode;
}

int scp_read_json_file(ssh_session *session, const char *filepath,
                       char **buffer, int *buffer_len)
{
    int retcode = SSH_SESSION_GOOD;
    int retval = 0;
    int size = 0;
    char *intern_buffer = NULL;
    ssh_scp scp_session = NULL;

    if(session == NULL || filepath == NULL || buffer == NULL
       || buffer_len == NULL)
    {
        retcode = SSH_SESSION_NULLPTR;
        goto CLEANUP;
    }

    /* Init SCP system */
    scp_session = ssh_scp_new(*session, SSH_SCP_READ, filepath);

    if(scp_session == NULL)
    {
        retcode = SSH_SESSION_BADALLOC;
        goto CLEANUP;
    }

    retval = ssh_scp_init(scp_session);
    if(retval != SSH_OK)
    {
        retcode = SSH_SESSION_SCP_FAIL;
        goto CLEANUP;
    }

    retval = ssh_scp_pull_request(scp_session);
    if(retval != SSH_SCP_REQUEST_NEWFILE)
    {
        retcode = SSH_SESSION_SCP_FAIL;
        goto CLEANUP;
    }

    size = ssh_scp_request_get_size(scp_session);
    if(size < 1)
    {
        retcode = SSH_SESSION_SCP_FAIL;
        goto CLEANUP;
    }

    intern_buffer = malloc(sizeof(char) * size);

    ssh_scp_accept_request(scp_session);
    retval = ssh_scp_read(scp_session, intern_buffer, size);

CLEANUP:
    if(intern_buffer != NULL)
    {
        if(retcode != SSH_SESSION_GOOD)
        {
            free(intern_buffer);
        }
        else
        {
            *buffer = intern_buffer;
            *buffer_len = size;
        }
    }
    /* Close and free the session */
    ssh_scp_close(scp_session);
    ssh_scp_free(scp_session);

    return retcode;
}

int scp_copy_directory(ssh_session *session, const char *dirpath,
                       const char *result_dirpath)
{
    int retval = SSH_SESSION_GOOD;

    return retval;
}

static void ssh_connection_chomp(char *buffer, int buffer_len, char delim)
{
    int len = strnlen(buffer, buffer_len);
    int m_off = (len-1) < 0 ? 0 : (len-1);

    if(*(buffer+m_off) == delim)
    {
        *(buffer+m_off) = '\0';
    }
}
