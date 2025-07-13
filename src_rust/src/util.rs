// Module utilitaire Rust inspiré de util.c/h

use std::fmt;
use std::io;

#[derive(Debug)]
pub enum UtilError {
    Parse(String),
    Io(io::Error),
}

impl fmt::Display for UtilError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            UtilError::Parse(s) => write!(f, "Parse error: {}", s),
            UtilError::Io(e) => write!(f, "IO error: {}", e),
        }
    }
}

impl std::error::Error for UtilError {}

impl From<io::Error> for UtilError {
    fn from(e: io::Error) -> Self {
        UtilError::Io(e)
    }
}

pub fn str_to_u64(value: &str, min: u64, max: u64) -> Result<u64, UtilError> {
    let value = value.trim();
    if value == "max" {
        return Ok(max);
    }
    let mut result: u64 = 0;
    let mut chars = value.chars().peekable();
    while let Some(&c) = chars.peek() {
        if c.is_ascii_digit() {
            result = result.checked_mul(10).ok_or_else(|| UtilError::Parse(value.to_string()))?;
            result = result.checked_add(c.to_digit(10).unwrap() as u64).ok_or_else(|| UtilError::Parse(value.to_string()))?;
            chars.next();
        } else {
            break;
        }
    }
    let suffix: String = chars.collect();
    let multiplier = match suffix.to_ascii_lowercase().as_str() {
        "k" | "kb" | "kib" => 1 << 10,
        "m" | "mb" | "mib" => 1 << 20,
        "g" | "gb" | "gib" => 1 << 30,
        "" => 1,
        _ => return Err(UtilError::Parse(format!("Invalid suffix: {suffix}"))),
    };
    result = result.checked_mul(multiplier).ok_or_else(|| UtilError::Parse(value.to_string()))?;
    if result < min || result > max {
        return Err(UtilError::Parse(format!("Value {result} not in range [{min}, {max}]")));
    }
    Ok(result)
}

pub fn round_up_to_mib(n: u64) -> u64 {
    (n >> 20) + if n & ((1 << 20) - 1) != 0 { 1 } else { 0 }
} 