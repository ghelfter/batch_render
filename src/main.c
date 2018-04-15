/**************************************************
 *                  Galen Helfter
 *               ghelfter@gmail.com
 *                     main.c
 **************************************************/

#define _POSIX_C_SOURCE 200809L

/* C standard headers */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <unistd.h>

#include <libssh/libssh.h>
#include <cjson/cJSON.h>

#include "ssh_session.h"

#define NARGS 3
#define MAX_HOSTS 256

/* Function declarations */
void print_usage(FILE *fout);
void print_help(FILE *fout);
char* construct_machine_name(const char *prefix, const char *suffix, int n);

int main(int argc, char **argv)
{
    int retcode = 0;
    int i = 0, j = 0;
    int ssh_retval = 0;
    ssh_session session = NULL;
    char *buffer = NULL;
    int buffer_len = 0;

    FILE *fd = NULL;
    char *config_buffer = NULL;
    int config_size = 0;

    char* host_list[MAX_HOSTS];
    int num_hosts = 0;

    int start = 0;
    int end = 0;

    /* int state_flag = 0; */

    /* JSON structures */
    struct cJSON *config_json = NULL;
    struct cJSON *config_host = NULL;
    struct cJSON *config_hosts = NULL;
    struct cJSON *config_cluster = NULL;
    struct cJSON *config_clusters = NULL;
    struct cJSON *local_config = NULL;

    struct cJSON *t1 = NULL;
    struct cJSON *t2 = NULL;

    struct stat st;

    pid_t pid = 0;

    /* Clear host list */
    memset(host_list, 0x00, sizeof(char*) * num_hosts);

    /* Check command line arguments */
    if(argc < NARGS)
    {
        for(i = 1; i < argc; ++i)
        {
            if(!strncmp(argv[i], "-h", 2))
            {
                print_help(stdout);
                goto CLEANUP; /* Exit */
            }
            else if(!strncmp(argv[i], "--help", 6))
            {
                print_help(stdout);
                goto CLEANUP; /* Exit */
            }
        }

        retcode = 1; /* Improper usage, set retcode and exit */
        print_usage(stderr);
        goto CLEANUP;
    }

    /* Check for help flag */
    for(i = 1; i < argc; ++i)
    {
        if(!strncmp(argv[i], "-h", 2))
        {
            print_help(stdout);
            goto CLEANUP; /* Exit */
        }
        else if(!strncmp(argv[i], "--help", 6))
        {
            print_help(stdout);
            goto CLEANUP; /* Exit */
        }
    }

    /* Load JSON file into memory */
    fd = fopen(argv[1], "r");
    if(fd == NULL)
    {
        fprintf(stderr, "Failed to open file %s\n", argv[1]);
        retcode = 1;
        goto CLEANUP;
    }

    fseek(fd, 0, SEEK_END);
    config_size = ftell(fd);
    fseek(fd, 0, SEEK_SET);

    config_buffer = malloc(sizeof(char) * (config_size+1));
    if(config_buffer == NULL)
    {
        fprintf(stderr, "Failed memory allocation\n");
        retcode = 1;
        goto CLEANUP;
    }

    fread(config_buffer, sizeof(char), config_size, fd);
    *(config_buffer + config_size) = '\0';

    fclose(fd);
    fd = NULL;

    /* Assert that the specified directory exists */
    if(stat(argv[2], &st) != 0)
    {
        fprintf(stderr, "Local directory not found.\n");
        retcode = 1;
        goto CLEANUP;
    }


    /* Use cJSON to parse the JSON */
    config_json = cJSON_Parse(config_buffer);
    if(config_json == NULL)
    {
        fprintf(stderr, "Failed to parse JSON file.\n");
        retcode = 1;
        goto CLEANUP;
    }

    /* Go through the hosts */
    config_hosts = cJSON_GetObjectItemCaseSensitive(config_json, "hosts");

    if(config_hosts == NULL)
    {
        fprintf(stderr, "Failed to parse JSON file.\n");
        retcode = 1;
        goto CLEANUP;
    }

    cJSON_ArrayForEach(config_host, config_hosts)
    {
        fprintf(stdout, "Host: %s\n", config_host->valuestring);
        /* Insert into a list */
        host_list[num_hosts] = strdup(config_host->valuestring);
        num_hosts += 1;
    }

    config_host = NULL;
    config_hosts = NULL;

    config_clusters = cJSON_GetObjectItemCaseSensitive(config_json,
                                                       "clusters");

    cJSON_ArrayForEach(config_cluster, config_clusters)
    {
        /* Acquire start & end numbers and construct machine list */
        t1 = cJSON_GetObjectItemCaseSensitive(config_cluster, "start");
        t2 = cJSON_GetObjectItemCaseSensitive(config_cluster, "end");
        if(t1 == NULL || t2 == NULL)
        {
            fprintf(stderr, "Failed to parse JSON file.\n");
            retcode = 1;
            goto CLEANUP;
        }

        if(!cJSON_IsNumber(t1) || !cJSON_IsNumber(t2))
        {
            fprintf(stderr, "Failed to parse JSON file.\n");
            retcode = 1;
            goto CLEANUP;
        }

        start = t1->valueint;
        end = t2->valueint;

        t1 = cJSON_GetObjectItemCaseSensitive(config_cluster, "prefix");
        t2 = cJSON_GetObjectItemCaseSensitive(config_cluster, "suffix");
        if(t1 == NULL || t2 == NULL)
        {
            fprintf(stderr, "Failed to parse JSON file.\n");
            retcode = 1;
            goto CLEANUP;
        }

        if(!cJSON_IsString(t1) || !cJSON_IsString(t2))
        {
            fprintf(stderr, "Failed to parse JSON file.\n");
            retcode = 1;
            goto CLEANUP;
        }

        for(i = start; i <= end; ++i)
        {
            host_list[num_hosts] = construct_machine_name(t1->valuestring,
                                                          t2->valuestring,
                                                          i);
            num_hosts += 1;
        }
    }

    config_cluster = NULL;
    config_clusters = NULL;

    for(i = 0; i < num_hosts; ++i)
    {
        /* Fork the process for each of the different SSH connections */
        pid = fork();
        if (pid == 0)
        {
            /* Use current machine just to test */
            ssh_retval = instantiate_ssh_connection(&session, "Arthedain");
            if(ssh_retval != SSH_SESSION_GOOD)
            {
                retcode = 1;
                goto CLEANUP;
            }
            ssh_retval = scp_read_json_file(&session,
                                    "/home/batch_renderer/batch_render.json",
                                    &buffer, &buffer_len);

            if(ssh_retval != SSH_SESSION_GOOD)
            {
                retcode = 1;
                goto CLEANUP;
            }

            /* Acquire render directory from the host */
            local_config = cJSON_Parse(buffer);
            if(local_config == NULL)
            {
                fprintf(stderr, "Failed to parse hoost JSON file.\n");
                retcode = 1;
                goto CLEANUP;
            }

            t1 = NULL;
            t1 = cJSON_GetObjectItemCaseSensitive(local_config,
                                                  "render_directory");
            if(t1 == NULL || !cJSON_IsString(t1))
            {
                fprintf(stderr, "Host JSON file does not fit standard.\n");
                retcode = 1;
                goto CLEANUP;
            }

            fprintf(stdout, "Render directory: %s\n", t1->valuestring);
        }
        /* Failure to fork */
        else if(pid < 0)
        {
            break;
        }
    }
    /* Have the parent process wait on the remaining child processes */
    if(pid > 0)
    {
        for(j = 0; j < i; ++j)
        {
            wait(&retcode);
        }
    }

CLEANUP:

    if(ssh_retval != SSH_SESSION_GOOD)
    {
        ssh_connection_print_error(ssh_retval);
    }

    /* Clear host list */
    for(i = 0; i < num_hosts; ++i)
    {
        if(host_list[i] != NULL)
        {
            free(host_list[i]);
            host_list[i] = NULL;
        }
    }

    if(config_json != NULL)
    {
        cJSON_Delete(config_json);
        config_json = NULL;
    }

    if(local_config != NULL)
    {
        cJSON_Delete(local_config);
        local_config = NULL;
    }

    if(fd != NULL)
    {
        fclose(fd);
        fd = NULL;
    }

    if(config_buffer != NULL)
    {
        free(config_buffer);
        config_buffer = NULL;
    }

    /* Terminate the session */
    ssh_retval =  terminate_ssh_connection(&session);

    if(buffer != NULL)
    {
        free(buffer);
        buffer = NULL;
    }

    return retcode;
}

void print_usage(FILE *fout)
{
    fprintf(fout, "Usage: ./batch_collect {config_file} {final_directory}\n");
}

void print_help(FILE *fout)
{
    fprintf(fout, "batch_collect:\n\nDescription\n"
                  "This program is part of a set of utility scripts making"
                  " a distributed render queue. These utilities take JSON"
                  " configuration files, and use them to distribute"
                  " rendering tasks to the set of machines specified in"
                  " them.\n\n"
                  "This program uses libssh and cJSON to parse the config"
                  " files and to connect to the given machines. It will"
                  " save all of the files in the render directory to the"
                  " given directory.\n"
                  "\nAUTHOR\nThese scripts and programs were written by"
                  " Galen Helfter.\n");
}

char* construct_machine_name(const char *prefix, const char *suffix, int n)
{
    char *res = NULL;
    int len = 0;

    /* Convert n to a string */
    len = snprintf(NULL, 0, "%d", n);
    len += strlen(prefix);
    len += strlen(suffix);

    res = malloc(sizeof(char) * (len+1));

    snprintf(res, len+1, "%s%d%s", prefix, n, suffix);

    return res;
}
