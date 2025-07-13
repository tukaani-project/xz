// Équivalent de util.c/h - fonctions utilitaires pour xz

use crate::util::{UtilError, str_to_u64};
use std::io::{self, IsTerminal};

// Buffers pour les conversions de nombres (équivalent des bufs[4][128] en C)
static mut BUFS: [[u8; 128]; 4] = [[0; 128]; 4];
static mut BUF_INDEX: usize = 0;

// Support des séparateurs de milliers (simplifié)
#[derive(Debug, Clone, Copy)]
enum ThousandSep {
    Unknown,
    Works,
    Broken,
}

static mut THOUSAND: ThousandSep = ThousandSep::Unknown;

// Version sûre de malloc qui ne retourne jamais null
pub fn xmalloc(size: usize) -> Vec<u8> {
    vec![0; size]
}

// Version sûre de realloc
pub fn xrealloc(mut vec: Vec<u8>, new_size: usize) -> Vec<u8> {
    vec.resize(new_size, 0);
    vec
}

// Version sûre de strdup
pub fn xstrdup(src: &str) -> String {
    src.to_string()
}

// Conversion de chaîne vers u64 avec suffixes (équivalent de str_to_uint64)
pub fn str_to_uint64(name: &str, value: &str, min: u64, max: u64) -> Result<u64, UtilError> {
    str_to_u64(value, min, max)
}

// Arrondir à la MiB supérieure
pub fn round_up_to_mib(n: u64) -> u64 {
    (n >> 20) + if n & ((1 << 20) - 1) != 0 { 1 } else { 0 }
}

// Conversion d'un u64 en chaîne avec séparateurs de milliers
pub fn uint64_to_str(value: u64, slot: u32) -> String {
    // Version simplifiée sans séparateurs de milliers pour l'instant
    format!("{}", value)
}

// Unités pour l'affichage convivial
#[derive(Debug, Clone, Copy)]
pub enum NicestrUnit {
    B,
    KiB,
    MiB,
    GiB,
    TiB,
}

// Conversion en chaîne conviviale (équivalent de uint64_to_nicestr)
pub fn uint64_to_nicestr(
    value: u64,
    unit_min: NicestrUnit,
    unit_max: NicestrUnit,
    always_also_bytes: bool,
    _slot: u32,
) -> String {
    let units = [
        (NicestrUnit::TiB, 1u64 << 40, "TiB"),
        (NicestrUnit::GiB, 1u64 << 30, "GiB"),
        (NicestrUnit::MiB, 1u64 << 20, "MiB"),
        (NicestrUnit::KiB, 1u64 << 10, "KiB"),
        (NicestrUnit::B, 1, "B"),
    ];

    for (unit, divisor, suffix) in &units {
        if value >= *divisor && (*unit as u8) >= (unit_min as u8) && (*unit as u8) <= (unit_max as u8) {
            let result = value / divisor;
            if always_also_bytes && *divisor > 1 {
                return format!("{} {} ({} B)", result, suffix, value);
            } else {
                return format!("{} {}", result, suffix);
            }
        }
    }

    format!("{} B", value)
}

// snprintf avec gestion de position et taille restante
pub fn my_snprintf(pos: &mut String, fmt: &str, args: std::fmt::Arguments) {
    use std::fmt::Write;
    let _ = write!(pos, "{}", args);
}

// Test si un descripteur de fichier est un terminal
pub fn is_tty(fd: i32) -> bool {
    match fd {
        0 => io::stdin().is_terminal(),
        1 => io::stdout().is_terminal(),
        2 => io::stderr().is_terminal(),
        _ => false,
    }
}

// Test si stdin est un terminal
pub fn is_tty_stdin() -> bool {
    is_tty(0)
}

// Test si stdout est un terminal
pub fn is_tty_stdout() -> bool {
    is_tty(1)
} 