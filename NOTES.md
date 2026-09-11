# Google Patch Rewards — xz (liblzma) local draft notes

**Date:** 2026-09-11 (America/Chicago)  
**Do not claim yet:** need upstream merge + ≥30 days, then https://bughunters.google.com/report/patch_rewards

## Chosen target + why

- **Project:** xz / liblzma (tukaani-project/xz) — Tier-1 **Widespread decompression** (also in scope for Core infrastructure data parsers)
- **Upstream:** https://github.com/tukaani-project/xz (GitHub PR; base `master`)
- **Local clone:** `/workspace/google-patch-xz`
- **Branch:** `local/dict-fbounds-safety` (from `master`)
- **Why this target:**
  1. Tier-1 widespread decompression library (xz / .lzma / LZMA2) used across Linux distros, package managers, firmware, and containers.
  2. Clear, mergeable first-CL scope: **one** private struct field pair (`lzma_dict.buf` / `lzma_dict.size`) on the shared LZ decode / dictionary path — not a whole-library sweep.
  3. Same pattern as libpng / libwebp / giflib: **inert macros** when the flag is off; experimental Clang `-fbounds-safety` only when explicitly enabled.
  4. Private `lzma_dict` (not public ABI): safe to declare `size` before `buf` for the attribute.
  5. Not already done upstream (no `__counted_by` / `-fbounds-safety` in tree; no overlapping open PRs for counted_by as of 2026-09-11).
  6. AI CONTRIBUTING gate clear (no CONTRIBUTING.md; no AI ban in README / AUTHORS / `.github`).

## Security benefit

`lzma_dict.buf` is the history / out-window buffer for LZ-based decompressors (LZMA, LZMA2, etc.). Untrusted compressed input drives writes and history reads through this buffer. The library already tracks allocated capacity in `lzma_dict.size`, but the compiler cannot see that relationship.

This draft:

1. Introduces `lzma_bounds_safety.h` with `LZMA_COUNTED_BY` / `LZMA_COUNTED_BY_OR_NULL` (empty by default).
2. Annotates `lzma_dict.buf` with `LZMA_COUNTED_BY_OR_NULL(size)` (nullable; size may be 0).
3. Declares `size` **before** `buf` so the named count is in scope for the attribute.
4. Makes update sites **capacity-then-pointer** (and clear/free sites **pointer-null then size 0**) so counted-by invariants hold under a real `-fbounds-safety` build.
5. Wires optional CMake `XZ_ENABLE_FBOUNDS_SAFETY` (OFF by default) → `-DLZMA_SUPPORT_FBOUNDS_SAFETY` + `-fbounds-safety`.

**Default builds are unchanged:** macros expand to nothing; no new runtime checks without the experimental flag.

**Note on `LZ_DICT_EXTRA`:** allocation is `size + LZ_DICT_EXTRA` so optimized `dict_repeat()` may touch a few bytes past `size`. The annotation ties `buf` to the existing `size` field (logical capacity used for wrap/limit). A real `-fbounds-safety` build with `LZMA_LZ_DECODER_CONFIG >= 2` may need a follow-up (force config 0/1 under the flag, or express slack explicitly). Default / inert builds are unaffected.

## Files changed

| File | Change |
|------|--------|
| `src/liblzma/common/lzma_bounds_safety.h` | **New** — inert / Clang bounds macros |
| `src/liblzma/lz/lz_decoder.h` | Include header; declare `size` before `buf`; annotate `buf` |
| `src/liblzma/lz/lz_decoder.c` | Capacity-first assign; clear/free pointer-then-size |
| `CMakeLists.txt` | `XZ_ENABLE_FBOUNDS_SAFETY` option OFF; list new header |
| `NOTES.md` | This file |

## Verified locally 2026-09-11

| Check | Result |
|-------|--------|
| Default `XZ_ENABLE_FBOUNDS_SAFETY=OFF` cmake build | **PASS** (gcc; static liblzma + tools) |
| `ctest` (22 tests) | **PASS** |
| `XZ_ENABLE_FBOUNDS_SAFETY=ON` | **Not feasible on this box** — needs Clang with `-fbounds-safety` / `ptrcheck.h` |

## How to build / test

Default (macros inert — must stay green):

```sh
cmake -S . -B build-draft -DBUILD_SHARED_LIBS=OFF
cmake --build build-draft -j
ctest --test-dir build-draft --output-on-failure
```

With experimental bounds-safety toolchain (maintainers / CI; **not** available on this box — no Clang/`ptrcheck.h`):

```sh
cmake -S . -B build-fbs -DXZ_ENABLE_FBOUNDS_SAFETY=ON \
  -DCMAKE_C_COMPILER=<clang-with-fbounds-safety>
cmake --build build-fbs -j
```

## Upstream submit plan

1. Open a focused GitHub PR against `tukaani-project/xz` branch **`master`**.
2. Proposed title: `liblzma: add optional -fbounds-safety annotation for lzma_dict.buf`
3. Frame as secure-by-design / Safe Buffers-style systematization of an existing size+pointer pair; cite libwebp/libpng prior art and Google Patch Rewards Tier-1 decompression goals.
4. Emphasize: default build behavior unchanged; flag OFF; no PoC / no CVE claim.
5. Do **not** claim on https://bughunters.google.com/report/patch_rewards until **merge + ≥30 days**.

## Follow-ups (separate CLs)

- `worker_thread.in` / `in_size` in `stream_decoder_mt.c`
- `lzma_outbuf.buf[]` / `allocated` (FAM — different annotation shape)
- Express or gate `LZ_DICT_EXTRA` slack under a real `-fbounds-safety` build
- Autotools `--enable-fbounds-safety` mirror of the CMake option

## AI gate

- **No `CONTRIBUTING.md`** in upstream tree.
- Searched README, AUTHORS, THANKS, TODO, PACKAGERS, `.github/` — **no AI / LLM / Copilot ban**.
- Maintainer has asked in the past whether some PRs were LLM-generated (style question only; not a ban). Be ready to discuss authorship if asked.
- **AI gate: CLEAR** (no ban found).

## Overlapping PRs

Checked open PRs on 2026-09-11 (e.g. #242, #241, #235, #181, #177, #173, #120, #229). **None** add `__counted_by` / `-fbounds-safety`. Safe to proceed with local draft.

## Status

**LOCAL DRAFT ONLY** — commit on `local/dict-fbounds-safety`. Do **not** push/PR from this agent run. Still **no claim** until merge + ≥30 days unreverted.
