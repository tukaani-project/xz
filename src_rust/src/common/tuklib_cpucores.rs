// Équivalent de tuklib_cpucores.c/h

use std::thread;

pub fn tuklib_cpucores() -> u32 {
    // Utiliser std::thread::available_parallelism() disponible depuis Rust 1.59
    match thread::available_parallelism() {
        Ok(count) => count.get() as u32,
        Err(_) => {
            // Fallback: essayer de détecter manuellement
            #[cfg(target_os = "linux")]
            {
                if let Ok(content) = std::fs::read_to_string("/proc/cpuinfo") {
                    let count = content.lines()
                        .filter(|line| line.starts_with("processor"))
                        .count();
                    if count > 0 {
                        return count as u32;
                    }
                }
            }
            
            #[cfg(target_os = "macos")]
            {
                if let Ok(output) = std::process::Command::new("sysctl")
                    .args(&["-n", "hw.ncpu"])
                    .output()
                {
                    if let Ok(count_str) = String::from_utf8(output.stdout) {
                        if let Ok(count) = count_str.trim().parse::<u32>() {
                            return count;
                        }
                    }
                }
            }
            
            // Par défaut, retourner 1
            1
        }
    }
} 