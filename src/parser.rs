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

/// Represents a parsed tag line with all its components
/// This is an intermediate structure used during parsing before converting to EntsTag
#[derive(Debug, Clone)]
struct ParsedTag {
    indent: usize,      // Indentation level (0, 1, 2, etc.)
    tag_type: TagType,  // Normal (-), Dud (+), or Exclusive (*)
    name: String,       // The tag name
    alias: Option<String>, // Optional alias in parentheses
}

/// Parse tag type markers: -, +, *
/// - Normal tags are marked with `-`
/// - Dud tags are marked with `+` 
/// - Exclusive tags are marked with `*` (changed from +- or -+)
fn parse_tag_type(input: &str) -> IResult<&str, TagType> {
    alt((
        map(char('*'), |_| TagType::Exclusive),  // Changed from +- or -+ to *
        map(char('+'), |_| TagType::Dud),
        map(char('-'), |_| TagType::Normal),
    ))(input)
}

/// Parse zero or more spaces (not tabs or newlines)
/// Used for optional whitespace parsing
fn parse_spaces(input: &str) -> IResult<&str, &str> {
    take_while(|c| c == ' ')(input)
}

/// Parse one or more required spaces
/// Used after tag type markers where space is mandatory
fn parse_spaces1(input: &str) -> IResult<&str, &str> {
    take_while1(|c| c == ' ')(input)
}

/// Parse indentation and return the indentation level
/// ENTS requires indentation to be multiples of 4 spaces
/// Returns the number of indentation levels (spaces / 4)
fn parse_indent(input: &str) -> IResult<&str, usize> {
    map(parse_spaces, |spaces| spaces.len())(input)
}

/// Parse an escaped character for tag names
/// Allows escaping of special characters like (, ), and :
/// Returns a parser that matches \c where c is the specified character
fn parse_escaped_char(c: char) -> impl Fn(&str) -> IResult<&str, char> {
    move |input: &str| {
        preceded(char('\\'), char(c))(input)
    }
}

/// Parse a tag name with support for escaped characters
/// Tag names continue until they hit a terminator: (, :, newline, or end of input
/// Supports escaping of terminators with backslashes
/// Returns the trimmed tag name
fn parse_tag_name(input: &str) -> IResult<&str, String> {
    let mut result = String::new();
    let mut remaining = input;
    
    loop {
        // Try to parse escaped characters first
        // This allows tag names to contain literal (, ), or : characters
        if let Ok((rest, ch)) = alt((
            parse_escaped_char('('),
            parse_escaped_char(')'),
            parse_escaped_char(':'),
        ))(remaining) {
            result.push(ch);
            remaining = rest;
            continue;
        }
        
        // Check for terminators that end the tag name
        if remaining.is_empty() || 
           remaining.starts_with('(') ||   // Start of alias
           remaining.starts_with(':') ||   // End of line marker (optional)
           remaining.starts_with('\n') ||  // Newline
           remaining.starts_with('\r') {   // Carriage return
            break;
        }
        
        // Take one character and add it to the result
        if let Some(ch) = remaining.chars().next() {
            result.push(ch);
            remaining = &remaining[ch.len_utf8()..];
        } else {
            break;
        }
    }
    
    // Trim whitespace from the result
    let trimmed = result.trim();
    
    // Tag names cannot be empty
    if trimmed.is_empty() {
        return Err(nom::Err::Error(nom::error::Error::new(input, nom::error::ErrorKind::TakeWhile1)));
    }
    
    // Calculate how much input we consumed
    let consumed_len = input.len() - remaining.len();
    Ok((&input[consumed_len..], trimmed.to_string()))
}

/// Parse an alias enclosed in parentheses
/// Aliases are optional shortcuts for tag names
/// Format: (alias_name)
fn parse_alias(input: &str) -> IResult<&str, String> {
    delimited(
        char('('),
        map(is_not(")"), |s: &str| s.trim().to_string()),
        char(')')
    )(input)
}

/// Parse a complete tag line
/// Format: [indent][tag_type] [tag_name][ (alias)][ :]
/// Where:
/// - indent is 0 or more groups of 4 spaces
/// - tag_type is -, +, or *
/// - tag_name is required and can contain escaped characters
/// - alias is optional and enclosed in parentheses
/// - : is optional and marks end of line explicitly
fn parse_tag_line(input: &str) -> IResult<&str, ParsedTag> {
    let original_input = input;
    
    // Parse indentation (must be multiple of 4 spaces)
    let (input, indent) = parse_indent(input)?;
    
    // Verify indent is multiple of 4 for proper ENTS formatting
    if indent % 4 != 0 {
        println!("Invalid indent: {} spaces", indent);
        return Err(nom::Err::Error(nom::error::Error::new(
            input, 
            nom::error::ErrorKind::Verify
        )));
    }
    
    // Parse the tag type marker (-, +, or *)
    let (input, tag_type) = parse_tag_type(input)?;
    
    // Require at least one space after tag type
    let (input, spaces) = parse_spaces1(input)?;
    
    // Parse the tag name
    let (input, name) = parse_tag_name(input)?;
    
    // After the tag name, we might have:
    // 1. Nothing (end of line)
    // 2. Spaces followed by alias
    // 3. Spaces followed by colon
    // 4. Alias followed by optional colon
    
    // Parse optional spaces
    let (input, _) = parse_spaces(input)?;
    
    // Parse optional alias in parentheses
    let (input, alias) = opt(parse_alias)(input)?;
    
    // Parse optional trailing spaces
    let (input, _) = parse_spaces(input)?;
    
    // Parse optional colon (explicit line terminator)
    let (input, _) = opt(char(':'))(input)?;
    
    // Parse any final trailing spaces
    let (input, _) = parse_spaces(input)?;
    
    Ok((input, ParsedTag {
        indent: indent / 4, // Convert to indentation level (0, 1, 2, etc.)
        tag_type,
        name,
        alias,
    }))
}

/// Parse newline characters
/// Handles different newline formats: \n, \r\n, or \r
fn parse_newline(input: &str) -> IResult<&str, ()> {
    alt((
        map(tag("\r\n"), |_| ()),  // Windows style
        map(tag("\n"), |_| ()),    // Unix style
        map(tag("\r"), |_| ()),    // Old Mac style
    ))(input)
}

/// Parse a single line which can be either empty or contain a tag
/// Returns None for empty lines, Some(ParsedTag) for tag lines
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

/// Parse an entire ENTS file
/// Processes the file line by line, tracking line numbers for error reporting
/// Skips empty lines and parses tag lines
fn parse_ents_file(input: &str) -> IResult<&str, Vec<ParsedTag>> {
    let mut remaining = input;
    let mut tags = Vec::new();
    let mut line_num = 1;
    
    // Process input character by character until we've consumed everything
    while !remaining.is_empty() {
        // Skip any empty lines at the beginning
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
                // Successfully parsed a tag
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

/// Build a hierarchical tag structure from flat parsed tags
/// Creates parent-child relationships based on indentation levels
/// Also extracts aliases and creates a mapping from alias to tag name
fn build_hierarchy(parsed_tags: Vec<ParsedTag>) -> (Vec<EntsTag>, HashMap<String, String>) {
    let mut aliases = HashMap::new();
    let mut all_tags: Vec<EntsTag> = Vec::new();
    let mut tag_stack: Vec<usize> = Vec::new(); // Stack of indices into all_tags for tracking hierarchy
    
    for parsed_tag in parsed_tags {
        // Add alias to the aliases map if present
        if let Some(alias) = &parsed_tag.alias {
            aliases.insert(alias.clone(), parsed_tag.name.clone());
        }
        
        // Adjust stack to match current indent level
        // Remove tags from stack that are at the same or deeper level
        tag_stack.truncate(parsed_tag.indent);
        
        // Calculate ancestry by walking up the stack
        let mut ancestry = Vec::new();
        for &idx in &tag_stack {
            ancestry.push(all_tags[idx].name.clone());
        }
        
        // Create the new tag with the calculated ancestry
        let mut tag = EntsTag {
            name: parsed_tag.name.clone(),
            tag_type: parsed_tag.tag_type,
            children: Vec::new(),     // Will be populated as we process children
            ancestry,
            show: Some(true),         // New tags are visible by default
            files: None,              // Set to None to match expected JSON output
            child_tags: Vec::new(),   // Temporary field used during parsing
            alias: parsed_tag.alias,
        };
        
        // Add this tag to its parent's children list if there is a parent
        if let Some(&parent_idx) = tag_stack.last() {
            all_tags[parent_idx].children.push(parsed_tag.name.clone());
        }
        
        // Add tag to all_tags and remember its index for potential children
        let tag_index = all_tags.len();
        all_tags.push(tag);
        
        // Push this tag's index onto the stack for potential children
        tag_stack.push(tag_index);
    }
    
    // Return all tags and the aliases mapping
    (all_tags, aliases)
}

/// Main parse function that reads an ENTS file and returns a TagsFile structure
/// This is the primary entry point for parsing ENTS files
/// 
/// # Arguments
/// * `file_path` - Path to the ENTS file to parse
/// 
/// # Returns
/// * `Ok(TagsFile)` - Successfully parsed tag structure
/// * `Err(Box<dyn Error>)` - Parse error or file I/O error
pub fn parse_ents(file_path: &str) -> Result<TagsFile, Box<dyn Error>> {
    // Read the file contents
    let content = fs::read_to_string(file_path)?;
    
    // Normalize line endings to \n for consistent parsing
    // This handles files created on different operating systems
    let normalized_content = content.replace("\r\n", "\n").replace("\r", "\n");
    
    // Parse the normalized content
    let (remaining, parsed_tags) = parse_ents_file(&normalized_content)
        .map_err(|e| format!("Parse error: {:?}", e))?;
    
    // Check if we parsed the entire file successfully
    if !remaining.trim().is_empty() {
        return Err(format!("Failed to parse entire file. Remaining: {:?}", remaining).into());
    }
    
    println!("Parsed {} tags", parsed_tags.len());
    
    // Build the hierarchical structure and extract aliases
    let (all_tags, aliases) = build_hierarchy(parsed_tags);
    
    // Create and return the complete TagsFile structure
    Ok(TagsFile {
        files: Vec::new(), // Initialize with empty files vector
        aliases,
        tags: all_tags,
    })
}

// Unit tests to verify parser functionality
#[cfg(test)]
mod tests {
    use super::*;
    
    /// Test parsing of different tag type markers
    #[test]
    fn test_parse_tag_type() {
        assert_eq!(parse_tag_type("-").unwrap().1, TagType::Normal);
        assert_eq!(parse_tag_type("+").unwrap().1, TagType::Dud);
        assert_eq!(parse_tag_type("*").unwrap().1, TagType::Exclusive); // Updated test
    }
    
    /// Test parsing of tag names with various terminators
    #[test]
    fn test_parse_tag_name() {
        assert_eq!(parse_tag_name("hello world").unwrap().1, "hello world");
        assert_eq!(parse_tag_name("hello (alias)").unwrap().1, "hello");
        assert_eq!(parse_tag_name("hello:").unwrap().1, "hello");
        assert_eq!(parse_tag_name("hello\\(world\\)").unwrap().1, "hello(world)");
    }
    
    /// Test parsing of aliases in parentheses
    #[test]
    fn test_parse_alias() {
        assert_eq!(parse_alias("(test)").unwrap().1, "test");
        assert_eq!(parse_alias("(ny)").unwrap().1, "ny");
    }
    
    /// Test parsing a simple tag line
    #[test]
    fn test_parse_simple_tag() {
        let input = "- jade\n";
        let (_, tag) = parse_tag_line(input).unwrap();
        assert_eq!(tag.indent, 0);
        assert_eq!(tag.tag_type, TagType::Normal);
        assert_eq!(tag.name, "jade");
        assert_eq!(tag.alias, None);
    }
    
    /// Test parsing a tag with alias and new exclusive syntax
    #[test]
    fn test_parse_tag_with_alias() {
        let input = "    * new york (ny)\n"; // Updated to use * instead of +-
        let (_, tag) = parse_tag_line(input).unwrap();
        assert_eq!(tag.indent, 1);
        assert_eq!(tag.tag_type, TagType::Exclusive);
        assert_eq!(tag.name, "new york");
        assert_eq!(tag.alias, Some("ny".to_string()));
    }
}
