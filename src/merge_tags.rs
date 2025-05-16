use serde::{Serialize, Deserialize};
use serde_json::{Value, Map, json};
use std::collections::HashMap;
use std::error::Error;
use std::fs;
use std::path::Path;
use std::env;
use std::string::FromUtf8Error;
use crate::relationship::{TagsFile, EntsTag, TagType};


pub fn merge_tags_files(temp_tags_file: &str, output_file: &str) -> Result<(), Box<dyn Error>> {
    // Read the new tags file
    let temp_tags_content = fs::read_to_string(temp_tags_file)?;
    let mut temp_tags_data: TagsFile = serde_json::from_str(&temp_tags_content)?;
    
    // Check if output file exists
    if !Path::new(output_file).exists() {
        // If it doesn't exist, just set all tags to show=true and save
        for tag in &mut temp_tags_data.tags {
            tag.show = Some(true);
        }
        
        // Convert to Value for pretty printing
        let json_value = serde_json::to_value(&temp_tags_data)?;
        
        // Pretty print and save in memory
        pretty_print_json(None, Some(output_file), Some(&json_value))?;
        return Ok(());
    }
    
    // If output file exists, read the existing tags
    let existing_content = fs::read_to_string(output_file)?;
    let existing_data: TagsFile = serde_json::from_str(&existing_content)?;
    
    // Create maps for quick lookup
    let existing_tags_by_name: HashMap<String, EntsTag> = existing_data.tags
        .into_iter()
        .map(|tag| (tag.name.clone(), tag))
        .collect();
    
    let new_tags_by_name: HashMap<String, EntsTag> = temp_tags_data.tags
        .iter()
        .map(|tag| (tag.name.clone(), tag.clone()))
        .collect();
    
    // Create the merged tags list
    let mut merged_tags = Vec::new();
    
    // Process tags in the new file
    for (tag_name, tag) in &new_tags_by_name {
        if let Some(existing_tag) = existing_tags_by_name.get(tag_name) {
            // Tag exists in both files, update properties
            let mut merged_tag = existing_tag.clone();
            merged_tag.tag_type = tag.tag_type.clone();
            // Note: parent field update removed as it's not in EntsTag
            merged_tag.children = tag.children.clone();
            
            // Copy ancestry field from new tags
            merged_tag.ancestry = tag.ancestry.clone();
            
            // Set to true since it's in the new file
            merged_tag.show = Some(true);
            merged_tags.push(merged_tag);
        } else {
            // Tag only in new file
            let mut new_tag = tag.clone();
            new_tag.show = Some(true);
            merged_tags.push(new_tag);
        }
    }
    
    // Process tags that are only in the existing file
    for (tag_name, tag) in &existing_tags_by_name {
        if !new_tags_by_name.contains_key(tag_name) {
            // Tag only in existing file, mark as hidden
            let mut modified_tag = tag.clone();
            modified_tag.show = Some(false);
            
            // Check if it's already been added
            if !merged_tags.iter().any(|t| t.name == *tag_name) {
                merged_tags.push(modified_tag);
            }
        }
    }
    
    // Create the final merged data
    let mut merged_data = temp_tags_data.clone();
    merged_data.tags = merged_tags;
    
    // Merge aliases from existing data
    for (alias, value) in existing_data.aliases {
        if !merged_data.aliases.contains_key(&alias) {
            merged_data.aliases.insert(alias, value);
        }
    }
    
    // Convert to Value for pretty printing
    let json_value = serde_json::to_value(&merged_data)?;
    
    // Pretty print and save directly in memory
    pretty_print_json(None, Some(output_file), Some(&json_value))?;
    
    println!("Successfully merged tags from {} into {}", temp_tags_file, output_file);
    Ok(())
}

/// Pretty prints a JSON file or Value with fields in a specific order
/// 
/// If `input_value` is provided, it will be used instead of reading from `input_file`.
/// At least one of `input_file` or `input_value` must be provided.
fn pretty_print_json(input_file: Option<&str>, output_file: Option<&str>, input_value: Option<&Value>) -> Result<(), Box<dyn Error>> {
    // Helper function to reorder fields in a Value according to a specific order
    fn reorder_fields(obj: &Value, is_tags: bool) -> Value {
        match obj {
            Value::Array(arr) => {
                let new_arr: Vec<Value> = arr.iter()
                    .map(|item| {
                        if is_tags {
                            reorder_fields(item, true)
                        } else {
                            reorder_fields(item, false)
                        }
                    })
                    .collect();
                Value::Array(new_arr)
            }
            Value::Object(map) => {
                let mut new_map = Map::new();
                
                if is_tags {
                    // Order for tag objects
                    for field in ["name", "type", "parent", "children"].iter() {
                        if let Some(value) = map.get(*field) {
                            if *field == "children" {
                                new_map.insert(field.to_string(), value.clone());
                            } else {
                                new_map.insert(field.to_string(), value.clone());
                            }
                        }
                    }
                    
                    // Add remaining fields
                    for (key, value) in map.iter() {
                        if !["name", "type", "parent", "children"].contains(&key.as_str()) {
                            new_map.insert(key.clone(), reorder_fields(value, false));
                        }
                    }
                } else {
                    if map.contains_key("tags") {
                        // Put aliases first, then tags
                        if let Some(aliases) = map.get("aliases") {
                            new_map.insert("aliases".to_string(), aliases.clone());
                        } else {
                            new_map.insert("aliases".to_string(), json!({}));
                        }
                        
                        if let Some(tags) = map.get("tags") {
                            new_map.insert("tags".to_string(), reorder_fields(tags, true));
                        }
                    } else {
                        // Just copy all fields
                        for (key, value) in map.iter() {
                            new_map.insert(key.clone(), value.clone());
                        }
                    }
                }
                
                Value::Object(new_map)
            }
            _ => obj.clone(),
        }
    }
    
    // Get the data either from file or provided value
    let data = match (input_file, input_value) {
        (Some(file), None) => {
            // Read from file
            let file_content = fs::read_to_string(file)?;
            serde_json::from_str(&file_content)?
        },
        (None, Some(value)) => {
            // Use provided value
            value.clone()
        },
        (Some(_), Some(value)) => {
            // Both provided, prioritize value
            value.clone()
        },
        (None, None) => {
            return Err(Box::new(std::io::Error::new(
                std::io::ErrorKind::InvalidInput,
                "Either input_file or input_value must be provided"
            )));
        }
    };
    
    // Helper function to pretty print a JSON Value object with proper formatting
    fn pretty_print_json_value(data: &Value, output_file: Option<&str>, reorder_fn: fn(&Value, bool) -> Value) -> Result<(), Box<dyn Error>> {
        // Reorder the fields
        let reordered_data = reorder_fn(data, false);
        
        // Create serializer options with 2-space indentation
        let mut serializer = serde_json::ser::PrettyFormatter::with_indent(b"  ");
        let buf = Vec::new();
        let mut ser = serde_json::Serializer::with_formatter(buf, serializer);
        
        // Serialize the JSON
        reordered_data.serialize(&mut ser)?;
        let formatted_json = String::from_utf8(ser.into_inner())?;
        
        // Apply additional formatting to special array cases, particularly "children" arrays
        // Split the input into lines
        let lines: Vec<&str> = formatted_json.lines().collect();
        let mut result = Vec::new();
        let mut i = 0;
        
        while i < lines.len() {
            let line = lines[i];
            
            // Check if this line starts a children array
            if line.contains("\"children\": [") && !line.contains("]") {
                // This is a multi-line children array
                let mut combined = line.to_string();
                i += 1;
                
                // Collect all the elements of the array
                while i < lines.len() && !lines[i].contains("]") {
                    let content = lines[i].trim();
                    // Add the content without a newline
                    if content.starts_with("\"") && (content.ends_with("\",") || content.ends_with("\"")) {
                        combined.push_str(content);
                    }
                    i += 1;
                }
                
                // Add the closing bracket
                if i < lines.len() {
                    combined.push_str(lines[i].trim());
                    result.push(combined);
                }
                i += 1;
            } else {
                // Handle single-line arrays
                let mut line_str = line.to_string();
                
                // Fix empty arrays
                if line_str.contains("\"children\": []") {
                    line_str = line_str.replace("\"children\": []", "\"children\": []");
                }
                
                // Fix inline arrays
                if line_str.contains("\"children\": [") && line_str.contains("]") {
                    // Make sure there aren't unnecessary spaces
                    let start_idx = line_str.find("\"children\": [").unwrap();
                    let end_idx = line_str.rfind("]").unwrap();
                    let array_content = &line_str[start_idx + 13..end_idx];
                    let trimmed_content = array_content.trim();
                    
                    let before = &line_str[0..start_idx + 13];
                    let after = &line_str[end_idx..];
                    
                    line_str = format!("{}{}{}", before, trimmed_content, after);
                }
                
                result.push(line_str);
                i += 1;
            }
        }
        
        // Join the lines and do final cleanup
        let mut output = result.join("\n");
        
        // Remove spaces between brackets and quotes
        output = output.replace("[ ", "[").replace(" ]", "]");
        
        // Write the formatted JSON
        match output_file {
            Some(path) => {
                fs::write(path, output)?;
                println!("Pretty-printed JSON saved to {}", path);
            }
            None => {
                println!("{}", output);
            }
        }
        
        Ok(())
    }
    
    // Format and write the JSON using the helper function
    pretty_print_json_value(&data, output_file, reorder_fields)
}
