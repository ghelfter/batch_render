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

#include <libssh/libssh.h>
#include <cjson/cJSON.h>

#include "ssh_session.h"

#define NARGS 3

/* Function declarations */
void print_usage(FILE *fout);
void print_help(FILE *fout);

int main(int argc, char **argv)
{
    int retcode = 0;
    int i = 0;
    int retval;
    ssh_session session = NULL;
    char *buffer = NULL;
    int buffer_len = 0;

    FILE *fd = NULL;
    char *config_buffer = NULL;
    int config_size = 0;

    /* int state_flag = 0; */

    /* JSON structures */
    struct cJSON *config_json = NULL;;
    struct cJSON *config_host = NULL;;
    struct cJSON *config_hosts = NULL;;
    struct cJSON *config_cluster = NULL;;
    struct cJSON *config_clusters = NULL;;
    struct cJSON *local_config = NULL;

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
        print_usage(stdout);
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
        retcode = 1;
        goto CLEANUP;
    }

    fseek(fd, 0, SEEK_END);
    config_size = ftell(fd);
    fseek(fd, 0, SEEK_SET);

    config_buffer = malloc(sizeof(char) * (config_size+1));
    if(config_buffer == NULL)
    {
        retcode = 1;
        goto CLEANUP;
    }

    fread(config_buffer, sizeof(char), config_size, fd);
    *(config_buffer + config_size) = '\0';

    fclose(fd);
    fd = NULL;

    /* Use cJSON to parse  the JSON */
    config_json = cJSON_Parse(config_buffer);
    if(config_json == NULL)
    {
        retcode = 1;
        goto CLEANUP;
    }

    /* Go through the hosts */
    config_hosts = cJSON_GetObjectItemCaseSensitive(config_json, "hosts");

    cJSON_ArrayForEach(config_host, config_hosts)
    {
        fprintf(stdout, "Host: %s\n", config_host->valuestring);
        /* Insert into a list */
    }

    config_host = NULL;
    config_hosts = NULL;

    config_clusters = cJSON_GetObjectItemCaseSensitive(config_json,
                                                       "clusters");

    cJSON_ArrayForEach(config_cluster, config_clusters)
    {
        /* Acquire start & end numbers and construct machine list */
    }

    config_cluster = NULL;
    config_clusters = NULL;

    /* Use current machine just to test */
    retval = instantiate_ssh_connection(&session, "Arthedain");
    if(retval != SSH_SESSION_GOOD)
    {
        retcode = 1;
        goto CLEANUP;
    }

    retval = scp_read_json_file(&session,
                                "/home/batch_renderer/batch_render.json",
                                &buffer,
                                &buffer_len);
    if(retval != SSH_SESSION_GOOD)
    {
        retcode = 1;
        goto CLEANUP;
    }

    fwrite(buffer, sizeof(char), buffer_len, stdout);
    fputc('\n', stdout);

CLEANUP:
    if(config_json != NULL)
    {
        cJSON_Delete(config_json);
        config_json = NULL;
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
    retval =  terminate_ssh_connection(&session);

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
    fprintf(fout, "batch_collect:\n\nDescription\n");
    fprintf(fout, "This program is part of a set of utility scripts making");
    fprintf(fout, " a distributed render queue. These utilities take JSON");
    fprintf(fout, " configuration files, and use them to distribute");
    fprintf(fout, " rendering tasks to the set of machines specified in");
    fprintf(fout, " them.\n\n");

    fprintf(fout, "This program uses libssh and cJSON to parse the config");
    fprintf(fout, " files and to connect to the given machines. It will");
    fprintf(fout, " save all of the files in the render directory to the");
    fprintf(fout, " given directory.\n");

    fprintf(fout, "\nAUTHOR\nThese scripts and programs were written by");
    fprintf(fout, " Galen Helfter.\n");
}
