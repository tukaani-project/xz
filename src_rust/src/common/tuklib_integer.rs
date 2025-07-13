// Équivalent de tuklib_integer.h - opérations sur les entiers

// Fonctions de swap d'octets
pub fn byteswap16(n: u16) -> u16 {
    n.swap_bytes()
}

pub fn byteswap32(n: u32) -> u32 {
    n.swap_bytes()
}

pub fn byteswap64(n: u64) -> u64 {
    n.swap_bytes()
}

// Conversions endianness
#[cfg(target_endian = "big")]
pub fn conv16be(n: u16) -> u16 { n }
#[cfg(target_endian = "big")]
pub fn conv16le(n: u16) -> u16 { byteswap16(n) }
#[cfg(target_endian = "big")]
pub fn conv32be(n: u32) -> u32 { n }
#[cfg(target_endian = "big")]
pub fn conv32le(n: u32) -> u32 { byteswap32(n) }
#[cfg(target_endian = "big")]
pub fn conv64be(n: u64) -> u64 { n }
#[cfg(target_endian = "big")]
pub fn conv64le(n: u64) -> u64 { byteswap64(n) }

#[cfg(target_endian = "little")]
pub fn conv16be(n: u16) -> u16 { byteswap16(n) }
#[cfg(target_endian = "little")]
pub fn conv16le(n: u16) -> u16 { n }
#[cfg(target_endian = "little")]
pub fn conv32be(n: u32) -> u32 { byteswap32(n) }
#[cfg(target_endian = "little")]
pub fn conv32le(n: u32) -> u32 { n }
#[cfg(target_endian = "little")]
pub fn conv64be(n: u64) -> u64 { byteswap64(n) }
#[cfg(target_endian = "little")]
pub fn conv64le(n: u64) -> u64 { n }

// Lecture/écriture non alignée
pub fn read16ne(buf: &[u8]) -> u16 {
    u16::from_ne_bytes([buf[0], buf[1]])
}

pub fn read32ne(buf: &[u8]) -> u32 {
    u32::from_ne_bytes([buf[0], buf[1], buf[2], buf[3]])
}

pub fn read64ne(buf: &[u8]) -> u64 {
    u64::from_ne_bytes([
        buf[0], buf[1], buf[2], buf[3],
        buf[4], buf[5], buf[6], buf[7]
    ])
}

pub fn write16ne(buf: &mut [u8], num: u16) {
    let bytes = num.to_ne_bytes();
    buf[0] = bytes[0];
    buf[1] = bytes[1];
}

pub fn write32ne(buf: &mut [u8], num: u32) {
    let bytes = num.to_ne_bytes();
    buf[0..4].copy_from_slice(&bytes);
}

pub fn write64ne(buf: &mut [u8], num: u64) {
    let bytes = num.to_ne_bytes();
    buf[0..8].copy_from_slice(&bytes);
}

// Lecture/écriture big endian
pub fn read16be(buf: &[u8]) -> u16 {
    u16::from_be_bytes([buf[0], buf[1]])
}

pub fn read32be(buf: &[u8]) -> u32 {
    u32::from_be_bytes([buf[0], buf[1], buf[2], buf[3]])
}

pub fn read64be(buf: &[u8]) -> u64 {
    u64::from_be_bytes([
        buf[0], buf[1], buf[2], buf[3],
        buf[4], buf[5], buf[6], buf[7]
    ])
}

// Lecture/écriture little endian
pub fn read16le(buf: &[u8]) -> u16 {
    u16::from_le_bytes([buf[0], buf[1]])
}

pub fn read32le(buf: &[u8]) -> u32 {
    u32::from_le_bytes([buf[0], buf[1], buf[2], buf[3]])
}

pub fn read64le(buf: &[u8]) -> u64 {
    u64::from_le_bytes([
        buf[0], buf[1], buf[2], buf[3],
        buf[4], buf[5], buf[6], buf[7]
    ])
}

// Opérations de bit scan
pub fn bsr32(n: u32) -> u32 {
    31 - n.leading_zeros()
}

pub fn clz32(n: u32) -> u32 {
    n.leading_zeros()
}

pub fn ctz32(n: u32) -> u32 {
    n.trailing_zeros()
}

pub fn bsf32(n: u32) -> u32 {
    ctz32(n)
} 