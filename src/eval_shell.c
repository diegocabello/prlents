#include "eval_shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_bash_functions(void) {
    printf("# Bash Functions\n\n");
    printf("ct() {\n");
    printf("    echo $1 > ~/.entsfs\n");
    printf("}\n\n");
    printf("setps() {\n");
    printf("    if [ -f ~/.entsfs ]; then\n");
    printf("        FUSENTS_VALUE=$(cat ~/.entsfs)\n");
    printf("        if [ \"$FUSENTS_VALUE\" = \"\" ]; then\n");
    printf("            PS1=\"\\[\\033[01;32m\\]\\u@\\h | \\W\\[\\033[00m\\] \\$ \"\n");
    printf("        else\n");
    printf("            PS1=\"\\[\\033[01;32m\\]\\u@\\h | \\W | $FUSENTS_VALUE\\[\\033[00m\\] \\$ \"\n");
    printf("        fi\n");
    printf("    else\n");
    printf("        touch ~/.entsfs\n");
    printf("        PS1=\"\\[\\033[01;32m\\]\\u@\\h | \\W\\[\\033[00m\\] \\$ \"\n");
    printf("    fi\n");
    printf("}\n\n");
    printf("PROMPT_COMMAND=setps\n\n");
    printf("fil() {\n");
    printf("    if [ -z \"$1\" ]; then\n");
    printf("        if [ -f ~/.entsfs ] && [ -s ~/.entsfs ]; then\n");
    printf("            prlents intersection $(cat ~/.entsfs)\n");
    printf("        else\n");
    printf("            echo \"No entity set in ~/.entsfs\"\n");
    printf("        fi\n");
    printf("        return\n");
    printf("    fi\n");
    printf("    ct $1\n");
    printf("    prlents intersection $1\n");
    printf("}\n\n");
    printf("tag() {\n");
    printf("    prlents ttf add $(cat ~/.entsfs) $@\n");
    printf("}\n");
}

static void print_zsh_functions(void) {
    printf("# Zsh Functions\n\n");
    printf("ct() {\n");
    printf("    echo $1 > ~/.entsfs\n");
    printf("}\n\n");
    printf("setps() {\n");
    printf("    if [ -f ~/.entsfs ]; then\n");
    printf("        FUSENTS_VALUE=$(cat ~/.entsfs)\n");
    printf("        if [ \"$FUSENTS_VALUE\" = \"\" ]; then\n");
    printf("            PS1=\"%%F{green}%%n@%%m | %%1~ %%f%%# \"\n");
    printf("        else\n");
    printf("            PS1=\"%%F{green}%%n@%%m | %%1~ | $FUSENTS_VALUE %%f%%# \"\n");
    printf("        fi\n");
    printf("    else\n");
    printf("        touch ~/.entsfs\n");
    printf("        PS1=\"%%F{green}%%n@%%m | %%1~ %%f%%# \"\n");
    printf("    fi\n");
    printf("}\n\n");
    printf("precmd() { setps }\n\n");
    printf("fil() {\n");
    printf("    if [ -z \"$1\" ]; then\n");
    printf("        if [ -f ~/.entsfs ] && [ -s ~/.entsfs ]; then\n");
    printf("            prlents intersection $(cat ~/.entsfs)\n");
    printf("        else\n");
    printf("            echo \"No entity set in ~/.entsfs\"\n");
    printf("        fi\n");
    printf("        return\n");
    printf("    fi\n");
    printf("    ct $1\n");
    printf("    prlents intersection $1\n");
    printf("}\n\n");
    printf("tag() {\n");
    printf("    prlents ttf add $(cat ~/.entsfs) $@\n");
    printf("}\n");
}

void print_shell_functions(void) {
    const char *shell = getenv("SHELL");
    if (shell) {
        if (strstr(shell, "bash")) {
            print_bash_functions();
        } else if (strstr(shell, "zsh")) {
            print_zsh_functions();
        } else {
            printf("# Unknown shell: %s\n", shell);
            printf("# Showing bash version as default\n\n");
            print_bash_functions();
        }
    } else {
        printf("# Could not detect shell, showing both versions\n\n");
        print_bash_functions();
        printf("\n");
        print_zsh_functions();
    }
}
