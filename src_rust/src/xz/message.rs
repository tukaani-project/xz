// Équivalent de message.c/h - système de messages

use std::io::{self, Write};

#[derive(Debug, Clone, Copy, PartialEq, PartialOrd)]
pub enum MessageVerbosity {
    Silent,
    Error,
    Warning,
    Verbose,
    Debug,
}

static mut VERBOSITY: MessageVerbosity = MessageVerbosity::Warning;
static mut FILENAME: Option<String> = None;

pub fn message_init() {
    // Initialisation du système de messages
}

pub fn message_verbosity_increase() {
    unsafe {
        VERBOSITY = match VERBOSITY {
            MessageVerbosity::Silent => MessageVerbosity::Error,
            MessageVerbosity::Error => MessageVerbosity::Warning,
            MessageVerbosity::Warning => MessageVerbosity::Verbose,
            MessageVerbosity::Verbose => MessageVerbosity::Debug,
            MessageVerbosity::Debug => MessageVerbosity::Debug,
        };
    }
}

pub fn message_verbosity_decrease() {
    unsafe {
        VERBOSITY = match VERBOSITY {
            MessageVerbosity::Silent => MessageVerbosity::Silent,
            MessageVerbosity::Error => MessageVerbosity::Silent,
            MessageVerbosity::Warning => MessageVerbosity::Error,
            MessageVerbosity::Verbose => MessageVerbosity::Warning,
            MessageVerbosity::Debug => MessageVerbosity::Verbose,
        };
    }
}

pub fn message_verbosity_get() -> MessageVerbosity {
    unsafe { VERBOSITY }
}

pub fn message(verbosity: MessageVerbosity, msg: &str) {
    if verbosity <= message_verbosity_get() {
        eprintln!("{}", msg);
    }
}

pub fn message_warning(msg: &str) {
    message(MessageVerbosity::Warning, &format!("Warning: {}", msg));
}

pub fn message_error(msg: &str) {
    message(MessageVerbosity::Error, &format!("Error: {}", msg));
}

pub fn message_fatal(msg: &str) -> ! {
    message(MessageVerbosity::Error, &format!("Fatal: {}", msg));
    std::process::exit(1);
}

pub fn message_filename(src_name: &str) {
    unsafe {
        FILENAME = Some(src_name.to_string());
    }
}

pub fn message_set_files(_files: u32) {
    // Stub
}

pub fn message_try_help() {
    eprintln!("Try --help for more information.");
}

pub fn message_version() -> ! {
    println!("xz (Rust version) 1.0.0");
    std::process::exit(0);
}

pub fn message_help(_long_help: bool) -> ! {
    println!("Usage: xz [OPTIONS] [FILES]");
    println!("Compress or decompress .xz files");
    std::process::exit(0);
} 