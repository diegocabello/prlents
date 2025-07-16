use argh::FromArgs;

#[derive(FromArgs)]
/// prlents - a tool for parsing and filtering
pub struct Args {
    /// enable explicit mode
    #[argh(switch, short = 'e', long = "explict")]
    pub explicit: bool,
    
    /// enable force mode
    #[argh(switch, short = 'f', long = "force")]
    pub force: bool,

    /// command to run
    #[argh(positional)]
    pub command: String,
    
    /// additional arguments
    #[argh(positional)]
    pub args: Vec<String>,
}