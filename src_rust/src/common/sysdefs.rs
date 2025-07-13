// Équivalent de sysdefs.h - définitions système communes

#[cfg(target_os = "windows")]
pub const TUKLIB_DOSLIKE: bool = true;
#[cfg(not(target_os = "windows"))]
pub const TUKLIB_DOSLIKE: bool = false;

// Types de base
pub type Uint32 = u32;
pub type Uint64 = u64;
pub type SizeT = usize;

// Constantes
pub const UINT32_MAX: u32 = u32::MAX;
pub const UINT64_MAX: u64 = u64::MAX;
pub const SIZE_MAX: usize = usize::MAX;

// Macros utilitaires
macro_rules! array_size {
    ($arr:expr) => {
        std::mem::size_of_val(&$arr) / std::mem::size_of_val(&$arr[0])
    };
}

pub(crate) use array_size;

// Fonction memzero équivalente
pub fn memzero(ptr: &mut [u8]) {
    ptr.fill(0);
}

// Fallthrough pour match (équivalent de FALLTHROUGH en C)
macro_rules! fallthrough {
    () => {};
}

pub(crate) use fallthrough; 