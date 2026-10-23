# Local lifter copy (GPL-3.0, from ssbu-recomp)

Copied from https://github.com/Denkishi/ssbu-recomp `tools/lifter/` at commit `3a96916` (2026-10-16).
License: GPL-3.0 (`LICENSE`).

Changes from upstream:
- `recomp.py`: `ELF` points at `../targets/exefs/main.elf`; `TEXT_SIZE`, `RELA/RELASZ`, `JMPREL/PLTRELSZ`,
  `SYMTAB/STRTAB` set from our main.elf's DYNAMIC segment; added `import os`.
- `recomp_eh.py`: unchanged.

Full lift (writes `eh_main_tables.json` in the current directory; deletes stray `.c` files in `<outdir>`):

    cd lifter
    python recomp_eh.py ..\build\gen_eh 16

Further changes (2026-10-15):
- `recomp.py`: exclusive pairs (`ldxp/ldaxp/stxp/stlxp`), `{s,u}{mull,mlal,mlsl}{2}`, `{s,u}qxtn{2}`,
  `{s,u}rshr`, `sq{r}shrn{2}`, `fcvtz{s,u}` (fixed-point). Marked `[Recomps#1]`.
- `rt_ext.h` (new): the C helpers those emit. Runtime must add `uint64_t ex_val2;` to `Ctx` and build with `-mcx16`.
- `recomp_eh.py`: every `.eh_frame` FDE start is a function entry; generated files include `rt_ext.h`.
- `test_ext.py` (new): decoder + semantics check of the added forms against Unicorn (`pip install unicorn capstone`).

## b3recomp changes (2026-10-23)
- `recomp.py`: layout (`TEXT_SIZE`, `RELA`, `JMPREL`, `SYMTAB`, `STRTAB`) read from the ELF's first PT_LOAD and DYNAMIC
  (`_layout`), so the same lifter runs on Bayonetta 3's main.
- `recomp.py` + `rt_ext.h`, marked `[b3recomp]`: the AdvSIMD forms B3's main uses and B2's did not: `{s,u}{max,min}`,
  `{s,u}{r,q}shl` by register, `{s,u}q{add,sub}`, `mla`/`mls`, `addp` (vector and scalar), `{s,u}{addl,addw,subl,subw}{2}`,
  `addv`/`{s,u}{max,min}v`/`{s,u}addlv`, `frint{n,m,p,z,a}`, `fcvta{s,u}`, `{s,u}addlp`/`{s,u}adalp`, `{s,u}sra`,
  `{r}shrn{2}`, `orr`/`bic` (vector, immediate), integer `mul`/`mla`/`mls`/`sq{r}dmulh` by element, one-lane
  `ld2..ld4`/`st2..st4` and `ld2r..ld4r`. With them every instruction of B3's main translates (what is left is
  zero padding, the MOD0 header and `udf` traps).
- `test_b3.py` (new): every word of your main.elf that uses these forms, run in Unicorn and in C with the real helpers
  (all 32 vector registers, the base register and memory compared). On the developer's dump: 200 words, 4,000 cases,
  0 failures. Needs `pip install unicorn` (a venv is fine).
