use argh::FromArgs;

#[derive(FromArgs)]
/// prlents - a tool for parsing and filtering
pub struct Args {
    /// print shell functions for evaluation
    #[argh(switch, long = "eval-shell")]
    pub eval_shell: bool,

    /// enable explicit mode
    #[argh(switch, short = 'e', long = "explicit")]
    pub explicit: bool,
    
    /// enable force mode
    #[argh(switch, short = 'f', long = "force")]
    pub force: bool,

    /// enable quiet mode
    #[argh(switch, short = 'q', long = "quiet")]
    pub quiet: bool,

    /// command to run
    #[argh(positional)]
    pub command: String,
    
    /// additional arguments
    #[argh(positional)]
    pub args: Vec<String>,
}
