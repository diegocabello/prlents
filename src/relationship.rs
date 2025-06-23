use std::error::Error;
use std::collections::{HashSet};
use std::os::unix::fs::MetadataExt;
use std::path::Path;

use crate::common::{TagType, EntsTag, TagsFile, FileData, save_tags_to_json};
use crate::handle_file::{handle_file, find_filename_by_inode};

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

pub fn assign_bidir_file_tag_rel(
    file_name: &str, 
    tag: &str, 
    operation: Operation, 
    tags_file: &mut TagsFile
) -> Result<(), Box<dyn Error>> {

    // Look up the inode early to avoid borrowing conflicts
    let file_inode = handle_file(file_name, tags_file)?;
    let file_inode_str = file_inode.to_string();

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
            let foo = &tags_file.tags[foo_index];
            
            match foo.tag_type {
                TagType::Dud => {
                    println!("cannot assign dud tag to files: \t{}", display_tag_name);
                    return Ok(());
                },
                TagType::Exclusive => {
                    let bar = single_inspect(tags_file, &file_inode_str)?;
                    let (_, qux) = collect_tags_recursively(tag, tags_file)?;
                    let common_elements: HashSet<_> = bar.intersection(&qux).cloned().collect();
                    
                    if !common_elements.is_empty() {
                        let elements_str = common_elements.iter()
                            .map(|s| s.as_str())
                            .collect::<Vec<&str>>()
                            .join(", ");
                            
                        println!("cannot assign exclusive tag {} to file {} due to children {}", 
                            tag, file_name, elements_str);
                        return Ok(());
                    }
                },

                TagType::Normal => {
                    let bar = single_inspect(tags_file, &file_inode_str)?;
                    let ancestry_set: HashSet<String> = foo.ancestry.iter().cloned().collect();
                    let common_elements: HashSet<_> = ancestry_set.intersection(&bar).cloned().collect();
                    
                    if !common_elements.is_empty() {
                        // Check if any of the common ancestors are actually exclusive tags
                        for ancestor_name in &common_elements {
                            // Find the ancestor tag and check its type
                            if let Some(ancestor_tag) = tags_file.tags.iter().find(|t| 
                                t.name == *ancestor_name && is_visible_tag(t)) {
                                
                                if ancestor_tag.tag_type == TagType::Exclusive {
                                    println!("cannot assign normal tag {} to file {} due to it having been assigned ancestor exclusive tag {}", 
                                        tag, file_name, ancestor_name);
                                    return Ok(());
                                }
                            }
                        }
                        // If we get here, none of the ancestors are exclusive tags, so assignment is allowed
                    }
                }
            }
            
            // Add file to tag's files if not already present
            let foo = &mut tags_file.tags[foo_index];
            let files = foo.files.get_or_insert_with(Vec::new);

            if !files.contains(&file_inode_str) {
                files.push(file_inode_str);
                println!("assigned  file, tag: \t{} \t{}", file_name, display_tag_name);
            } else {
                println!("pre-exist file, tag: \t{} \t{}", file_name, display_tag_name);
            }
        },
        Operation::Remove => {
            // Remove file from tag's files if present
            let foo = &mut tags_file.tags[foo_index];
            if let Some(files) = &mut foo.files {
                if let Some(pos) = files.iter().position(|f| *f == file_inode_str) {
                    files.remove(pos);
                    println!("removed file, tag: \t{} \t{}", file_name, tag);
                } else {
                    println!("there is no correlation between file '{}' and tag '{}'", file_name, display_tag_name);
                }
            } else {
                println!("there is no correlation between file '{}' and tag '{}'", file_name, display_tag_name);
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


pub fn filter_command(tags_file: &mut TagsFile, tags: &[String]) -> Result<Vec<String>, Box<dyn Error>> {
    
    let mut all_normal_tags = HashSet::new();
    
    for tag in tags {
        let (_, normal_tags_set) = collect_tags_recursively(tag, tags_file)?;
        all_normal_tags.extend(normal_tags_set);
    }
    
    let mut unique_inodes = HashSet::new();

    for tag_name in &all_normal_tags {
        if let Some(tag_obj) = tags_file.tags.iter()
            .find(|tag| tag.name == *tag_name && is_visible_tag(tag)) {
            if let Some(files) = &tag_obj.files {
                unique_inodes.extend(files.iter().cloned());
            }
        }
    }
    
    // Track whether we need to save changes
    let mut needs_save = false;
    
    // Convert inodes to filenames and update last_known_name if needed
    let mut result: Vec<String> = Vec::new();
    
    for inode_str in &unique_inodes {
        // Convert string to u64 inode
        if let Ok(inode) = inode_str.parse::<u64>() {

            // if it finds the inode
            if let Some((position, file_data)) = tags_file.files.iter().enumerate().find(|(_, file_data)| file_data.file_inode == inode) {
                let last_known_name = &file_data.last_known_name;
                // and if the last known file there
                if Path::new(last_known_name).is_file() {
                    result.push(last_known_name.clone());
                // if the file name changed
                } else {
                    // Look up current filename by inode using the file system
                    match find_filename_by_inode(inode)? {
                        Some(current_path) => {
                            // File exists in our registry, check if name needs updating
                            if tags_file.files[position].last_known_name != current_path {
                                //println!("Updating file path: {} -> {}", tags_file.files[position].last_known_name, current_path);
                                tags_file.files[position].last_known_name = current_path.clone();
                                needs_save = true;
                            }
                            result.push(current_path);
                        },
                        None => {
                            // File not found in filesystem - do not include it in results
                            println!("Warning: File with inode {} not found in filesystem", inode);
                            // We don't add it to the results since you don't want to show missing files
                        }
                    }
                }
            }
        }
    }
    
    result.sort();
    
    // Save changes to tags.json if needed
    if needs_save {
        save_tags_to_json(tags_file)?;
    }
    
    Ok(result)
}

// Modified to accept inode string directly instead of filename
fn represent_single_inspect(tags_file: &TagsFile, file_inode_str: &str) -> Result<HashSet<String>, Box<dyn Error>> {
    let mut return_set = HashSet::new();
    
    for tag in &tags_file.tags {
        if is_visible_tag(tag) {
            if let Some(files) = &tag.files {
                if files.contains(&file_inode_str.to_string()) {
                    if !tag.ancestry.is_empty() {
                        let mut path_parts = tag.ancestry.clone();
                        path_parts.push(tag.name.clone());
                        let full_tag_path = path_parts.join("/");
                        return_set.insert(full_tag_path);
                    } else {
                        return_set.insert(tag.name.clone());
                    }
                }
            }
        }
    }
    
    Ok(return_set)
}

fn single_inspect(tags_file: &TagsFile, file_inode_str: &str) -> Result<HashSet<String>, Box<dyn Error>> {
    let mut return_set = HashSet::new();
    
    for tag in &tags_file.tags {
        if is_visible_tag(tag) {
            if let Some(files) = &tag.files {
                if files.contains(&file_inode_str.to_string()) {
                    return_set.insert(tag.name.clone());  // Just the name, not the path
                }
            }
        }
    }
    
    Ok(return_set)
}

pub fn represent_inspect(tags_file: &mut TagsFile, files: &[String]) -> Result<(), Box<dyn Error>> {
    let multi_display = files.len() > 1;
    let tab_container = if multi_display { "\t" } else { "" };

    for (_count, file) in files.iter().enumerate() {
        // Look up the inode first
        let file_inode = handle_file(file, tags_file)?;
        let file_inode_str = file_inode.to_string();
        
        // Then call single_inspect with the inode string
        let element = represent_single_inspect(tags_file, &file_inode_str)?;
        
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