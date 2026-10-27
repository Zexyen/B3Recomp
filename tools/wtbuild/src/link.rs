//! The `link` step (scripts/build_runtime.py, Linux): build/game.so from the lifted objects, the host runtime into
//! build/obj_host, build/witch-time-game, libssbu_glslang.so (the shader compiler shim) and libwitchtime.so (the
//! launcher's: the same program as a library; a failure there does not fail the build).

use crate::compile::{run, tail};
use std::fs;
use std::path::Path;
use std::process::Command;
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::Mutex;
use std::time::Instant;

/// build_runtime.py HOST_SOURCES (Linux: plus the POSIX platform layer)
const HOST_SOURCES: &[&str] = &[
    "hostrt.c",
    "boot.c",
    "nnsdk.c",
    "libcxx.c",
    "os_threads.c",
    "nvn_stub.c",
    "nvn_gl.c",
    "nvn_vk.c",
    "mx_translate.c",
    "tex_decode.c",
    "nnfs.c",
    "clib.c",
    "nntime.c",
    "nnhid.c",
    "nnpad_sdl.c",
    "gen/sdkdata.c",
    "display.c",
    "nnpl.c",
    "nnfontll.c",
    "nnmovie.c",
    "nnmisc.c",
    "nnaudio.c",
    "nnmovie_mf.c",
    "nniostream.c",
    "nncxxrt.c",
    "nncxxlib.c",
    "nnlibc2.c",
    "gen/sdklift_gen.c",
    "sdklift_rt.c",
    "nnsdk2.c",
    "nnapplet.c",
    "nnalbum.c",
    "nnnet.c",
    "nnfsdir.c",
    "nnunwind.c",
    "nvn_sdk.c",
    "nvn_c4.c",
    "nvn_c5.c",
    "gfx_options.c",
    "hitch.c",
    "r0_removed.c",
    "r2_imports.c",
    "b3_imports.c",
    "watch.c",
    "pkz_dump.c",
    "gamedll.c",
    "pcmenu.c",
    "options_row.c",
    "plat_posix.c",
    "plat_audio_sdl.c",
];
/// the system's SDL3, OpenGL, OpenSSL and FFmpeg (movies)
const LIBS: &[&str] = &[
    "-lSDL3",
    "-lGL",
    "-lcrypto",
    "-ldl",
    "-lpthread",
    "-lm",
    "-lavformat",
    "-lavcodec",
    "-lswscale",
    "-lswresample",
    "-lavutil",
];

pub fn run_step(repo: &Path, clang: &str, env: &(dyn Fn(&mut Command) + Sync), log: &mut dyn FnMut(String)) -> Result<(), String> {
    let rt = repo.join("runtime");
    let build = repo.join("build");
    let (obj, objh) = (build.join("obj"), build.join("obj_host"));
    let (dll, exe) = (build.join("game.so"), build.join("witch-time-game"));
    let s = |p: &Path| p.to_string_lossy().into_owned();
    fs::create_dir_all(&objh).map_err(|e| e.to_string())?;
    let cmd = |args: &[String]| {
        let mut c = Command::new(clang);
        env(&mut c);
        c.args(args);
        c
    };
    let strs = |v: &[&str]| v.iter().map(|x| x.to_string()).collect::<Vec<String>>();

    // 1. game.so: the lifted objects + the landing-pad resume table
    let mut objs: Vec<String> = fs::read_dir(&obj)
        .map_err(|e| e.to_string())?
        .flatten()
        .map(|e| s(&e.path()))
        .filter(|p| p.ends_with(".obj"))
        .collect();
    objs.sort();
    let rsp = build.join("game_dll.rsp");
    fs::write(&rsp, objs.iter().map(|o| format!("\"{}\"\n", o.replace('\\', "/"))).collect::<String>()).map_err(|e| e.to_string())?;
    let t = Instant::now();
    let mut a = strs(&["-shared", "-O1", "-w", "-mcx16", "-I"]);
    a.extend([s(&rt), s(&rt.join("game_shim_eh.c")), format!("@{}", s(&rsp)), "-o".into(), s(&dll)]);
    a.extend(strs(&["-fPIC", "-fuse-ld=lld", "-D__declspec(x)=", "-Xlinker", "-soname=game.so"]));
    let (code, out, _) = run(&mut cmd(&a), None);
    log(format!(
        "game.dll: exit {} in {:.0}s, {} objects",
        code.unwrap_or(-1),
        t.elapsed().as_secs_f64(),
        objs.len()
    ));
    if code != Some(0) {
        log(tail(&out, 4000).to_string());
        return Err("game.so did not link".into());
    }

    // 2. the host runtime
    let gen = rt.join("gen");
    let mut flags = vec!["-I".to_string(), s(&rt.join("posix"))];
    flags.extend(strs(&["-O1", "-g", "-Wno-everything", "-fno-strict-aliasing", "-mcx16", "-I"]));
    flags.extend([s(&rt), "-I".into(), s(&gen)]);
    flags.extend(strs(&[
        "-Dfn_table=(*game_fn_table_p)",
        "-Dfn_table_count=(*game_fn_table_count_p)",
        "-Ddyn_syms=(*game_dyn_syms_p)",
        "-Ddyn_sym_count=(*game_dyn_sym_count_p)",
        "-D_GNU_SOURCE",
        "-fPIC",
        "-D__declspec(x)=",
        "-D__stdcall=",
        "-D__cdecl=",
        "-Werror=implicit-function-declaration",
        "-Werror=incompatible-function-pointer-types",
        "-Werror=int-conversion",
    ]));
    let t = Instant::now();
    let host_objs: Vec<String> = HOST_SOURCES
        .iter()
        .map(|n| s(&objh.join(format!("{}.obj", Path::new(n).file_stem().unwrap().to_string_lossy()))))
        .collect();
    let failed = Mutex::new(Vec::new());
    let next = AtomicUsize::new(0);
    std::thread::scope(|sc| {
        for _ in 0..16 {
            sc.spawn(|| loop {
                let i = next.fetch_add(1, Ordering::SeqCst);
                let Some(name) = HOST_SOURCES.get(i) else { break };
                let mut a = vec!["-c".to_string()];
                a.extend(flags.iter().cloned());
                a.extend([s(&rt.join(name)), "-o".into(), host_objs[i].clone()]);
                let (code, out, _) = run(&mut cmd(&a), None);
                if code != Some(0) {
                    failed.lock().unwrap().push((i, name.to_string(), out));
                }
            });
        }
    });
    let mut failed = failed.into_inner().unwrap();
    failed.sort_by_key(|f| f.0);
    log(format!(
        "host: {} files compiled in {:.0}s, {} failed",
        HOST_SOURCES.len(),
        t.elapsed().as_secs_f64(),
        failed.len()
    ));
    for (_, name, out) in &failed {
        log(format!("==== {}\n{}", name, tail(out, 2500)));
    }
    if !failed.is_empty() {
        return Err("the host runtime did not compile".into());
    }

    // 3. witch-time-game: game.so beside the program (rpath $ORIGIN)
    let mut a = strs(&["-fuse-ld=lld", "-g", "-rdynamic", "-Xlinker", "--no-dependent-libraries"]);
    a.extend(host_objs.iter().cloned());
    a.extend([s(&dll), "-Xlinker".into(), "-rpath=$ORIGIN".into()]);
    a.extend(strs(LIBS));
    a.extend(["-o".into(), s(&exe)]);
    let (code, out, _) = run(&mut cmd(&a), None);
    log(format!("witch-time-game: link exit {}", code.unwrap_or(-1)));
    if code != Some(0) {
        log(tail(&out, 6000).to_string());
        return Err("witch-time-game did not link".into());
    }

    // 4. the shader compiler shim against the system's glslang; LoadLibraryA("ssbu_glslang.dll") opens it beside the program
    let so = build.join("libssbu_glslang.so");
    let mut a = strs(&["-shared", "-fPIC", "-O2", "-w", "-D_GNU_SOURCE", "-D__declspec(x)=", "-I"]);
    a.extend([s(&rt.join("posix")), "-I".into(), s(&rt), s(&rt.join("vk_glslang_shim.c"))]);
    a.extend(strs(&["-lglslang", "-lglslang-default-resource-limits", "-o"]));
    a.push(s(&so));
    let (code, out, _) = run(&mut cmd(&a), None);
    log(format!("libssbu_glslang.so: exit {}", code.unwrap_or(-1)));
    if code != Some(0) {
        log(tail(&out, 3000).to_string());
        return Err("libssbu_glslang.so did not build".into());
    }

    // 5. the same host objects as a library for the launcher (boot.c wt_host_main); without it the launcher starts
    // witch-time-game instead, so a failure here does not fail the build
    let host = build.join("libwitchtime.so");
    let mut a = strs(&["-shared", "-fuse-ld=lld", "-g", "-Xlinker", "--no-dependent-libraries"]);
    a.extend(host_objs.iter().cloned());
    a.extend([
        s(&dll),
        "-Xlinker".into(),
        "-rpath=$ORIGIN".into(),
        "-Xlinker".into(),
        "-soname=libwitchtime.so".into(),
    ]);
    a.extend(strs(LIBS));
    a.extend(["-o".into(), s(&host)]);
    let (code, out, _) = run(&mut cmd(&a), None);
    log(format!(
        "libwitchtime.so: exit {}{}",
        code.unwrap_or(-1),
        if code == Some(0) {
            ""
        } else {
            " (the launcher starts witch-time-game instead)"
        }
    ));
    if code != Some(0) {
        log(tail(&out, 3000).to_string());
    }
    log(format!(
        "{}: {:.1} MB (TOOLCHAIN=linux)",
        s(&exe),
        fs::metadata(&exe).map(|m| m.len()).unwrap_or(0) as f64 / (1 << 20) as f64
    ));
    Ok(())
}
