use pest::Parser;
use pest_derive::Parser;
use std::collections::HashMap;
use std::fs;

#[derive(Parser)]
#[grammar = "ents.pest"]
struct EntsParser;

#[derive(Debug, Clone)]
enum TagType {
    Normal,
    Dud,
    Exclusive,
}

#[derive(Debug, Clone)]
struct Tag {
    name: String,
    tag_type: TagType,
    alias: Option<String>,
    children: Vec<Tag>,
    ancestry: Vec<String>,
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // Read the ENTS file
    let ents_content = fs::read_to_string("tags.ents")?;
    
    // Parse the file
    let pairs = EntsParser::parse(Rule::file, &ents_content)?;
    
    // Process the parsed content
    let mut tags = Vec::new();
    let mut aliases = HashMap::new();
    
    // Track indentation levels and current tags at each level
    let mut level_tags: Vec<Vec<Tag>> = vec![Vec::new()];
    
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
                tags = level_tags[0].clone();
            }
            _ => {}
        }
    }
    
    // Print the parsed result
    println!("{:#?}", tags);
    println!("Aliases: {:#?}", aliases);
    
    Ok(())
}

fn process_tag(
    tag_pair: pest::iterators::Pair<Rule>,
    level_tags: &mut Vec<Vec<Tag>>,
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
    let tag = Tag {
        name,
        tag_type,
        alias,
        children: Vec::new(), // Will be populated later
        ancestry,
    };
    
    // Add the tag to the current level
    level_tags[indent_level].push(tag);
    
    // If there's a parent tag, add this as a child
    if indent_level > 0 {
        if let Some(parent) = level_tags[indent_level - 1].last_mut() {
            // Get a clone of the tag we just added
            if let Some(child) = level_tags[indent_level].last() {
                parent.children.push(child.clone());
            }
        }
    }
}
