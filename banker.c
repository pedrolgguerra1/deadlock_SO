#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <stdarg.h>
#define CUSTOMER_FILE "customer.txt"
#define COMMANDS_FILE "commands.txt"
#define RESULT_FILE   "result.txt"
typedef enum { CMD_RQ, CMD_RL, CMD_STAR } cmd_type;
typedef struct {
    cmd_type type;
    int customer;
    int *values;
    int line;
} command;

static int n_customers = 0;
static int n_resources = 0;
static int *available = NULL;
static int **maximum = NULL;
static int **allocation = NULL;
static int **need = NULL;
static command *cmds = NULL;
static int n_cmds = 0;
static FILE *out = NULL;
static void free_all(void)
{
    for (int i = 0; i < n_customers; i++) {
        free(maximum[i]);
        free(allocation[i]);
        free(need[i]);
    }
    free(maximum);
    free(allocation);
    free(need);
    free(available);
    for (int k = 0; k < n_cmds; k++)
        free(cmds[k].values);
    free(cmds);
    maximum = allocation = need = NULL;
    available = NULL;
    cmds = NULL;
    n_customers = n_cmds = n_resources = 0;
    if (out && out != stdout)
        fclose(out);
    out = NULL;
}
int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    free_all();
    return 0;
}