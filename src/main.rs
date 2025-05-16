use std::path::Path;
use std::error::Error;
use std::env;

mod common;
mod relationship;
//mod merge_tags;

use crate::common::{TagType, EntsTag, TagsFile};

use relationship::{
    //TagType, EntsTag, TagsFile, Operation, 
    Operation, 
    is_visible_tag, read_tags_from_json, save_tags_to_json,
    assign_bidir_file_tag_rel, filter_command, represent_inspect
};

//use merge_tags::{merge_tags_files}

fn main() -> Result<(), Box<dyn Error>> {
    let args: Vec<String> = env::args().collect();
    
    if args.len() < 3 {
        println!("Usage: prlents <ttf|ftt|fil> [(<add|remove|show> <monad> <opt1> <opt2> ...) | (<tag1> <tag2> ...)]");
        return Ok(());
    }

    let command = &args[1];
    
    let mut tags_file = read_tags_from_json()?;
    
    
    // if command == "process" || command == "parse" {

    // } else 
    if command == "filter" || command == "fil" {
        let tags = args[2..].to_vec();
        for file in filter_command(&tags_file, &tags)? {
            println!("{}", file.trim());
        }
    } else if command == "inspect" || command == "insp" {
        let files = args[2..].to_vec();
        represent_inspect(&tags_file, &files)?;
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