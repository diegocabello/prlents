use cc;

fn main() {
    cc::Build::new()
        .file("src/eval_shell.c")
        .compile("eval_shell");
}
