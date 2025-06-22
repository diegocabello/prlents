use serde::{Serialize, Deserialize};
use std::collections::HashMap;
use std::error::Error;
use std::fs;
use std::io;

#[derive(Serialize, Deserialize, Debug, PartialEq, Clone)]
pub enum TagType {
    #[serde(rename = "normal")]
    Normal,
    #[serde(rename = "dud")]
    Dud,
    #[serde(rename = "exclusive")]
    Exclusive,
}

// Unified tag structure for both parsing and serialization
#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct EntsTag {
    pub name: String,
    #[serde(rename = "type")]
    pub tag_type: TagType,
    pub children: Vec<String>, //this is inodes now
    pub ancestry: Vec<String>, //this is inodes now
    pub show: Option<bool>,
    pub files: Option<Vec<String>>, //this is inodes now
    
    // Fields used during parsing, skipped during serialization
    #[serde(skip)]
    pub child_tags: Vec<EntsTag>,
    #[serde(skip)]
    pub alias: Option<String>,
}

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

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct FileData {
    pub last_known_name: String,
    pub file_inode: u64,
    pub parent_dir_inode: u64,
    // pub sha1_hash: [u8; 40],
    // pub fuzzy_hash: [u8; 70]
}

#[derive(Serialize, Deserialize, Debug, Clone, Default)]
pub struct TagsFile {
    pub files: Vec<FileData>,
    pub aliases: HashMap<String, String>,
    pub tags: Vec<EntsTag>,
}

pub fn read_tags_from_json() -> Result<TagsFile, Box<dyn Error>> {
    match fs::read_to_string("tags.json") {
        Ok(json_content) => {
            let tags_file: TagsFile = serde_json::from_str(&json_content)?;
            Ok(tags_file)
        },
        Err(e) if e.kind() == std::io::ErrorKind::NotFound => {
            println!("Error: tags.json not found. Run 'prlents process tags.ents' to create it.");
            Ok(TagsFile::default())  
        },
        Err(e) => Err(e.into())
    }
}

pub fn save_tags_to_json(tags_file: &TagsFile) -> Result<(), Box<dyn Error>> {
    let json_content = serde_json::to_string_pretty(tags_file)?;
    fs::write("tags.json", json_content)?;
    Ok(())
}
