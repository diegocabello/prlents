use std::env;

fn print_bash_functions() {
    println!("# Bash Functions\n");
    println!("ct() {{");
    println!("    echo $1 > ~/.entsfs");
    println!("}}");
    println!();
    println!("setps() {{");
    println!("    if [ -f ~/.entsfs ]; then");
    println!("        FUSENTS_VALUE=$(cat ~/.entsfs)");
    println!("        if [ \"$FUSENTS_VALUE\" = \"\" ]; then");
    println!("            PS1=\"\\[\\033[01;32m\\]\\u@\\h | \\W\\[\\033[00m\\] \\$ \"");
    println!("        else");
    println!("            PS1=\"\\[\\033[01;32m\\]\\u@\\h | \\W | $FUSENTS_VALUE\\[\\033[00m\\] \\$ \"");
    println!("        fi");
    println!("    else");
    println!("        touch ~/.entsfs");
    println!("        PS1=\"\\[\\033[01;32m\\]\\u@\\h | \\W\\[\\033[00m\\] \\$ \"");
    println!("    fi");
    println!("}}");
    println!();
    println!("PROMPT_COMMAND=setps");
    println!();
    println!("fil() {{");
    println!("    if [ -z \"$1\" ]; then");
    println!("        if [ -f ~/.entsfs ] && [ -s ~/.entsfs ]; then");
    println!("            prlents intersection $(cat ~/.entsfs)");
    println!("        else");
    println!("            echo \"No entity set in ~/.entsfs\"");
    println!("        fi");
    println!("        return");
    println!("    fi");
    println!("    ct $1");
    println!("    prlents intersection $1");
    println!("}}");
    println!();
    println!("tag() {{");
    println!("    prlents ttf add $(cat ~/.entsfs) $@");
    println!("}}");
}

fn print_zsh_functions() {
    println!("# Zsh Functions\n");
    println!("ct() {{");
    println!("    echo $1 > ~/.entsfs");
    println!("}}");
    println!();
    println!("setps() {{");
    println!("    if [ -f ~/.entsfs ]; then");
    println!("        FUSENTS_VALUE=$(cat ~/.entsfs)");
    println!("        if [ \"$FUSENTS_VALUE\" = \"\" ]; then");
    println!("            PS1=\"%F{{green}}%n@%m | %1~ %f%# \"");
    println!("        else");
    println!("            PS1=\"%F{{green}}%n@%m | %1~ | $FUSENTS_VALUE %f%# \"");
    println!("        fi");
    println!("    else");
    println!("        touch ~/.entsfs");
    println!("        PS1=\"%F{{green}}%n@%m | %1~ %f%# \"");
    println!("    fi");
    println!("}}");
    println!();
    println!("precmd() {{ setps }}");
    println!();
    println!("fil() {{");
    println!("    if [ -z \"$1\" ]; then");
    println!("        if [ -f ~/.entsfs ] && [ -s ~/.entsfs ]; then");
    println!("            prlents intersection $(cat ~/.entsfs)");
    println!("        else");
    println!("            echo \"No entity set in ~/.entsfs\"");
    println!("        fi");
    println!("        return");
    println!("    fi");
    println!("    ct $1");
    println!("    prlents intersection $1");
    println!("}}");
    println!();
    println!("tag() {{");
    println!("    prlents ttf add $(cat ~/.entsfs) $@");
    println!("}}");
}

pub fn print_shell_functions() {
    match env::var("SHELL") {
        Ok(shell) => {
            if shell.ends_with("/bash") || shell.contains("bash") {
                print_bash_functions();
            } else if shell.ends_with("/zsh") || shell.contains("zsh") {
                print_zsh_functions();
            } else {
                println!("# Unknown shell: {}", shell);
                println!("# Showing bash version as default\n");
                print_bash_functions();
            }
        }
        Err(_) => {
            println!("# Could not detect shell, showing both versions\n");
            print_bash_functions();
            println!();
            print_zsh_functions();
        }
    }
}