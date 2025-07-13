mod util;
mod common;
mod xz;
mod liblzma;
mod xzdec;
mod lzmainfo;

use std::env;

#[derive(Debug, Default)]
struct Cli {
    input: Option<String>,
    output: Option<String>,
    decompress: bool,
    list: bool,
    test: bool,
    threads: Option<usize>,
    block_size: Option<usize>,
    keep: bool,
    force: bool,
    no_warn: bool,
}

fn parse_args() -> Cli {
    let mut cli = Cli::default();
    let mut args = env::args().skip(1);
    while let Some(arg) = args.next() {
        match arg.as_str() {
            "-i" | "--input" => cli.input = args.next(),
            "-o" | "--output" => cli.output = args.next(),
            "--decompress" => cli.decompress = true,
            "--list" => cli.list = true,
            "--test" => cli.test = true,
            "--threads" => cli.threads = args.next().and_then(|v| v.parse().ok()),
            "--block-size" => cli.block_size = args.next().and_then(|v| v.parse().ok()),
            "--keep" => cli.keep = true,
            "--force" => cli.force = true,
            "--no-warn" => cli.no_warn = true,
            _ => {},
        }
    }
    cli
}

fn main() {
    let cli = parse_args();
    println!("Arguments: {:?}", cli);
    // TODO: Appeler les modules migrés ici
}
