// Équivalent de tuklib_progname.c/h

use std::env;
use std::path::Path;
use std::sync::OnceLock;

static PROGNAME: OnceLock<String> = OnceLock::new();

pub fn tuklib_progname_init(argv: &[String]) {
    let name = if let Some(arg0) = argv.first() {
        Path::new(arg0)
            .file_name()
            .and_then(|n| n.to_str())
            .unwrap_or("unknown")
            .to_string()
    } else {
        env::current_exe()
            .ok()
            .and_then(|p| p.file_name().map(|n| n.to_string_lossy().into_owned()))
            .unwrap_or_else(|| "unknown".to_string())
    };
    
    PROGNAME.set(name).unwrap_or(());
}

pub fn progname() -> &'static str {
    PROGNAME.get().map(|s| s.as_str()).unwrap_or("unknown")
} 