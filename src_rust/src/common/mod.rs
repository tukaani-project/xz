// Module common - équivalent de src/common/

pub mod sysdefs;
pub mod tuklib_common;
pub mod tuklib_exit;
pub mod tuklib_gettext;
pub mod tuklib_progname;
pub mod tuklib_integer;
pub mod tuklib_physmem;
pub mod tuklib_cpucores;
pub mod tuklib_mbstr;
pub mod tuklib_open_stdxxx;
pub mod mythread;

pub use sysdefs::*;
pub use tuklib_common::*; 