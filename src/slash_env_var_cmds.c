#include <slash/slash.h>
#include <slash/optparse.h>
#include <stdlib.h>
#include <string.h>
#include "environment.h"

/**
 * Adds the following commands to CSH:
 * - var_set
 * - var_get
 * - var_unset
 * - var_show
 * - var_clear
 * - var_expand
 * 
 * Examples:
 * 
 * var_set WATCH_TIME 2000
 * var_set CMD "list"
 * var_show
 *
 * var_expand "watch -n $(WATCH_TIME) $(CMD)"
 * - prints the expanded version of its argument
 * 
 * var_expand -e "watch -n $(WATCH_TIME) $(CMD)"
 * -e means "csh-execute the expanded argument"
 * 
 * var_unset CMD
 * var_show
 * 
 * var_clear
 * var_show
 * 
 * var_set MY_PASSWORD 123456 # Don't you ever do that!
 * var_expand -q -e "command -p $(PASSWORD)"
 * -q means do NOT print the expanded version of its argument, but just csh-execute it.
 * This is useful of you do not want the "PASSWORD" variable to appear in logs, for instance. The log will still
 * contain this 'var_expand -q -e "command -p $(PASSWORD)"', which is still useful enough for tracing/debugging without
 * leaking sensitive info.
 */

slash_command_group(env, "CSH environment variables");

static int slash_var_set(struct slash *slash)
{
    optparse_t * parser = optparse_new("var_set", "NAME VALUE");
    optparse_add_help(parser);
    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi == -1) {
        optparse_del(parser);
        return SLASH_EUSAGE;
    }
    if ((slash->argc - argi) != 3) {
        printf("Must give NAME and VALUE parameters\n");
        optparse_del(parser);
	    return SLASH_EINVAL;
    }
    csh_putvar(slash->argv[1], slash->argv[2]);
    printf("Variable %s defined, value=%s\n", slash->argv[1], csh_getvar(slash->argv[1]));
    optparse_del(parser);    
	return SLASH_SUCCESS;
}
slash_command(var_set, slash_var_set, NULL, "Create or update an environment variable in CSH");

static int slash_var_unset(struct slash *slash)
{
    optparse_t * parser = optparse_new("var_unset", "NAME");
    optparse_add_help(parser);
    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi == -1) {
        optparse_del(parser);
        return SLASH_EUSAGE;
    }
    if ((slash->argc - argi) != 2) {
        printf("Must give NAME and aonly NAME parameter\n");
        optparse_del(parser);
	    return SLASH_EINVAL;
    }
    csh_delvar(slash->argv[1]);
    optparse_del(parser);
	return SLASH_SUCCESS;
}
slash_command(var_unset, slash_var_unset, NULL, "Remove an environment variable in CSH");

static int slash_var_clear(struct slash *slash)
{
    optparse_t * parser = optparse_new("var_clear", NULL);
    optparse_add_help(parser);
    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi == -1) {
        optparse_del(parser);
        return SLASH_EUSAGE;
    }
    if ((slash->argc - argi) != 0) {
        printf("var_clear takes no parameters\n");
        optparse_del(parser);
	    return SLASH_EINVAL;
    }
    csh_clearenv();
    optparse_del(parser);
	return SLASH_SUCCESS;
}
slash_command(var_clear, slash_var_clear, NULL, "Clear all environment variables in CSH");

static int slash_var_get(struct slash *slash)
{
    optparse_t * parser = optparse_new("var_get", "NAME");
    optparse_add_help(parser);
    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi == -1) {
        optparse_del(parser);
        return SLASH_EUSAGE;
    }
    if ((slash->argc - argi) != 2) {
        printf("Must give NAME and aonly NAME parameter\n");
        optparse_del(parser);
	    return SLASH_EINVAL;
    }
    char *value = csh_getvar(slash->argv[1]);
    if(value) {
        printf("%s\n", value);
    }
    optparse_del(parser);
	return SLASH_SUCCESS;
}
slash_command(var_get, slash_var_get, NULL, "Show the value of an environment variable in CSH, shows nothing if variable is not defined");

static void print_var(const char *name) {
    printf("%s=%s\n", name, csh_getvar(name));
}

static int slash_var_show(struct slash *slash)
{
    optparse_t * parser = optparse_new("var_show", "Show all the defined environment variables");
    optparse_add_help(parser);
    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi == -1) {
        optparse_del(parser);
        return SLASH_EUSAGE;
    }
    if ((slash->argc - argi) != 0) {
        printf("var_show takes no parameters\n");
        optparse_del(parser);
	    return SLASH_EINVAL;
    }
    csh_foreach_var(print_var);
    optparse_del(parser);
	return SLASH_SUCCESS;
}
slash_command(var_show, slash_var_show, NULL, "Print the defined variables and their values");

static int slash_var_expand(struct slash *slash)
{
    int result = SLASH_SUCCESS;
    int execute = 0;
    int quiet = 0;
    optparse_t * parser = optparse_new("var_expand", "[-e] [-q] INPUT");
    optparse_add_help(parser);
    optparse_add_set(parser, 'e', "execute", true, &execute, "Attempt to run the result of the expanded string");
    optparse_add_set(parser, 'q', "quiet", true, &quiet, "Do not print the expanded string before executing it (useful if line contains sensitive info that you do not want logged)");
    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi == -1) {
        optparse_del(parser);
        return SLASH_EUSAGE;
    }
    if ((slash->argc - argi) != 2) {
        printf("Must give an INPUT parameter\n");
        optparse_del(parser);
	    return SLASH_EINVAL;
    }
    char *expansion;
    if(!execute) {
        expansion = csh_expand_vars(slash->argv[slash->argc - 1]);
        printf("%s\n", expansion);
        free(expansion);
    } else {
        expansion = csh_expand_vars(slash->argv[slash->argc - 1]);
		strcpy(slash->buffer, expansion);
        free(expansion);
		slash->length = strlen(slash->buffer);
        if(!quiet) {
            slash_refresh(slash, 1);
            printf("\n");
        }
		result = slash_execute(slash, slash->buffer);
    }
    optparse_del(parser);
	return result;
}
slash_command(var_expand, slash_var_expand, NULL, "Display the given INPUT string with references to defined variables expanded");
