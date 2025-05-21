use pest::Parser;
use pest_derive::Parser;
use std::collections::HashMap;
use std::fs;
use std::error::Error;
use std::path::Path;

// Import the unified types from common.rs
use crate::common::{TagType, EntsTag, TagsFile};

#[derive(Parser)]
#[grammar = "grammar.pest"] // This is included at compile time
struct EntsParser;

impl EntsTag {
    pub fn new(name: String, tag_type: TagType, ancestry: Vec<String>) -> Self {
        EntsTag {
            name,
            tag_type,
            children: Vec::new(),
            ancestry,
            show: Some(true),
            files: Some(Vec::new()),
            child_tags: Vec::new(),
            alias: None,
        }
    }
    
    // Call this before serialization to convert child_tags to children names
    pub fn finalize(&mut self) {
        // Extract children names from child_tags
        self.children = self.child_tags.iter()
            .map(|tag| tag.name.clone())
            .collect();
        
        // Recursively finalize children
        for child in &mut self.child_tags {
            child.finalize();
        }
    }
}

fn process_tag(
    tag_pair: pest::iterators::Pair<Rule>,
    level_tags: &mut Vec<Vec<EntsTag>>,
    aliases: &mut HashMap<String, String>
) {
    let mut indent_level = 0;
    let mut tag_type = TagType::Normal;
    let mut name = String::new();
    let mut alias = None;
    
    for inner_pair in tag_pair.into_inner() {
        match inner_pair.as_rule() {
            Rule::indent => {
                // Calculate indentation level (4 spaces = 1 level)
                indent_level = inner_pair.as_str().len() / 4;
            }
            Rule::tag_type => {
                for type_pair in inner_pair.into_inner() {
                    match type_pair.as_rule() {
                        Rule::normal_tag => tag_type = TagType::Normal,
                        Rule::dud_tag => tag_type = TagType::Dud,
                        Rule::exclusive_tag => tag_type = TagType::Exclusive,
                        _ => {}
                    }
                }
            }
            Rule::tag_name => {
                name = inner_pair.as_str().trim().to_string();
            }
            Rule::alias => {
                for alias_pair in inner_pair.into_inner() {
                    if alias_pair.as_rule() == Rule::alias_name {
                        let alias_name = alias_pair.as_str().trim().to_string();
                        alias = Some(alias_name.clone());
                        aliases.insert(alias_name, name.clone());
                    }
                }
            }
            _ => {}
        }
    }
    
    // Ensure we have enough levels in our vectors
    while level_tags.len() <= indent_level {
        level_tags.push(Vec::new());
    }
    
    // Calculate ancestry based on parent tags
    let mut ancestry = Vec::new();
    for level in 0..indent_level {
        if let Some(parent) = level_tags[level].last() {
            ancestry.push(parent.name.clone());
        }
    }
    
    // Create the tag
    let mut tag = EntsTag::new(name, tag_type, ancestry);
    tag.alias = alias;
    
    // If there's a parent tag, we need to clone this tag before adding it
    // to avoid the borrow checker error
    if indent_level > 0 {
        // Clone the tag we're about to add
        let tag_clone = tag.clone();
        
        // Add the tag to the current level
        level_tags[indent_level].push(tag);
        
        // Now we can add the clone to the parent's child_tags
        if let Some(parent) = level_tags[indent_level - 1].last_mut() {
            parent.child_tags.push(tag_clone);
        }
    } else {
        // No parent, just add the tag to the current level
        level_tags[indent_level].push(tag);
    }
}


// Prepare a flat list of all tags from the hierarchy
fn flatten_tags(root_tags: Vec<EntsTag>) -> Vec<EntsTag> {
    let mut all_tags = Vec::new();
    
    // Recursive function to add a tag and all its children to the result vector
    fn add_tag_and_children(tag: EntsTag, all_tags: &mut Vec<EntsTag>) {
        let children = tag.child_tags.clone();
        
        // Add current tag (without children to avoid duplication)
        let mut tag_without_children = tag.clone();
        tag_without_children.child_tags = Vec::new();
        all_tags.push(tag_without_children);
        
        // Add all children
        for child in children {
            add_tag_and_children(child, all_tags);
        }
    }
    
    // Process all root tags
    for tag in root_tags {
        add_tag_and_children(tag, &mut all_tags);
    }
    
    all_tags
}

// Main parse function that returns a TagsFile
pub fn parse_ents(file_path: &str) -> Result<TagsFile, Box<dyn Error>> {
    // Get current directory
    let current_dir = std::env::current_dir()?;
    println!("Current directory: {:?}", current_dir);
    
    // Convert relative path to absolute path
    let absolute_path = if Path::new(file_path).is_absolute() {
        Path::new(file_path).to_path_buf()
    } else {
        current_dir.join(file_path)
    };
    
    println!("Absolute file path: {:?}", absolute_path);
    println!("File exists: {}", absolute_path.exists());
    
    // Try to read with absolute path
    if !absolute_path.exists() {
        println!("{:?} not found", absolute_path);
        return Err(Box::new(std::io::Error::new(
            std::io::ErrorKind::NotFound,
            format!("{:?} not found", absolute_path)
        )));
    }

    // Read the ENTS file
    let ents_content = match fs::read_to_string(&absolute_path) {
        Ok(content) => content,
        Err(e) => {
            println!("Error reading {:?}: {}", absolute_path, e);
            return Err(Box::new(e));
        }
    };

    // Parse the file
    let pairs = EntsParser::parse(Rule::file, &ents_content)?;
    
    // Process the parsed content
    let mut root_tags = Vec::new();
    let mut aliases = HashMap::new();
    
    // Track indentation levels and current tags at each level
    let mut level_tags: Vec<Vec<EntsTag>> = vec![Vec::new()];
    
    for pair in pairs {
        match pair.as_rule() {
            Rule::file => {
                // Process the tags in the file
                for tag_pair in pair.into_inner() {
                    if tag_pair.as_rule() == Rule::tag {
                        process_tag(tag_pair, &mut level_tags, &mut aliases);
                    }
                }
                
                // The root tags are at level 0
                root_tags = level_tags[0].clone();
            }
            _ => {}
        }
    }
    
    // Debug prints (remove in production)
    println!("Parsed {} root tags", root_tags.len());
    println!("Found {} aliases", aliases.len());
    
    // Finalize all tags (convert child_tags to children names)
    for tag in &mut root_tags {
        tag.finalize();
    }
    
    // Create a flat list of all tags
    let all_tags = flatten_tags(root_tags);
    
    // Create and return the TagsFile
    Ok(TagsFile {
        aliases,
        tags: all_tags,
    })
}