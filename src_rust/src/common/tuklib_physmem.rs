// Équivalent de tuklib_physmem.c/h

#[cfg(target_os = "linux")]
use std::fs;

#[cfg(target_os = "macos")]
use std::process::Command;

#[cfg(target_os = "windows")]
use std::mem;

pub fn tuklib_physmem() -> u64 {
    #[cfg(target_os = "linux")]
    {
        // Lire /proc/meminfo sur Linux
        if let Ok(content) = fs::read_to_string("/proc/meminfo") {
            for line in content.lines() {
                if line.starts_with("MemTotal:") {
                    if let Some(kb_str) = line.split_whitespace().nth(1) {
                        if let Ok(kb) = kb_str.parse::<u64>() {
                            return kb * 1024; // Convertir de KB en bytes
                        }
                    }
                }
            }
        }
        0
    }
    
    #[cfg(target_os = "macos")]
    {
        // Utiliser sysctl sur macOS
        if let Ok(output) = Command::new("sysctl")
            .args(&["-n", "hw.memsize"])
            .output()
        {
            if let Ok(size_str) = String::from_utf8(output.stdout) {
                if let Ok(size) = size_str.trim().parse::<u64>() {
                    return size;
                }
            }
        }
        0
    }
    
    #[cfg(target_os = "windows")]
    {
        // Utiliser GetPhysicallyInstalledSystemMemory sur Windows
        // Pour l'instant, retourner une valeur par défaut
        // TODO: Implémenter avec winapi si nécessaire
        8_589_934_592 // 8 GB par défaut
    }
    
    #[cfg(not(any(target_os = "linux", target_os = "macos", target_os = "windows")))]
    {
        // Plateforme non supportée
        0
    }
} 