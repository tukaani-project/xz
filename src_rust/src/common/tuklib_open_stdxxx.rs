// Équivalent de tuklib_open_stdxxx.c/h

use std::process;

pub fn tuklib_open_stdxxx(err_status: i32) {
    // En Rust, stdin/stdout/stderr sont toujours disponibles
    // Cette fonction est principalement utile en C pour s'assurer
    // que les descripteurs de fichiers 0, 1, 2 sont ouverts
    
    // Pour l'instant, on ne fait rien car Rust garantit que
    // std::io::stdin(), stdout(), stderr() sont toujours utilisables
    
    // Si on voulait vraiment vérifier, on pourrait tester :
    use std::io::{self, Write};
    
    // Test simple pour voir si stderr est utilisable
    if let Err(_) = writeln!(io::stderr(), "") {
        process::exit(err_status);
    }
} 