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

static void die(const char *fmt, ...)
{
    va_list ap;
    fputs("Erro: ", stderr);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(EXIT_FAILURE);
}

static int parse_nonneg(const char *s, int *value)
{
    char *end;
    long x;
    if (*s == '\0' || *s == '-' || *s == '+')
        return 0;
    errno = 0;
    x = strtol(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0' || x < 0 || x > INT_MAX)
        return 0;
    *value = (int)x;
    return 1;
}

static char *trim(char *s)
{
    char *e;
    while (*s == ' ' || *s == '\t')
        s++;
    e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n'))
        *--e = '\0';
    return s;
}

static void *xmalloc(size_t n)
{
    void *p = malloc(n ? n : 1);
    if (!p)
        die("memoria insuficiente");
    return p;
}

static void parse_args(int argc, char **argv)
{
    if (argc < 2)
        die("uso: %s <instancias_recurso_0> [<instancias_recurso_1> ...]", argv[0]);
    n_resources = argc - 1;
    available = xmalloc(n_resources * sizeof(int));
    for (int j = 0; j < n_resources; j++)
        if (!parse_nonneg(argv[j + 1], &available[j]))
            die("argumento '%s' invalido: use inteiros nao negativos", argv[j + 1]);
}

static void add_customer(int *row)
{
    int i = n_customers;
    maximum    = realloc(maximum,    (i + 1) * sizeof(int *));
    allocation = realloc(allocation, (i + 1) * sizeof(int *));
    need       = realloc(need,       (i + 1) * sizeof(int *));
    if (!maximum || !allocation || !need)
        die("memoria insuficiente");
    maximum[i] = row;
    allocation[i] = calloc(n_resources, sizeof(int));
    need[i] = xmalloc(n_resources * sizeof(int));
    if (!allocation[i])
        die("memoria insuficiente");
    memcpy(need[i], row, n_resources * sizeof(int));
    n_customers++;
}

static void load_customers(void)
{
    FILE *f = fopen(CUSTOMER_FILE, "r");
    char *line = NULL, *p, *comma;
    char msg[160] = "";
    size_t cap = 0;
    int lineno = 0, count, *row = NULL;

    if (!f)
        die("nao foi possivel abrir '%s': %s", CUSTOMER_FILE, strerror(errno));

    while (getline(&line, &cap, f) != -1) {
        lineno++;
        p = trim(line);
        if (*p == '\0')
            continue;
        row = xmalloc(n_resources * sizeof(int));
        count = 0;
        for (;;) {
            comma = strchr(p, ',');
            if (comma)
                *comma = '\0';
            if (count >= n_resources) {
                snprintf(msg, sizeof msg,
                         "%s linha %d: mais valores do que os %d recursos informados na execucao",
                         CUSTOMER_FILE, lineno, n_resources);
                goto fail;
            }
            if (!parse_nonneg(trim(p), &row[count])) {
                snprintf(msg, sizeof msg,
                         "%s linha %d: valor invalido na coluna %d (esperado inteiro >= 0)",
                         CUSTOMER_FILE, lineno, count + 1);
                goto fail;
            }
            count++;
            if (!comma)
                break;
            p = comma + 1;
        }
        if (count != n_resources) {
            snprintf(msg, sizeof msg,
                     "%s linha %d: %d valores, mas foram informados %d recursos na execucao",
                     CUSTOMER_FILE, lineno, count, n_resources);
            goto fail;
        }
        add_customer(row);
        row = NULL;
    }
    if (ferror(f)) {
        snprintf(msg, sizeof msg, "falha de leitura em %s", CUSTOMER_FILE);
        goto fail;
    }
    if (n_customers == 0) {
        snprintf(msg, sizeof msg, "%s esta vazio: nenhum cliente encontrado", CUSTOMER_FILE);
        goto fail;
    }
    free(line);
    fclose(f);
    return;

fail:
    free(row);
    free(line);
    fclose(f);
    die("%s", msg);
}

static void add_command(cmd_type type, int customer, int *values, int line)
{
    cmds = realloc(cmds, (n_cmds + 1) * sizeof(command));
    if (!cmds)
        die("memoria insuficiente");
    cmds[n_cmds].type = type;
    cmds[n_cmds].customer = customer;
    cmds[n_cmds].values = values;
    cmds[n_cmds].line = line;
    n_cmds++;
}

static void parse_commands(void)
{
    FILE *f = fopen(COMMANDS_FILE, "r");
    char *line = NULL, *p, *tok, *save;
    char msg[160] = "";
    size_t cap = 0;
    int lineno = 0, cust, count, *vals = NULL;
    cmd_type type;

    if (!f)
        die("nao foi possivel abrir '%s': %s", COMMANDS_FILE, strerror(errno));

    while (getline(&line, &cap, f) != -1) {
        lineno++;
        p = trim(line);
        if (*p == '\0')
            continue;
        tok = strtok_r(p, " \t", &save);
        if (strcmp(tok, "*") == 0) {
            if (strtok_r(NULL, " \t", &save)) {
                snprintf(msg, sizeof msg, "%s linha %d: o comando * nao recebe parametros",
                         COMMANDS_FILE, lineno);
                goto fail;
            }
            add_command(CMD_STAR, -1, NULL, lineno);
            continue;
        }
        if (strcmp(tok, "RQ") == 0)
            type = CMD_RQ;
        else if (strcmp(tok, "RL") == 0)
            type = CMD_RL;
        else {
            snprintf(msg, sizeof msg, "%s linha %d: comando '%s' desconhecido (use RQ, RL ou *)",
                     COMMANDS_FILE, lineno, tok);
            goto fail;
        }
        tok = strtok_r(NULL, " \t", &save);
        if (!tok || !parse_nonneg(tok, &cust) || cust >= n_customers) {
            snprintf(msg, sizeof msg, "%s linha %d: indice de cliente invalido (esperado 0 a %d)",
                     COMMANDS_FILE, lineno, n_customers - 1);
            goto fail;
        }
        vals = xmalloc(n_resources * sizeof(int));
        count = 0;
        while ((tok = strtok_r(NULL, " \t", &save)) != NULL) {
            if (count >= n_resources) {
                snprintf(msg, sizeof msg,
                         "%s linha %d: valores demais (esperado %d, conforme os recursos da execucao)",
                         COMMANDS_FILE, lineno, n_resources);
                goto fail;
            }
            if (!parse_nonneg(tok, &vals[count])) {
                snprintf(msg, sizeof msg, "%s linha %d: valor '%s' invalido (esperado inteiro >= 0)",
                         COMMANDS_FILE, lineno, tok);
                goto fail;
            }
            count++;
        }
        if (count != n_resources) {
            snprintf(msg, sizeof msg,
                     "%s linha %d: %d valores, mas foram informados %d recursos na execucao",
                     COMMANDS_FILE, lineno, count, n_resources);
            goto fail;
        }
        add_command(type, cust, vals, lineno);
        vals = NULL;
    }
    if (ferror(f)) {
        snprintf(msg, sizeof msg, "falha de leitura em %s", COMMANDS_FILE);
        goto fail;
    }
    free(line);
    fclose(f);
    return;

fail:
    free(vals);
    free(line);
    fclose(f);
    die("%s", msg);
}

static int exceeds(const int *a, const int *b)
{
    for (int j = 0; j < n_resources; j++)
        if (a[j] > b[j])
            return 1;
    return 0;
}

static int is_safe(void)
{
    int *work = xmalloc(n_resources * sizeof(int));
    int *finish = calloc(n_customers, sizeof(int));
    int done = 0, progress = 1;

    if (!finish) {
        free(work);
        die("memoria insuficiente");
    }
    memcpy(work, available, n_resources * sizeof(int));
    while (done < n_customers && progress) {
        progress = 0;
        for (int i = 0; i < n_customers; i++) {
            if (!finish[i] && !exceeds(need[i], work)) {
                for (int j = 0; j < n_resources; j++)
                    work[j] += allocation[i][j];
                finish[i] = 1;
                done++;
                progress = 1;
            }
        }
    }
    free(work);
    free(finish);
    return done == n_customers;
}

static void print_vec(FILE *f, const int *v)
{
    for (int j = 0; j < n_resources; j++)
        fprintf(f, j ? " %d" : "%d", v[j]);
}

static void request_resources(int i, const int *req)
{
    if (exceeds(req, need[i])) {
        fprintf(out, "The customer %d request ", i);
        print_vec(out, req);
        fputs(" was denied because exceed its maximum allocation\n", out);
        return;
    }
    if (exceeds(req, available)) {
        fputs("The resources ", out);
        print_vec(out, available);
        fprintf(out, " are not enough to customer %d request ", i);
        print_vec(out, req);
        fputs(" \n", out);
        return;
    }

    for (int j = 0; j < n_resources; j++) {
        available[j]     -= req[j];
        allocation[i][j] += req[j];
        need[i][j]       -= req[j];
    }
    if (!is_safe()) {
        for (int j = 0; j < n_resources; j++) {
            available[j]     += req[j];
            allocation[i][j] -= req[j];
            need[i][j]       += req[j];
        }
        fprintf(out, "The customer %d request ", i);
        print_vec(out, req);
        fputs(" was denied because result in an unsafe state\n", out);
        return;
    }
    fprintf(out, "Allocate to customer %d the resources ", i);
    print_vec(out, req);
    fputc('\n', out);
}

static void release_resources(int i, const int *rel)
{
    if (exceeds(rel, allocation[i])) {
        fprintf(out, "The customer %d release ", i);
        print_vec(out, rel);
        fputs(" was denied because exceed its allocation\n", out);
        return;
    }
    for (int j = 0; j < n_resources; j++) {
        available[j]     += rel[j];
        allocation[i][j] -= rel[j];
        need[i][j]       += rel[j];
    }
    fprintf(out, "Release from customer %d the resources ", i);
    print_vec(out, rel);
    fputc('\n', out);
}

static void process_commands(void)
{
    for (int k = 0; k < n_cmds; k++) {
        switch (cmds[k].type) {
        case CMD_RQ: request_resources(cmds[k].customer, cmds[k].values); break;
        case CMD_RL: release_resources(cmds[k].customer, cmds[k].values); break;
        case CMD_STAR: break;
        }
    }
}

int main(int argc, char **argv)
{
    parse_args(argc, argv);
    load_customers();
    parse_commands();
    out = stdout;
    process_commands();
    free_all();
    return 0;
}