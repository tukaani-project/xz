// Équivalent de tuklib_exit.c/h

use std::process;
use std::io::{self, Write};

pub fn tuklib_exit(status: i32, err_status: i32, show_error: bool) -> ! {
    // Fermer stdout et stderr proprement
    let _ = io::stdout().flush();
    let _ = io::stderr().flush();
    
    if show_error && status != 0 {
        // Afficher un message d'erreur si nécessaire
        eprintln!("Error occurred during execution");
    }
    
    // Utiliser err_status en cas d'erreur, sinon status
    let exit_code = if status != 0 { err_status } else { status };
    process::exit(exit_code);
} 