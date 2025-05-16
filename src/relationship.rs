use std::fs;
use std::path::Path;
use std::error::Error;
use std::collections::{HashSet};

use crate::common::{TagType, EntsTag, TagsFile};

#[derive(Debug, Clone, Copy)]
pub enum Operation {
    Unknown,
    Add,
    Remove,
}

impl From<&str> for Operation {
    fn from(operation: &str) -> Self {
        match operation {
            "assign" | "add" => Operation::Add,
            "remove" | "rm" => Operation::Remove,
            _ => Operation::Unknown,
        }
    }
}

pub fn is_visible_tag(tag: &EntsTag) -> bool {
    tag.show.unwrap_or(true)
}

pub fn read_tags_from_json() -> Result<TagsFile, Box<dyn Error>> {
    let json_content = fs::read_to_string("tags.json")?;
    let tags_file: TagsFile = serde_json::from_str(&json_content)?;
    Ok(tags_file)
}

pub fn save_tags_to_json(tags_file: &TagsFile) -> Result<(), Box<dyn Error>> {
    let json_content = serde_json::to_string_pretty(tags_file)?;
    fs::write("tags.json", json_content)?;
    Ok(())
}

pub fn assign_bidir_file_tag_rel(
    file: &str, 
    tag: &str, 
    operation: Operation, 
    tags_file: &mut TagsFile
) -> Result<(), Box<dyn Error>> {
    // Resolve the actual tag name from aliases
    let display_tag_name = match tags_file.aliases.get(tag) {
        Some(actual_name) => actual_name,
        None => tag,
    };
    
    // Find the tag in the tags list
    let foo_index = tags_file.tags.iter().position(|t| 
        t.name == display_tag_name && is_visible_tag(t));
    
    let foo_index = match foo_index {
        Some(index) => index,
        None => {
            println!("tag or alias does not exist: {}", tag);
            return Ok(());
        }
    };

    match operation {
        Operation::Add => {
            // Check if file exists
            if !Path::new(file).exists() {
                println!("file does not exist: {}", file);
                return Ok(());
            }
            
            let foo = &tags_file.tags[foo_index];
            
            match foo.tag_type {
                TagType::Dud => {
                    println!("cannot assign dud tag to files: \t{}", display_tag_name);
                    return Ok(());
                },
                TagType::Exclusive => {
                    let bar = single_inspect(tags_file, file)?;
                    let (_, qux) = collect_tags_recursively(tag, tags_file)?;
                    let common_elements: HashSet<_> = bar.intersection(&qux).cloned().collect();
                    
                    if !common_elements.is_empty() {

                        let elements_str = common_elements.iter()
                            .map(|s| s.as_str())
                            .collect::<Vec<&str>>()
                            .join(", ");
                            
                        println!("cannot assign exclusive tag {} to file {} due to children {}", 
                            tag, file, elements_str);
                        return Ok(());
                    }
                },

                TagType::Normal => {
                    let bar = single_inspect(tags_file, file)?;
                    let ancestry_set: HashSet<String> = foo.ancestry.iter().cloned().collect();
                    let common_elements: HashSet<_> = ancestry_set.intersection(&bar).cloned().collect();
                    
                    if !common_elements.is_empty() {

                        let ancestor_tag = common_elements.iter().next().unwrap();

                        println!("cannot assign normal tag {} to file {} due to it having been assigned ancestor exclusive tag {}", 
                            tag, file, ancestor_tag);
                        return Ok(());
                    }
                }
            }
            
            // Add file to tag's files if not already present
            let foo = &mut tags_file.tags[foo_index];
            let files = foo.files.get_or_insert_with(Vec::new);
            if !files.contains(&file.to_string()) {
                files.push(file.to_string());
                println!("assigned  file, tag: \t{} \t{}", file, display_tag_name);
            } else {
                println!("pre-exist file, tag: \t{} \t{}", file, display_tag_name);
            }
        },
        Operation::Remove => {
            // Remove file from tag's files if present
            let foo = &mut tags_file.tags[foo_index];
            if let Some(files) = &mut foo.files {
                if let Some(pos) = files.iter().position(|f| f == file) {
                    files.remove(pos);
                    println!("removed file, tag: \t{} \t{}", file, tag);
                } else {
                    println!("there is no correlation between file '{}' and tag '{}'", file, display_tag_name);
                }
            } else {
                println!("there is no correlation between file '{}' and tag '{}'", file, display_tag_name);
            }
        },
        Operation::Unknown => {
            println!("invalid operation");
        }
    }
    
    Ok(())
}

fn collect_tags_recursively(tag_name: &str, tags_file: &TagsFile) 
    -> Result<(HashSet<String>, HashSet<String>), Box<dyn Error>> {
    
    // Resolve actual tag name from aliases
    let display_tag_name = match tags_file.aliases.get(tag_name) {
        Some(actual_name) => actual_name,
        None => tag_name,
    };
    
    // Find the tag in the tags list
    let tag_obj = tags_file.tags.iter()
        .find(|t| t.name == display_tag_name && is_visible_tag(t))
        .ok_or_else(|| format!("tag '{}' is not in tags", tag_name))?;
    
    let mut normal_and_duds_set = HashSet::new();
    let mut normal_tags_set = HashSet::new();
    
    // Recursive helper function to collect tags
    fn edit_lists(
        tag_object: &EntsTag, 
        all_tags: &[EntsTag],
        normal_and_duds_set: &mut HashSet<String>, 
        normal_tags_set: &mut HashSet<String>
    ) {
        // Verify tag type
        if tag_object.tag_type != TagType::Normal && 
           tag_object.tag_type != TagType::Dud && 
           tag_object.tag_type != TagType::Exclusive {
            println!("tag '{}' is of invalid type '{:?}'", tag_object.name, tag_object.tag_type);
            return;
        }
        
        // Add to normal_and_duds_set
        normal_and_duds_set.insert(tag_object.name.clone());
        
        // Add to normal_tags_set if applicable
        if tag_object.tag_type == TagType::Normal || tag_object.tag_type == TagType::Exclusive {
            normal_tags_set.insert(tag_object.name.clone());
        }
        
        // Process children recursively
        for child_name in &tag_object.children {
            if let Some(child_object) = all_tags.iter()
                .find(|t| t.name == *child_name && is_visible_tag(t)) {
                edit_lists(child_object, all_tags, normal_and_duds_set, normal_tags_set);
            }
        }
    }
    
    // Start the recursive collection
    edit_lists(tag_obj, &tags_file.tags, &mut normal_and_duds_set, &mut normal_tags_set);
    
    Ok((normal_and_duds_set, normal_tags_set))
}

fn single_inspect(tags_file: &TagsFile, file: &str) -> Result<HashSet<String>, Box<dyn Error>> {
    let mut return_set = HashSet::new();
    
    for tag in &tags_file.tags {
        if is_visible_tag(tag) {
            if let Some(files) = &tag.files {
                if files.contains(&file.to_string()) {
                    return_set.insert(tag.name.clone());
                }
            }
        }
    }
    
    Ok(return_set)
}

pub fn filter_command(tags_file: &TagsFile, tags: &[String]) -> Result<Vec<String>, Box<dyn Error>> {
    let mut all_normal_tags = HashSet::new();
    
    for tag in tags {
        let (_, normal_tags_set) = collect_tags_recursively(tag, tags_file)?;
        all_normal_tags.extend(normal_tags_set);
    }
    
    let mut unique_files = HashSet::new();
    for tag_name in &all_normal_tags {
        if let Some(tag_obj) = tags_file.tags.iter()
            .find(|tag| tag.name == *tag_name && is_visible_tag(tag)) {
            if let Some(files) = &tag_obj.files {
                unique_files.extend(files.iter().cloned());
            }
        }
    }
    
    let mut result: Vec<String> = unique_files.into_iter().collect();
    result.sort();
    Ok(result)
}

pub fn represent_inspect(tags_file: &TagsFile, files: &[String]) -> Result<(), Box<dyn Error>> {
    let multi_display = files.len() > 1;
    let tab_container = if multi_display { "\t" } else { "" };

    for (_count, file) in files.iter().enumerate() {
        let element = single_inspect(tags_file, file)?;
        
        if multi_display {
            let header_length = std::cmp::max(20, file.len() + 5);
            let padding = header_length - file.len();
            println!("\n====={}{}=", file, "=".repeat(padding));
        }

        for tag in element {
            println!("{}{}", tab_container, tag);
        }
    }
    
    Ok(())
}