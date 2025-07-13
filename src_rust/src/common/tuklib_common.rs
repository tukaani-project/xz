// Équivalent de tuklib_common.h

// Attributs pour les fonctions (équivalents des attributs GCC/Clang)
macro_rules! tuklib_attr_noreturn {
    () => {};
}

macro_rules! tuklib_attr_format_printf {
    ($fmt_index:expr, $args_index:expr) => {};
}

pub(crate) use tuklib_attr_noreturn;
pub(crate) use tuklib_attr_format_printf;

// Concaténation de symboles
macro_rules! tuklib_cat {
    ($a:ident, $b:ident) => {
        paste::paste! { [<$a $b>] }
    };
}

// Symboles avec préfixe (pour éviter les conflits)
pub const TUKLIB_SYMBOL_PREFIX: &str = "";

// Détection de compilateur
#[cfg(any(target_env = "gnu", target_env = "musl"))]
pub const TUKLIB_GNUC: bool = true;
#[cfg(not(any(target_env = "gnu", target_env = "musl")))]
pub const TUKLIB_GNUC: bool = false; 