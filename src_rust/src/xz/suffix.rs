// Équivalent de suffix.c/h - gestion des suffixes de fichiers

use std::path::Path;

#[derive(Debug, Clone, Copy)]
pub enum FormatType {
    Auto,
    Xz,
    Lzma,
    Lzip,
    Raw,
}

// Suffixes supportés
const SUFFIXES: &[(&str, &str)] = &[
    (".xz", ""),
    (".txz", ".tar"),
    (".lzma", ""),
    (".tlz", ".tar"),
    (".lz", ""),
];

pub fn suffix_get_dest_name(src_name: &str) -> Option<String> {
    let path = Path::new(src_name);
    let filename = path.file_name()?.to_str()?;
    
    // Pour la compression, ajouter .xz
    if !has_compressed_suffix(filename) {
        return Some(format!("{}.xz", src_name));
    }
    
    // Pour la décompression, enlever le suffixe
    for (compressed, uncompressed) in SUFFIXES {
        if filename.ends_with(compressed) {
            let base = &filename[..filename.len() - compressed.len()];
            let result = if uncompressed.is_empty() {
                base.to_string()
            } else {
                format!("{}{}", base, uncompressed)
            };
            
            if let Some(parent) = path.parent() {
                return Some(parent.join(result).to_string_lossy().to_string());
            } else {
                return Some(result);
            }
        }
    }
    
    None
}

pub fn has_compressed_suffix(filename: &str) -> bool {
    SUFFIXES.iter().any(|(suffix, _)| filename.ends_with(suffix))
}

pub fn suffix_set(_custom_suffix: &str) {
    // TODO: Implémenter le suffixe personnalisé
}

pub fn suffix_is_set() -> bool {
    false // TODO: Implémenter
} 