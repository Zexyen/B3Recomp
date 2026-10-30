//! What the build makes differs between Linux and Windows: the file names, the compiler's target and the runtime's
//! libraries. The player's build is always for the system it runs on; `wtbuild compile / link / runtime --target windows`
//! makes the Windows files elsewhere too (CI builds the Windows runtime on Linux with llvm-mingw). Android (AArch64,
//! API 29 = Android 10) is never a host: the player's PC compiles and links game.so for it (`--target android`, no NDK
//! needed: the lifted code uses runtime/freestanding), and CI builds the runtime with the NDK's clang (LLVM_ROOT).

use std::fs;
use std::path::Path;

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Target {
    Linux,
    Windows,
    Android,
}

impl Target {
    pub fn host() -> Target {
        if cfg!(windows) {
            Target::Windows
        } else {
            Target::Linux
        }
    }

    /// `--target linux|windows` among the arguments, else the host
    pub fn from_args(args: &[String]) -> Result<Target, String> {
        match args.iter().position(|a| a == "--target").map(|i| args.get(i + 1).map(String::as_str)) {
            None => Ok(Target::host()),
            Some(Some("linux")) => Ok(Target::Linux),
            Some(Some("windows")) => Ok(Target::Windows),
            Some(Some("android")) => Ok(Target::Android),
            Some(v) => Err(format!("--target {}: linux, windows or android", v.unwrap_or(""))),
        }
    }

    pub fn windows(self) -> bool {
        self == Target::Windows
    }

    pub fn android(self) -> bool {
        self == Target::Android
    }

    /// TOOLCHAIN in build/setup_state.json, the log lines and the compile step's configuration hash
    pub fn name(self) -> &'static str {
        match self {
            Target::Linux => "linux",
            Target::Windows => "windows",
            Target::Android => "android",
        }
    }

    /// the game's own library, linked on the player's PC
    pub fn game_lib(self) -> &'static str {
        match self {
            Target::Linux | Target::Android => "game.so",
            Target::Windows => "game.dll",
        }
    }

    /// the program that runs the game (Android: the runtime library itself, loaded by the app's SDLActivity)
    pub fn game_exe(self) -> &'static str {
        match self {
            Target::Linux => "witch-time-game",
            Target::Windows => "witch-time-game.exe",
            Target::Android => "libwitchtime.so",
        }
    }

    /// clang flags that pick the target, and the instruction set the runtime and game.so are built for. x86-64: cmpxchg16b
    /// (-mcx16) for the 16-byte atomics of the lifted code. Windows: the MinGW ABI the runtime is built with, for any clang
    /// (the bundled one, LLVM's own release, would make MSVC objects; llvm-mingw's makes these anyway). Android: AArch64,
    /// the 16-byte atomics inline (ldxp / stxp) instead of calls into compiler-rt, which the bundled clang has not got
    pub fn clang_flags(self) -> Vec<String> {
        match self {
            Target::Linux => vec!["-mcx16".into()],
            Target::Windows => vec!["--target=x86_64-w64-mingw32".into(), "-mcx16".into()],
            Target::Android => vec!["--target=aarch64-linux-android29".into(), "-mno-outline-atomics".into()],
        }
    }

    /// the lifted objects: build/obj for this system's game library, build/obj_<target> for another's, so that both can be
    /// built in one folder
    pub fn obj_dir(self, repo: &Path) -> std::path::PathBuf {
        repo.join("build").join(if self == Target::host() { "obj".to_string() } else { format!("obj_{}", self.name()) })
    }

    /// where the game library and the runtime go: build/ for this system, build/<target> for another's
    pub fn out_dir(self, repo: &Path) -> std::path::PathBuf {
        let build = repo.join("build");
        if self == Target::host() { build } else { build.join(self.name()) }
    }

    /// code that goes into a shared library: position-independent on ELF; PE has no such flag (clang refuses it)
    pub fn pic(self) -> Vec<String> {
        match self {
            Target::Linux | Target::Android => vec!["-fPIC".into()],
            Target::Windows => vec![],
        }
    }
}

/// the compiler's name: clang, clang.exe
pub fn exe(name: &str) -> String {
    format!("{}{}", name, std::env::consts::EXE_SUFFIX)
}

/// a file's modification time in nanoseconds since 1970, as Python's st_mtime_ns
pub fn mtime_ns(m: &fs::Metadata) -> i128 {
    match m.modified().map(|t| t.duration_since(std::time::UNIX_EPOCH)) {
        Ok(Ok(d)) => d.as_nanos() as i128,
        Ok(Err(e)) => -(e.duration().as_nanos() as i128),
        Err(_) => 0,
    }
}

/// a program file this system can run (shutil.which's test)
pub fn is_program(p: &Path) -> bool {
    let Ok(m) = fs::metadata(p) else { return false };
    #[cfg(unix)]
    {
        use std::os::unix::fs::PermissionsExt;
        m.is_file() && m.permissions().mode() & 0o111 != 0
    }
    #[cfg(not(unix))]
    {
        m.is_file()
    }
}

/// removes a symbolic link or junction to a folder (older setups linked targets/romfs), never what it points to
pub fn remove_dir_link(link: &Path) -> std::io::Result<()> {
    if cfg!(windows) {
        fs::remove_dir(link) // a junction (or a directory symbolic link) is removed as an empty folder
    } else {
        fs::remove_file(link)
    }
}
