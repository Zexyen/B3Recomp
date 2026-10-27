//! What the build makes differs between Linux and Windows: the file names, the compiler's target and the runtime's
//! libraries. The player's build is always for the system it runs on; `wtbuild compile / link / runtime --target windows`
//! makes the Windows files elsewhere too (CI builds the Windows runtime on Linux with llvm-mingw).

use std::fs;
use std::path::Path;

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Target {
    Linux,
    Windows,
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
            Some(v) => Err(format!("--target {}: linux or windows", v.unwrap_or(""))),
        }
    }

    pub fn windows(self) -> bool {
        self == Target::Windows
    }

    /// TOOLCHAIN in build/setup_state.json, the log lines and the compile step's configuration hash
    pub fn name(self) -> &'static str {
        match self {
            Target::Linux => "linux",
            Target::Windows => "windows",
        }
    }

    /// the game's own library, linked on the player's PC
    pub fn game_lib(self) -> &'static str {
        match self {
            Target::Linux => "game.so",
            Target::Windows => "game.dll",
        }
    }

    /// the program that runs the game
    pub fn game_exe(self) -> &'static str {
        match self {
            Target::Linux => "witch-time-game",
            Target::Windows => "witch-time-game.exe",
        }
    }

    /// clang flags that pick the target. Windows: the MinGW ABI the runtime is built with, for any clang (the bundled one,
    /// LLVM's own release, would make MSVC objects; llvm-mingw's makes these anyway)
    pub fn clang_flags(self) -> Vec<String> {
        match self {
            Target::Linux => vec![],
            Target::Windows => vec!["--target=x86_64-w64-mingw32".into()],
        }
    }

    /// code that goes into a shared library: position-independent on ELF; PE has no such flag (clang refuses it)
    pub fn pic(self) -> Vec<String> {
        match self {
            Target::Linux => vec!["-fPIC".into()],
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

/// link = a link to the folder target: a symbolic link, on Windows a directory junction (which needs no rights a player
/// may lack, unlike a symbolic link)
pub fn link_dir(target: &Path, link: &Path) -> std::io::Result<()> {
    #[cfg(unix)]
    {
        std::os::unix::fs::symlink(target, link)
    }
    #[cfg(windows)]
    {
        junction::make(target, link).or_else(|e| std::os::windows::fs::symlink_dir(target, link).map_err(|_| e))
    }
}

/// a directory junction (a mount point reparse point), as `mklink /J` makes it
#[cfg(windows)]
mod junction {
    use std::os::windows::ffi::OsStrExt;
    use std::path::Path;

    type Handle = isize;
    const GENERIC_WRITE: u32 = 0x4000_0000;
    const OPEN_EXISTING: u32 = 3;
    const FILE_FLAG_BACKUP_SEMANTICS: u32 = 0x0200_0000;
    const FILE_FLAG_OPEN_REPARSE_POINT: u32 = 0x0020_0000;
    const FSCTL_SET_REPARSE_POINT: u32 = 0x0009_00a4;
    const IO_REPARSE_TAG_MOUNT_POINT: u32 = 0xa000_0003;
    extern "system" {
        fn CreateFileW(name: *const u16, access: u32, share: u32, sa: *mut u8, disp: u32, flags: u32, tmpl: Handle) -> Handle;
        fn DeviceIoControl(h: Handle, code: u32, inb: *const u8, ins: u32, outb: *mut u8, outs: u32, ret: *mut u32, ov: *mut u8) -> i32;
        fn CloseHandle(h: Handle) -> i32;
    }

    fn wide(s: &std::ffi::OsStr) -> Vec<u16> {
        s.encode_wide().collect()
    }

    pub fn make(target: &Path, link: &Path) -> std::io::Result<()> {
        let target = std::path::absolute(target)?;
        // the target as the system names it (\??\C:\...), and as it is shown
        let print = wide(target.as_os_str());
        let mut subst = wide(std::ffi::OsStr::new("\\??\\"));
        subst.extend(&print);
        let (sl, pl) = (subst.len() * 2, print.len() * 2);
        let path_len = sl + 2 + pl + 2;
        let mut buf: Vec<u8> = Vec::with_capacity(16 + path_len);
        buf.extend(IO_REPARSE_TAG_MOUNT_POINT.to_le_bytes());
        buf.extend(((8 + path_len) as u16).to_le_bytes()); // ReparseDataLength
        buf.extend(0u16.to_le_bytes());
        buf.extend(0u16.to_le_bytes()); // SubstituteNameOffset
        buf.extend((sl as u16).to_le_bytes());
        buf.extend(((sl + 2) as u16).to_le_bytes()); // PrintNameOffset
        buf.extend((pl as u16).to_le_bytes());
        for c in subst.iter().chain(&[0]).chain(&print).chain(&[0]) {
            buf.extend(c.to_le_bytes());
        }
        std::fs::create_dir(link)?;
        let name: Vec<u16> = wide(link.as_os_str()).into_iter().chain([0]).collect();
        let r = unsafe {
            let h = CreateFileW(
                name.as_ptr(),
                GENERIC_WRITE,
                0,
                std::ptr::null_mut(),
                OPEN_EXISTING,
                FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
                0,
            );
            if h == -1 {
                Err(std::io::Error::last_os_error())
            } else {
                let mut ret = 0u32;
                let ok = DeviceIoControl(
                    h,
                    FSCTL_SET_REPARSE_POINT,
                    buf.as_ptr(),
                    buf.len() as u32,
                    std::ptr::null_mut(),
                    0,
                    &mut ret,
                    std::ptr::null_mut(),
                );
                let e = std::io::Error::last_os_error();
                CloseHandle(h);
                if ok != 0 {
                    Ok(())
                } else {
                    Err(e)
                }
            }
        };
        if r.is_err() {
            let _ = std::fs::remove_dir(link);
        }
        r
    }
}

/// removes a link made by link_dir, never what it points to
pub fn remove_dir_link(link: &Path) -> std::io::Result<()> {
    if cfg!(windows) {
        fs::remove_dir(link) // a junction (or a directory symbolic link) is removed as an empty folder
    } else {
        fs::remove_file(link)
    }
}

#[cfg(test)]
mod tests {
    #[test]
    fn link_dir_makes_a_link_that_reads_through() {
        let t = std::env::temp_dir().join(format!("wtbuild_link_{}", std::process::id()));
        let (target, link) = (t.join("target dir"), t.join("link"));
        std::fs::create_dir_all(&target).unwrap();
        std::fs::write(target.join("f"), b"x").unwrap();
        super::link_dir(&target, &link).unwrap();
        assert_eq!(std::fs::read(link.join("f")).unwrap(), b"x");
        assert!(std::fs::symlink_metadata(&link).unwrap().file_type().is_symlink());
        super::remove_dir_link(&link).unwrap();
        assert!(target.join("f").is_file());
        std::fs::remove_dir_all(&t).unwrap();
    }
}
