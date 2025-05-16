use serde::{Serialize, Deserialize};
use std::collections::HashMap;

#[derive(Serialize, Deserialize, Debug, PartialEq)]
pub enum TagType {
    #[serde(rename = "normal")]
    Normal,
    #[serde(rename = "dud")]
    Dud,
    #[serde(rename = "exclusive")]
    Exclusive,
}

#[derive(Serialize, Deserialize, Debug)]
pub struct EntsTag {
    pub name: String,
    #[serde(rename = "type")]
    pub tag_type: TagType,
    pub children: Vec<String>,
    pub ancestry: Vec<String>,
    pub show: Option<bool>,
    pub files: Option<Vec<String>>,
}

#[derive(Serialize, Deserialize, Debug)]
pub struct TagsFile {
    pub aliases: HashMap<String, String>,
    pub tags: Vec<EntsTag>,
}
