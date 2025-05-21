use std::path::Path;
use std::error::Error;
use std::env;

mod common;
mod relationship;
//mod parser;
mod handle_file; 
//mod merge_tags;

//use parser::parse_ents;

use crate::common::{TagType, EntsTag, TagsFile, read_tags_from_json, save_tags_to_json};

use relationship::{
    Operation, is_visible_tag, assign_bidir_file_tag_rel, filter_command, represent_inspect
};

//use merge_tags::{merge_tags_files}

fn main() -> Result<(), Box<dyn Error>> {
    let args: Vec<String> = env::args().collect();
    
    if args.len() < 2 {
        println!("Usage: prlents <parse|ttf|ftt|filter|inspect> [(<add|remove|show> <monad> <opt1> <opt2> ...) | (<tag1> <tag2> ...)]");
        return Ok(());
    }

    let command = &args[1];

    // if command == "process" || command == "parse" {

    //     let current_dir = std::env::current_dir()?;
    //     println!("Current working directory: {:?}", current_dir);

    //     let file_path = if args.len() > 2 {
    //         &args[2]
    //     } else {
    //         "tags.ents" // Default if no file specified
    //     };
        
    //     println!("Processing file: {}", file_path);
    //     match parse_ents(file_path) {
    //         Ok(parsed_tags_file) => {
    //             save_tags_to_json(&parsed_tags_file)?;
    //             println!("Successfully parsed {} and saved to tags.json", file_path);
    //         },
    //         Err(e) => {
    //             println!("Error: {}", e);
    //             return Err(e);
    //         }
    //     }
    //     return Ok(());
    // } 
    
    if args.len() < 3 {
        println!("Usage: prlents <parse|ttf|ftt|filter|inspect> [(<add|remove|show> <monad> <opt1> <opt2> ...) | (<tag1> <tag2> ...)]");
        return Ok(());
    }    
    
    let mut tags_file = match read_tags_from_json() {
        Ok(tf) => tf,
        Err(e) => {
            return Err(e);
        }
    };
    
    if command == "filter" || command == "fil" {
        let tags = args[2..].to_vec();
        for file in filter_command(&mut tags_file, &tags)? {
            println!("{}", file.trim());
        }
    } else if command == "inspect" || command == "insp" {
        let files = args[2..].to_vec();
        represent_inspect(&mut tags_file, &files)?;
    } else {
        if command != "tagtofiles" && command != "ttf" && command != "filetotags" && command != "ftt" {
            println!("invalid command: {}", command);
            return Ok(());
        }

        if args.len() >= 4 {
            let operation = Operation::from(&args[2][..]);
            
            match operation {
                Operation::Unknown => {
                    println!("invalid operation: {}", args[2]);
                    return Ok(());
                },
                _ => {}
            }
            
            let monad = &args[3];
            let arguments = &args[4..];
            
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
                        assign_bidir_file_tag_rel(file, monad, operation, &mut tags_file)?;
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
                
                for tag in arguments {
                    assign_bidir_file_tag_rel(monad, tag, operation, &mut tags_file)?;
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