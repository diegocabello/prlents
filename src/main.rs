use std::path::Path;
use std::error::Error;
use std::collections::HashSet;
use std::env;
use std::fs;

mod common;
mod relationship;
mod parser;
mod handle_file; 
mod merge_tags;
mod options;

use parser::parse_ents;
use options::Args;

use crate::common::{TagType, EntsTag, TagsFile, read_tags_from_json, save_tags_to_json};

use relationship::{
    Operation, is_visible_tag, assign_bidir_file_tag_rel, filter_command, represent_inspect
};

use merge_tags::merge_tags;

fn main() -> Result<(), Box<dyn Error>> {

    let raw_args: Vec<String> = env::args().collect();

    if raw_args.len() < 2 {
        println!("Usage: prlents <ttf|ftt|fil|int|insp|process>");
        return Ok(());
    }
    
    let args: Args = argh::from_env();

    let command = &args.command;

    if command == "process" || command == "parse" {
        let current_dir = std::env::current_dir()?;

        let file_path = if !args.args.is_empty() {
            &args.args[0]
        } else {
            "tags.ents" // Default if no file specified
        };
        
        match parse_ents(file_path) {
            Ok(parsed_tags_file) => {
                let parsed_obj = &parsed_tags_file;
                let json_content = serde_json::to_string_pretty(parsed_obj)?; // it needs to be mutable

                if !Path::new("tags.json").exists() {
                    fs::write("tags.json", json_content + "\n")?;
                } else {
                    merge_tags(json_content, "tags.json");
                }

                println!("Successfully parsed {} and saved to tags.json", file_path);
            },
            Err(e) => {
                println!("Error: {}", e);
                return Err(e);
            }
        }
        return Ok(());
    }    
    
    let mut tags_file = match read_tags_from_json() {
        Ok(tf) => tf,
        Err(e) => {
            return Err(e);
        }
    };
    
    if command == "filter" || command == "fil" || command == "union" || command == "un" {
        for file in filter_command(&mut tags_file, &args.args, args.explicit)? {
            println!("{}", file.trim());
        }
    
    } else if command == "intersection" || command  == "intersect" || command == "int" {
        
        if args.args.len() < 1 {
            eprintln!("need at least one tag for intersection");
            std::process::exit(1);
        } else if args.args.len() == 1 {
            for file in filter_command(&mut tags_file, &args.args, args.explicit)? {
                println!("{}", file.trim());
            }
            return Ok(())
        } else {

            let mut result: HashSet<_> = filter_command(&mut tags_file, &[args.args[0].clone()], args.explicit)?.iter().cloned().collect();
            for tag in &args.args[1..] {
                let other_result: HashSet<_> = filter_command(&mut tags_file, &[tag.clone()], args.explicit)?.iter().cloned().collect();
                result = result.intersection(&other_result).cloned().collect();
            }
            
            let vec: Vec<_> = result.into_iter().collect(); 
            for file in vec {
                println!("{}", file.trim());
            }
        }

    } else if command == "inspect" || command == "insp" {
        represent_inspect(&mut tags_file, &args.args)?;
        
    } else {
        if command != "tagtofiles" && command != "ttf" && command != "filetotags" && command != "ftt" {
            println!("invalid command: {}", command);
            return Ok(());
        }

        if args.args.len() >= 2 {
            let operation = Operation::from(&args.args[0][..]);
            
            match operation {
                Operation::Unknown => {
                    println!("invalid operation: {}", args.args[0]);
                    return Ok(());
                },
                _ => {}
            }
            
            let monad = &args.args[1];
            let arguments = &args.args[2..];
            
            if command == "tagtofiles" || command == "ttf" {
                // Resolve the actual tag name from aliases
                let display_tag_name = match tags_file.aliases.get(monad) {
                    Some(actual_name) => actual_name,
                    None => monad,
                };
                
                let foo = tags_file.tags.iter()
                    .find(|t| t.name == *display_tag_name && is_visible_tag(t));
                
                if let Some(foo) = foo {
                    if foo.tag_type == TagType::Dud {
                        println!("cannot assign dud tag to files: \t{}", monad);
                        return Ok(());
                    }
                    
                    for file in arguments {
                        assign_bidir_file_tag_rel(file, monad, operation, &mut tags_file, args.force)?;
                    }
                    
                    save_tags_to_json(&tags_file)?;
                } else {
                    println!("tag does not exist: {}", monad);
                }

            } else if command == "filetotags" || command == "ftt" {
                if !Path::new(monad).exists() {
                    println!("file does not exist: {}", monad);
                    return Ok(());
                }
                
                if args.force && arguments.len() > 1 {
                    println!("logic for multiple force tags hasn't been implimented yet");
                    return Ok(());
                }

                //if there isn't a tag for this, the "tag does not exist" error message is handled in bidirrel
                for tag in arguments {
                    assign_bidir_file_tag_rel(monad, tag, operation, &mut tags_file, args.force)?;
                }
                
                save_tags_to_json(&tags_file)?;
            } else {
                println!("error: invalid command '{}'", command);
            }
        } else {
            println!("not enough options");
        }
    }
    
    Ok(())
}
