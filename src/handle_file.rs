use std::fs;
use std::path::{Path, PathBuf};
use std::error::Error;
use std::os::unix::fs::MetadataExt;
use jwalk::WalkDir;
use crate::common::{TagsFile, FileData};

struct FileLocation {
    path: PathBuf,
    file_inode: u64,
    parent_dir_inode: u64,
}

pub fn handle_file(file_path: &str, jf: &mut TagsFile) -> Result<u64, Box<dyn Error>> {
    if jf.files.is_empty() {
        jf.files = Vec::new();
    }
    
    if let Some(existing_file) = jf.files.iter().find(|file| file.last_known_name == file_path) { 
        return Ok(existing_file.file_inode);
    }
    
    match find_file_with_inodes(file_path)? {
        Some(location) => {
            let file_inode = location.file_inode;    
            if let Some(position) = jf.files.iter().position(|file| file.file_inode == location.file_inode) {
                jf.files[position].last_known_name = location.path.to_string_lossy().to_string();
                jf.files[position].parent_dir_inode = location.parent_dir_inode;
            } else {
                let new_file = FileData {
                    last_known_name: location.path.to_string_lossy().to_string(),
                    file_inode: location.file_inode,
                    parent_dir_inode: location.parent_dir_inode,
                };
                jf.files.push(new_file);
            }
            Ok(file_inode) 
        },
        None => {
            Err(format!("File '{}' not found in any directory", file_path).into())
        }
    }
}

fn find_file_with_inodes(file_path: &str) -> Result<Option<FileLocation>, Box<dyn Error>> {
    let path = Path::new(file_path);
    
    if path.exists() {
        let file_metadata = fs::metadata(path)?;
        let file_inode = file_metadata.ino();
        
        // Fix for empty parent path - always use "." for current directory
        let parent_path = if let Some(parent) = path.parent() {
            if parent.as_os_str().is_empty() {
                Path::new(".")
            } else {
                parent
            }
        } else {
            Path::new(".")
        };
        
        let parent_metadata = fs::metadata(parent_path)?;
        let parent_dir_inode = parent_metadata.ino();
        
        let current_dir = std::env::current_dir()?;
        let relative_path = if path.is_absolute() {
            if let Ok(rel_path) = path.strip_prefix(&current_dir) {
                rel_path.to_path_buf()
            } else {
                path.to_path_buf()
            }
        } else {
            path.to_path_buf()
        };
        
        return Ok(Some(FileLocation {
            path: relative_path,
            file_inode,
            parent_dir_inode,
        }));
    }
    
    let file_name = match path.file_name() {
        Some(name) => name,
        None => return Ok(None),
    };
    
    for entry in WalkDir::new(".").parallelism(jwalk::Parallelism::RayonNewPool(4)) {
        match entry {
            Ok(entry) => {
                if entry.file_name.eq_ignore_ascii_case(file_name) {
                    let found_path = entry.path();
                    let file_metadata = fs::metadata(&found_path)?;
                    let file_inode = file_metadata.ino();
                    
                    // Same fix for parent path
                    let parent_path = if let Some(parent) = found_path.parent() {
                        if parent.as_os_str().is_empty() {
                            Path::new(".")
                        } else {
                            parent
                        }
                    } else {
                        Path::new(".")
                    };
                    
                    let parent_metadata = fs::metadata(parent_path)?;
                    let parent_dir_inode = parent_metadata.ino();
                    
                    let current_dir = std::env::current_dir()?;
                    let relative_path = if found_path.is_absolute() {
                        if let Ok(rel_path) = found_path.strip_prefix(&current_dir) {
                            rel_path.to_path_buf()
                        } else {
                            found_path
                        }
                    } else {
                        found_path
                    };
                    
                    return Ok(Some(FileLocation {
                        path: relative_path,
                        file_inode,
                        parent_dir_inode,
                    }));
                }
            },
            Err(e) => {
                eprintln!("Error during directory traversal: {}", e);
            }
        }
    }
    
    Ok(None)
}


pub fn find_filename_by_inode(target_inode: u64) -> Result<Option<String>, Box<dyn Error>> {
    //println!("Searching for file with inode: {}", target_inode);
    
    // Start recursive search from current directory
    for entry in WalkDir::new(".").parallelism(jwalk::Parallelism::RayonNewPool(4)) {
        match entry {
            Ok(entry) => {
                // Skip directories to speed up the search (optional)
                if entry.file_type.is_dir() {
                    continue;
                }
                
                // Get the full path
                let path = entry.path();
                
                // Get metadata to check inode
                match std::fs::metadata(&path) {
                    Ok(metadata) => {
                        let file_inode = metadata.ino();
                        
                        // Check if this is the file we're looking for
                        if file_inode == target_inode {
                            //println!("Found matching file: {:?}", path);
                            return Ok(Some(path.to_string_lossy().to_string()));
                        }
                    },
                    Err(e) => {
                        // Log the error but continue searching
                        eprintln!("Error reading metadata for {:?}: {}", path, e);
                    }
                }
            },
            Err(e) => {
                eprintln!("Error during directory traversal: {}", e);
            }
        }
    }
    
    // If we get here, no matching file was found
    //println!("No file with inode {} found", target_inode);
    Ok(None)
}