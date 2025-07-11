use nom::{
    IResult,
    branch::alt,
    bytes::complete::{tag, take_while, take_while1, is_not},
    character::complete::char,
    combinator::{opt, map, eof},
    sequence::{preceded, delimited, tuple},
};
use std::collections::HashMap;
use std::error::Error;
use std::fs;

// Import the unified types from common.rs
use crate::common::{TagType, EntsTag, TagsFile};

#[derive(Debug, Clone)]
struct ParsedTag {
    indent: usize,
    tag_type: TagType,
    name: String,
    alias: Option<String>,
}

// Parse tag type: -, +, +-, -+
fn parse_tag_type(input: &str) -> IResult<&str, TagType> {
    alt((
        map(alt((tag("+-"), tag("-+"))), |_| TagType::Exclusive),
        map(char('+'), |_| TagType::Dud),
        map(char('-'), |_| TagType::Normal),
    ))(input)
}

// Parse spaces (not tabs or newlines)
fn parse_spaces(input: &str) -> IResult<&str, &str> {
    take_while(|c| c == ' ')(input)
}

// Parse required spaces (at least one)
fn parse_spaces1(input: &str) -> IResult<&str, &str> {
    take_while1(|c| c == ' ')(input)
}

// Parse indentation (must be multiple of 4 spaces)
fn parse_indent(input: &str) -> IResult<&str, usize> {
    map(parse_spaces, |spaces| spaces.len())(input)
}

// Parse escaped character
fn parse_escaped_char(c: char) -> impl Fn(&str) -> IResult<&str, char> {
    move |input: &str| {
        preceded(char('\\'), char(c))(input)
    }
}

// Parse tag name (everything until '(', ':', or newline, handling escapes)
fn parse_tag_name(input: &str) -> IResult<&str, String> {
    let mut result = String::new();
    let mut remaining = input;
    
    loop {
        // Try to parse escaped characters first
        if let Ok((rest, ch)) = alt((
            parse_escaped_char('('),
            parse_escaped_char(')'),
            parse_escaped_char(':'),
        ))(remaining) {
            result.push(ch);
            remaining = rest;
            continue;
        }
        
        // Check for terminators
        if remaining.is_empty() || 
           remaining.starts_with('(') || 
           remaining.starts_with(':') || 
           remaining.starts_with('\n') || 
           remaining.starts_with('\r') {
            break;
        }
        
        // Take one character
        if let Some(ch) = remaining.chars().next() {
            result.push(ch);
            remaining = &remaining[ch.len_utf8()..];
        } else {
            break;
        }
    }
    
    // Trim the result
    let trimmed = result.trim();
    
    if trimmed.is_empty() {
        return Err(nom::Err::Error(nom::error::Error::new(input, nom::error::ErrorKind::TakeWhile1)));
    }
    
    let consumed_len = input.len() - remaining.len();
    Ok((&input[consumed_len..], trimmed.to_string()))
}

// Parse alias in parentheses
fn parse_alias(input: &str) -> IResult<&str, String> {
    delimited(
        char('('),
        map(is_not(")"), |s: &str| s.trim().to_string()),
        char(')')
    )(input)
}

// Parse a complete tag line
fn parse_tag_line(input: &str) -> IResult<&str, ParsedTag> {
    let original_input = input;
    let (input, indent) = parse_indent(input)?;
    
    // Verify indent is multiple of 4
    if indent % 4 != 0 {
        println!("Invalid indent: {} spaces", indent);
        return Err(nom::Err::Error(nom::error::Error::new(
            input, 
            nom::error::ErrorKind::Verify
        )));
    }
    
    let (input, tag_type) = parse_tag_type(input)?;
    let (input, spaces) = parse_spaces1(input)?;
    // println!("After tag type '{}', found {} spaces",   // ===DEBUG===
    //     match tag_type {
    //         TagType::Normal => "-",
    //         TagType::Dud => "+", 
    //         TagType::Exclusive => "+-",
    //     }, 
    //     spaces.len()
    // );
    let (input, name) = parse_tag_name(input)?;
    
    // After the tag name, we might have:
    // 1. Nothing (end of line)
    // 2. Spaces followed by alias
    // 3. Spaces followed by colon
    // 4. Alias followed by optional colon
    
    let (input, _) = parse_spaces(input)?;
    let (input, alias) = opt(parse_alias)(input)?;
    let (input, _) = parse_spaces(input)?;
    let (input, _) = opt(char(':'))(input)?;
    let (input, _) = parse_spaces(input)?; // Allow trailing spaces
    
    Ok((input, ParsedTag {
        indent: indent / 4, // Convert to indentation level
        tag_type,
        name,
        alias,
    }))
}

// Parse newline (handles \n, \r\n, or \r)
fn parse_newline(input: &str) -> IResult<&str, ()> {
    alt((
        map(tag("\r\n"), |_| ()),
        map(tag("\n"), |_| ()),
        map(tag("\r"), |_| ()),
    ))(input)
}

// Parse a single line (tag + optional newline)
fn parse_line(input: &str) -> IResult<&str, Option<ParsedTag>> {
    alt((
        // Empty line (just newline)
        map(parse_newline, |_| None),
        // Tag line followed by newline or EOF
        map(
            tuple((
                parse_tag_line,
                alt((
                    map(parse_newline, |_| ()),
                    map(eof, |_| ()),
                )),
            )),
            |(tag, _)| Some(tag)
        ),
    ))(input)
}

// Parse entire file
fn parse_ents_file(input: &str) -> IResult<&str, Vec<ParsedTag>> {
    let mut remaining = input;
    let mut tags = Vec::new();
    let mut line_num = 1;
    
    while !remaining.is_empty() {
        // Skip any empty lines
        while remaining.starts_with('\n') || remaining.starts_with('\r') {
            if remaining.starts_with("\r\n") {
                remaining = &remaining[2..];
                line_num += 1;
            } else if remaining.starts_with('\n') || remaining.starts_with('\r') {
                remaining = &remaining[1..];
                line_num += 1;
            }
        }
        
        // If we've consumed all input, we're done
        if remaining.is_empty() {
            break;
        }
        
        // Try to parse a tag line
        match parse_tag_line(remaining) {
            Ok((rest, tag)) => {
                //println!("Line {}: Parsed tag '{}' (type: {:?})", line_num, tag.name, tag.tag_type); //debug
                tags.push(tag);
                remaining = rest;
                
                // Consume the line ending after the tag
                if remaining.starts_with("\r\n") {
                    remaining = &remaining[2..];
                    line_num += 1;
                } else if remaining.starts_with('\n') || remaining.starts_with('\r') {
                    remaining = &remaining[1..];
                    line_num += 1;
                }
            }
            Err(e) => {
                // If we can't parse a line and there's non-whitespace content left, that's an error
                if !remaining.trim().is_empty() {
                    println!("Failed to parse at line {}", line_num);
                    println!("Remaining content: {:?}", remaining.lines().next().unwrap_or(""));
                    return Err(nom::Err::Error(nom::error::Error::new(
                        remaining,
                        nom::error::ErrorKind::Many0
                    )));
                }
                break;
            }
        }
    }
    
    Ok(("", tags))
}

// Build hierarchy from flat parsed tags
fn build_hierarchy(parsed_tags: Vec<ParsedTag>) -> (Vec<EntsTag>, HashMap<String, String>) {
    let mut aliases = HashMap::new();
    let mut all_tags: Vec<EntsTag> = Vec::new();
    let mut tag_stack: Vec<usize> = Vec::new(); // Stack of indices into all_tags
    
    for parsed_tag in parsed_tags {
        // Add alias if present
        if let Some(alias) = &parsed_tag.alias {
            aliases.insert(alias.clone(), parsed_tag.name.clone());
        }
        
        // Adjust stack to match current indent level
        tag_stack.truncate(parsed_tag.indent);
        
        // Calculate ancestry
        let mut ancestry = Vec::new();
        for &idx in &tag_stack {
            ancestry.push(all_tags[idx].name.clone());
        }
        
        // Create the tag
        let mut tag = EntsTag {
            name: parsed_tag.name.clone(),
            tag_type: parsed_tag.tag_type,
            children: Vec::new(),
            ancestry,
            show: Some(true),
            files: None, // Set to None to match Haskell output
            child_tags: Vec::new(),
            alias: parsed_tag.alias,
        };
        
        // Add to parent's children if there is a parent
        if let Some(&parent_idx) = tag_stack.last() {
            all_tags[parent_idx].children.push(parsed_tag.name.clone());
        }
        
        // Add tag to all_tags and remember its index
        let tag_index = all_tags.len();
        all_tags.push(tag);
        
        // Push this tag's index onto the stack for potential children
        tag_stack.push(tag_index);
    }
    
    // Return all tags and aliases
    (all_tags, aliases)
}

// Since we're now returning all tags from build_hierarchy, we don't need a separate flatten function

// Main parse function that returns a TagsFile
pub fn parse_ents(file_path: &str) -> Result<TagsFile, Box<dyn Error>> {
    // Read the file
    let content = fs::read_to_string(file_path)?;
    
    // Normalize line endings to \n
    let normalized_content = content.replace("\r\n", "\n").replace("\r", "\n");
    
    // Parse the content
    let (remaining, parsed_tags) = parse_ents_file(&normalized_content)
        .map_err(|e| format!("Parse error: {:?}", e))?;
    
    // Check if we parsed the entire file
    if !remaining.trim().is_empty() {
        return Err(format!("Failed to parse entire file. Remaining: {:?}", remaining).into());
    }
    
    println!("Parsed {} tags", parsed_tags.len());
    
    // Build hierarchy and extract aliases
    let (all_tags, aliases) = build_hierarchy(parsed_tags);
    
    // Create and return the TagsFile
    Ok(TagsFile {
        files: Vec::new(), // Initialize empty files vector
        aliases,
        tags: all_tags,
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_parse_tag_type() {
        assert_eq!(parse_tag_type("-").unwrap().1, TagType::Normal);
        assert_eq!(parse_tag_type("+").unwrap().1, TagType::Dud);
        assert_eq!(parse_tag_type("+-").unwrap().1, TagType::Exclusive);
        assert_eq!(parse_tag_type("-+").unwrap().1, TagType::Exclusive);
    }
    
    #[test]
    fn test_parse_tag_name() {
        assert_eq!(parse_tag_name("hello world").unwrap().1, "hello world");
        assert_eq!(parse_tag_name("hello (alias)").unwrap().1, "hello");
        assert_eq!(parse_tag_name("hello:").unwrap().1, "hello");
        assert_eq!(parse_tag_name("hello\\(world\\)").unwrap().1, "hello(world)");
    }
    
    #[test]
    fn test_parse_alias() {
        assert_eq!(parse_alias("(test)").unwrap().1, "test");
        assert_eq!(parse_alias("(ny)").unwrap().1, "ny");
    }
    
    #[test]
    fn test_parse_simple_tag() {
        let input = "- jade\n";
        let (_, tag) = parse_tag_line(input).unwrap();
        assert_eq!(tag.indent, 0);
        assert_eq!(tag.tag_type, TagType::Normal);
        assert_eq!(tag.name, "jade");
        assert_eq!(tag.alias, None);
    }
    
    #[test]
    fn test_parse_tag_with_alias() {
        let input = "    +- new york (ny)\n";
        let (_, tag) = parse_tag_line(input).unwrap();
        assert_eq!(tag.indent, 1);
        assert_eq!(tag.tag_type, TagType::Exclusive);
        assert_eq!(tag.name, "new york");
        assert_eq!(tag.alias, Some("ny".to_string()));
    }
}
