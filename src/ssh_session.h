/**************************************************
 *                  Galen Helfter
 *               ghelfter@gmail.com
 *                  ssh_session.h
 **************************************************/

#ifndef _SSH_SESSION_H_
#define _SSH_SESSION_H_

#include <libssh/libssh.h>

/* Return codes */
#define SSH_SESSION_GOOD         0 /* Successful connection */
#define SSH_SESSION_BADALLOC     1 /* Failed memory allocation */
#define SSH_SESSION_NULLPTR      2 /* Passed a NULL pointer as argument */
#define SSH_SESSION_TERM_FAIL    3 /* Terminal failure */
#define SSH_SESSION_CONNECT_FAIL 4 /* Failed SSH connection  */
#define SSH_SESSION_NOT_KNOWN    5 /* Unknown host */
#define SSH_SESSION_FAILED_AUTH  6 /* Failed authentication */
#define SSH_SESSION_SCP_FAIL     7 /* SCP failed */

void ssh_connection_print_error(int code);
int instantiate_ssh_connection(ssh_session *session, const char *hostname);
int terminate_ssh_connection(ssh_session *session);
int scp_read_json_file(ssh_session *session, const char *filepath,
                       char **buffer, int *buffer_len);
int scp_copy_directory(ssh_session *session, const char *dirpath,
                       const char *result_dirpath);

#endif
