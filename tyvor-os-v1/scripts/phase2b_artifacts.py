"""Inspect/package Phase 2B software bench artifacts; never opens any device."""
import argparse
import difflib
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import uuid
os.environ["TYVOR_BUILD_PHASE"] = "phase2b"
from build_provenance import ROOT, BUILD_ROOT, EVIDENCE, verify_receipt
from package_artifacts import source_paths
from validate_esp_package import validate, verify_manifest
from validate_bench_ui import verify_ui

OUTPUT = ROOT / "artifacts/phase2b"

def check(condition, message):
    if not condition:
        raise ValueError(message)

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def inside(path):
    path = Path(path).resolve()
    check(path.is_relative_to(ROOT) and path != ROOT, "Mutation must stay inside workspace")
    return path

def inspect():
    from elftools.elf.elffile import ELFFile
    stm = BUILD_ROOT / "stm32"
    with (stm / "tyvor_stm32.elf").open("rb") as stream:
        elf = ELFFile(stream)
        check(elf.header["e_machine"] == "EM_ARM", "Expected ARM ELF")
        loads = [s for s in elf.iter_segments() if s["p_type"] == "PT_LOAD" and s["p_filesz"]
                 and 0x08000000 <= s["p_paddr"] < 0x08080000]
        end = max(s["p_paddr"] + s["p_filesz"] for s in loads)
        image = bytearray(end - 0x08000000)
        for segment in loads:
            check(segment["p_paddr"] + segment["p_filesz"] <= 0x08080000, "VE Flash overflow")
            offset = segment["p_paddr"] - 0x08000000
            image[offset:offset + segment["p_filesz"]] = segment.data()
        binary = (stm / "tyvor_stm32.bin").read_bytes()
        check(image == binary, "ELF / BIN load mismatch")
        sp, reset = struct.unpack_from("<II", binary)
        check(sp == 0x20020000 and reset == elf.header["e_entry"], "Bad vector/entry")
        vectors = struct.unpack_from("<98I", binary)
        check(all(x == 0 or (x & 1 and 0x08000000 <= x < end) for x in vectors[1:]), "Vector outside VE image")
        sections = {}
        for section in elf.iter_sections():
            if not section["sh_flags"] & 2 or not section["sh_size"]:
                continue
            address, size = section["sh_addr"], section["sh_size"]
            check(any(lo <= address and address + size <= hi for lo, hi in
                      ((0x08000000, 0x08080000), (0x20000000, 0x20020000), (0x10000000, 0x10010000))),
                  "Allocated STM32 section outside VE memory")
            sections[section.name] = dict(address=hex(address), size=size)
        ram_end = max(int(s["address"], 16) + s["size"] for s in sections.values()
                      if 0x20000000 <= int(s["address"], 16) < 0x20020000)
        stm_info = dict(bin_bytes=len(binary), elf_bytes=(stm / "tyvor_stm32.elf").stat().st_size,
                        elf_sha256=sha(stm / "tyvor_stm32.elf"), bin_sha256=sha(stm / "tyvor_stm32.bin"),
                        bin_matches_elf=True, sp=hex(sp), reset=hex(reset), vectors=98,
                        sections=sections, flash_capacity=524288, sram_region_bytes=ram_end - 0x20000000,
                        sram_capacity=131072, ccm_capacity=65536,
                        ccm_used=elf.get_section_by_name(".ccmram")["sh_size"])
        symbols = {s.name for s in elf.get_section_by_name(".symtab").iter_symbols()}
        check({"tyvor_core_poll", "tyvor_safety_step", "tyvor_mixer_run"}.issubset(symbols), "STM32 Core not linked")
        check("HAL_UART_Transmit" not in symbols, "Default STM32 must not link the unverified UART TX path")
    frames = []
    for path in stm.rglob("*.su"):
        for line in path.read_text().splitlines():
            fields = line.split("\t")
            if len(fields) >= 3 and fields[1].isdigit():
                frames.append(dict(function=fields[0], bytes=int(fields[1]), kind=fields[2]))
    stm_info["largest_per_function_stack"] = sorted(frames, key=lambda x: x["bytes"], reverse=True)[:15]
    idf_python = ROOT / ".tools/espressif/python_env/idf5.5_py3.11_env/Scripts/python.exe"
    esp = BUILD_ROOT / "esp32"
    with (esp / "tyvor_display.elf").open("rb") as stream:
        elf = ELFFile(stream)
        check(elf.header["e_machine"] == "EM_XTENSA", "Expected Xtensa ELF")
        symbols = {s.name for s in elf.get_section_by_name(".symtab").iter_symbols()}
        check("tyvor_ui_core_view" in symbols, "ESP UI state not linked")
        check(not ({"tyvor_core_select_model", "tyvor_safety_step", "tyvor_mixer_run", "tyvor_core_poll"} & symbols),
              "ESP must not link STM32 Core authority")
    size = json.loads(subprocess.run([str(idf_python), "-m", "esp_idf_size", "--target", "esp32s3",
                                     "--format", "json", str(esp / "tyvor_display.map")],
                                    capture_output=True, text=True, check=True).stdout)
    check(size["used_diram"] < size["diram_total"], "ESP static DIRAM exhausted")
    result = dict(hardware_tested=False, stm32=stm_info, esp32=dict(size=size,
        elf_sha256=sha(esp / "tyvor_display.elf"), bin_sha256=sha(esp / "tyvor_display.bin"), core_authority_linked=False, files={
        name: (esp / name).stat().st_size for name in
        ("tyvor_display.bin", "tyvor_display.elf", "bootloader/bootloader.bin", "partition_table/partition-table.bin")}))
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    (EVIDENCE / "memory-report.json").write_text(json.dumps(result, indent=2) + "\n")
    (EVIDENCE / "memory-report.txt").write_text(
        f"STM32 BIN={len(binary)}; SRAM region={stm_info['sram_region_bytes']}/131072; CCM={stm_info['ccm_used']}\n"
        f"ESP32 app={(esp / 'tyvor_display.bin').stat().st_size}; static DIRAM={size['used_diram']}/{size['diram_total']}\n"
        "Static layout only; no runtime stack/heap or board claim.\n")
    print((EVIDENCE / "memory-report.txt").read_text())

def receipts():
    for name, target, config in (("native-debug", "native", "Debug"), ("native-release", "native", "Release"),
                                 ("stm32", "stm32", "MinSizeRel"), ("esp32", "esp32", "IDF")):
        rec = verify_receipt(EVIDENCE / (name + "-receipt.json"))
        check((rec["target"], rec["config"], rec["profile"]) == (target, config, "phase2b"), "Receipt identity mismatch")

def changes(sources):
    baseline = ROOT / "build/phase2b-baseline/source-state.json"
    if not baseline.exists():
        return
    previous = {x["path"]: x["sha256"] for x in json.loads(baseline.read_text())}
    rows, patch = [], []
    for item in sources:
        name, digest = item["path"], item["sha256"]
        old = ROOT / "build/phase2b-baseline/source" / name
        status = "new" if name not in previous else "unchanged" if digest == previous[name] else "modified"
        if name in previous:
            check(old.is_file() and sha(old) == previous[name], "Frozen baseline changed")
        rows.append(dict(path=name, status=status, before_sha256=previous.get(name), after_sha256=digest))
        if status != "unchanged":
            patch.extend(difflib.unified_diff(old.read_text(encoding="utf-8-sig").splitlines(True) if old.exists() else [],
                         (ROOT / name).read_text(encoding="utf-8-sig").splitlines(True),
                         fromfile="a/" + name if old.exists() else "/dev/null", tofile="b/" + name))
    check(not (set(previous) - {x["path"] for x in rows}), "Unexpected source deletion")
    (EVIDENCE / "source-changes.json").write_text(json.dumps(dict(baseline_files=len(previous), current_files=len(rows), files=rows), indent=2) + "\n")
    (EVIDENCE / "phase2b-diff.patch").write_text("".join(patch), encoding="utf-8")

def package():
    receipts()
    check((EVIDENCE / "memory-report.json").is_file(), "Run inspect first using IDF Python")
    memory = json.loads((EVIDENCE / "memory-report.json").read_text())
    for target, base in (("stm32", "tyvor_stm32"), ("esp32", "tyvor_display")):
        for extension in ("elf", "bin"):
            check(memory[target][extension + "_sha256"] == sha(BUILD_ROOT / target / (base + "." + extension)),
                  "Memory inspection is stale: rebuild inspect evidence")
    cache = (BUILD_ROOT / "stm32/CMakeCache.txt").read_text()
    check("TYVOR_UART2_PA2_PA3_VERIFIED:BOOL=OFF" in cache.splitlines(), "STM32 UART gate must remain OFF")
    source_records = [dict(path=p.relative_to(ROOT).as_posix(), sha256=sha(p)) for p in source_paths()]
    changes(source_records)
    stage = inside(ROOT / ("artifacts/phase2b.staging-" + uuid.uuid4().hex[:8]))
    stage.mkdir(parents=True)
    def copy(source, destination):
        source, destination = Path(source), inside(stage / destination)
        check(source.is_file() and destination.is_relative_to(stage), "Unsafe/missing package file")
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)
    for name in ("tyvor_stm32.bin", "tyvor_stm32.elf", "tyvor_stm32.map"):
        copy(BUILD_ROOT / "stm32" / name, "stm32/" + name)
    copy(BUILD_ROOT / "stm32/CMakeCache.txt", "stm32/build-config.txt")
    for name in ("tyvor_display.bin", "tyvor_display.elf", "tyvor_display.map", "flasher_args.json",
                 "bootloader/bootloader.bin", "partition_table/partition-table.bin"):
        copy(BUILD_ROOT / "esp32" / name, "esp32/" + name)
    copy(ROOT / "esp32/sdkconfig", "esp32/sdkconfig.build")
    copy(ROOT / "esp32/partitions.csv", "esp32/partitions.csv")
    for config in ("debug", "release"):
        for name in ("test_protocol.exe", "test_reliability.exe", "test_core.exe", "tyvor_core_sim.exe", "test_bench.exe", "test_bench_regressions.exe", "tyvor_bench_ui_export.exe"):
            copy(BUILD_ROOT / ("native-" + config) / name, "native/" + config + "/" + name)
    for path in (ROOT / "sim/scenarios").glob("*.txt"):
        copy(path, "scenarios/" + path.name)
    for path in (ROOT / "sim/bench_scenarios").glob("*.txt"):
        copy(path, "bench-scenarios/" + path.name)
    ui = list((ROOT / "artifacts/phase2b-ui").glob("*.svg"))
    check(len(ui) == 35, "Expected 35 Core UI views")
    verified_ui = verify_ui(ROOT / "artifacts/phase2b-ui")
    ui_receipt = json.loads((EVIDENCE / "ui-validation.json").read_text())
    check(ui_receipt["files"] == verified_ui and ui_receipt["exporter_sha256"] == sha(BUILD_ROOT / "native-release/tyvor_bench_ui_export.exe"), "Stale UI evidence")
    for path in ui:
        copy(path, "ui/" + path.name)
    baseline = ROOT / "build/phase2b-baseline"
    for path in baseline.glob("*-before.txt"):
        copy(path, "evidence/before-fix/" + path.name)
    for name in ("debug.xml", "release.xml", "debug.txt", "release.txt", "source-state.json", "PHASE2A_MANIFEST.json"):
        if (baseline / name).is_file():
            copy(baseline / name, "evidence/before-fix/" + name)
    for path in EVIDENCE.iterdir():
        if (path.is_file() and path.suffix in (".json", ".xml", ".txt", ".patch", ".png")
                and "pending" not in path.name and not path.name.startswith("final-manifest-validation")):
            copy(path, "evidence/" + path.name)
    offline = validate(stage / "esp32")
    (stage / "evidence/esp-package-validation.json").write_text(json.dumps(offline, indent=2) + "\n")
    env = os.environ.copy();env.pop("GIT_CEILING_DIRECTORIES", None)
    status = subprocess.run(["git", "status", "--short", "--untracked-files=all"], cwd=ROOT, env=env,
                            capture_output=True, text=True, check=True).stdout
    (stage / "git-status.txt").write_text(status, encoding="utf-8")
    patches = []
    for item in source_records:
        diff = subprocess.run(["git", "diff", "--no-index", "--", "NUL", item["path"]], cwd=ROOT,
                              env=env, capture_output=True)
        check(diff.returncode in (0, 1), "Cannot export WIP snapshot")
        patches.append(diff.stdout)
    (stage / "evidence/source-snapshot.patch").write_bytes(b"\n".join(patches))
    files = [dict(path=p.relative_to(stage).as_posix(), size=p.stat().st_size, sha256=sha(p))
             for p in sorted(stage.rglob("*")) if p.is_file()]
    manifest = dict(schema=2, phase="TYVOR Phase 2B CORE BENCH", wire_protocol=2, core_monitor_revision=2, read_lease_timeout_ms=500, bench_only=True,
                    hardware_tested=False, rf_outputs=False, uart_pin_gates="OFF", files=files, sources=source_records)
    manifest_path = stage / "SHA256_MANIFEST.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
    verify_manifest(stage / "esp32", manifest_path, ROOT)
    receipts()
    if OUTPUT.exists():
        check(not OUTPUT.is_symlink(), "Unsafe old package")
        backup = inside(ROOT / ("build/phase2b-baseline/package-" + uuid.uuid4().hex[:8]))
        check(not backup.exists(), "Backup target exists")
        backup.parent.mkdir(parents=True, exist_ok=True)
        inside(OUTPUT).rename(backup)
    inside(stage).rename(inside(OUTPUT))
    print(f"PHASE 2B OFFLINE PACKAGE PASS: {len(files)} artifacts, {len(source_records)} source hashes")
    print("No flash/serial/commit/push/deploy. Phase 1/2A packages untouched.")

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("inspect", "package"))
    args = parser.parse_args()
    inspect() if args.action == "inspect" else package()

if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, KeyError, subprocess.CalledProcessError) as exc:
        print("PHASE 2B ARTIFACT FAIL:", exc, file=sys.stderr)
        sys.exit(1)
