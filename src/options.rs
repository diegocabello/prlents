use argh::FromArgs;

#[derive(FromArgs)]
/// prlents - a tool for parsing and filtering
pub struct Args {
    /// enable explicit mode
    #[argh(switch, short = 'e')]
    pub explicit: bool,
    
    /// command to run
    #[argh(positional)]
    pub command: String,
    
    /// additional arguments
    #[argh(positional)]
    pub args: Vec<String>,
}