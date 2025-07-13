// Équivalent de tuklib_mbstr.h - gestion des chaînes multibytes

// Version simplifiée qui assume UTF-8

pub fn tuklib_mbstr_width(s: &str) -> (usize, usize) {
    let bytes = s.len();
    let width = s.chars().count(); // Approximation simple
    (width, bytes)
}

pub fn tuklib_mbstr_width_mem(s: &str, len: usize) -> usize {
    let truncated = if s.len() > len {
        &s[..len]
    } else {
        s
    };
    truncated.chars().count()
}

pub fn tuklib_mbstr_fw(s: &str, columns_min: i32) -> i32 {
    let (width, _) = tuklib_mbstr_width(s);
    if width as i32 > columns_min {
        0
    } else {
        columns_min - width as i32
    }
}

// Fonction pour masquer les caractères non imprimables
pub fn tuklib_mask_nonprint(s: &str) -> String {
    s.chars()
        .map(|c| if c.is_control() { '?' } else { c })
        .collect()
}

pub fn tuklib_has_nonprint(s: &str) -> bool {
    s.chars().any(|c| c.is_control())
} 