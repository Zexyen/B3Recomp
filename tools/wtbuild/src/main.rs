mod lifter;
mod nso;
mod util;

use std::io::{BufRead, Write};
use std::path::Path;

fn die(msg: impl std::fmt::Display) -> ! {
    eprintln!("wtbuild: {}", msg);
    std::process::exit(1)
}

fn main() {
    let args: Vec<String> = std::env::args().skip(1).collect();
    match args.first().map(String::as_str) {
        Some("lift-words") => {
            // lines "<word hex> <pc hex> <entry hex> <end hex>" on stdin -> one statement per line
            let stdin = std::io::stdin();
            let mut out = std::io::BufWriter::new(std::io::stdout());
            for line in stdin.lock().lines() {
                let line = line.unwrap();
                let f: Vec<i64> = line.split_whitespace().map(|s| i64::from_str_radix(s, 16).unwrap()).collect();
                let (s, br) = lifter::lift_word(f[0] as u32, f[1], f[2], f[3]);
                writeln!(out, "{}{}", if br { "BR " } else { "" }, s).unwrap();
            }
        }
        Some("nso2elf") if args.len() == 3 => {
            let d = std::fs::read(&args[1]).unwrap_or_else(|e| die(format!("{}: {}", args[1], e)));
            let c = nso::nso2elf(&d).unwrap_or_else(|e| die(e));
            for l in &c.report {
                println!("{}", l);
            }
            std::fs::write(&args[2], &c.elf).unwrap_or_else(|e| die(e));
            println!("wrote {} {}", args[2], c.elf.len());
        }
        Some("lift") if args.len() >= 3 => {
            let img = std::fs::read(&args[1]).unwrap_or_else(|e| die(format!("{}: {}", args[1], e)));
            let mut im = lifter::Image::main(img);
            if !args.iter().any(|a| a == "--no-patches") {
                lifter::eh::options_row_patches(&mut im).unwrap_or_else(|e| die(e));
            }
            lifter::eh::lift_main(&im, Path::new(&args[2]), &mut |l| println!("{}", l)).unwrap_or_else(|e| die(e));
        }
        _ => die("usage: see tools/wtbuild/README.md"),
    }
}
