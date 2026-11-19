"""Gate and collect verified builds. Offline only: no device or Git write actions."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import uuid
from build_provenance import ROOT, EVIDENCE, verify_receipt, label
from validate_esp_package import validate, verify_manifest

OUTPUT = ROOT / "artifacts/phase1"

def checked_workspace(path):
    path = Path(path).resolve()
    if not path.is_relative_to(ROOT) or path == ROOT:
        raise ValueError("Artifact mutation outside workspace: " + str(path))
    return path

def sha(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()

def source_paths():
    paths = [ROOT / name for name in ("README.md", "CMakeLists.txt", ".gitignore")]
    for folder in ("shared", "stm32", "esp32", "sim", "cmake", "scripts", "docs"):
        paths += [p for p in (ROOT / folder).rglob("*") if p.is_file()
                  and "build" not in p.relative_to(ROOT / folder).parts
                  and p.suffix in (".c", ".h", ".ld", ".cmake", ".txt", ".py", ".ps1", ".json", ".md", ".defaults", ".csv", ".projbuild")
                  and p.name not in ("sdkconfig", "sdkconfig.old")]
    return sorted(set(paths))

def main():
    for target, config in (("native", "Debug"), ("native", "Release"),
                            ("stm32", "MinSizeRel"), ("esp32", "IDF")):
        rec = verify_receipt(EVIDENCE / (label(target, config) + "-receipt.json"))
        if (rec["target"], rec["config"]) != (target, config):
            raise ValueError("Receipt target/config mismatch")
    sources = [dict(path=p.relative_to(ROOT).as_posix(), sha256=sha(p)) for p in source_paths()]
    stm_cache = (ROOT / "build/stm32/CMakeCache.txt").read_text()
    if "TYVOR_UART2_PA2_PA3_VERIFIED:BOOL=OFF" not in stm_cache.splitlines():
        raise ValueError("STM32 unverified UART gate must remain OFF")
    stage = checked_workspace(ROOT / ("artifacts/phase1.staging-" + uuid.uuid4().hex[:8]))
    stage.mkdir(parents=True)
    def copy(source, destination):
        src = ROOT / source
        dst = checked_workspace(stage / destination)
        if not src.is_file() or not dst.is_relative_to(stage):
            raise ValueError("Invalid package copy: " + str(src))
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)
    for name in ("tyvor_stm32.elf", "tyvor_stm32.bin", "tyvor_stm32.map"):
        copy("build/stm32/" + name, "stm32/" + name)
    copy("build/stm32/CMakeCache.txt", "stm32/build-config.txt")
    for name in ("tyvor_display.elf", "tyvor_display.bin", "tyvor_display.map", "flasher_args.json",
                 "bootloader/bootloader.bin", "partition_table/partition-table.bin"):
        copy("build/esp32/" + name, "esp32/" + name)
    copy("esp32/sdkconfig", "esp32/sdkconfig.build")
    copy("esp32/partitions.csv", "esp32/partitions.csv")
    for name in ("test_protocol.exe", "test_reliability.exe", "tyvor_ui_sim.exe"):
        copy("build/native-release/" + name, "native/" + name)
    historical = {"artifact-verification.json", "esp-idf-python-freeze.txt", "idf-install-python.txt",
                  "esp32-build-console.txt", "stm32-build-console.txt", "stm32-build-fixed.txt",
                  "stm32-elf.txt", "stm32-stack-usage.txt"}
    for source in EVIDENCE.iterdir():
        if (source.is_file() and source.suffix in (".txt", ".xml", ".json")
                and "pending" not in source.name and not source.name.startswith("esp-package-final-validation")
                and source.name != "package-final-build.txt"):
            folder = "evidence/historical/" if source.name in historical else "evidence/"
            copy(source.relative_to(ROOT), folder + source.name)
    for name in ("reliability-diff.patch", "reliability-source-changes.json"):
        if (EVIDENCE / name).is_file():
            copy((EVIDENCE / name).relative_to(ROOT), "evidence/" + name)
    baseline = ROOT / "build/reliability-baseline/evidence"
    if baseline.exists():
        for source in baseline.iterdir():
            if source.is_file():
                copy(source.relative_to(ROOT), "evidence/before-fix/" + source.name)
    for name in ("source-state.json", "SHA256_MANIFEST.json"):
        if (ROOT / "build/reliability-baseline" / name).is_file():
            copy("build/reliability-baseline/" + name, "evidence/before-fix/" + name)
    for name in ("tyvor_protocol.c", "tyvor_protocol.h", "tyvor_status.h"):
        if (ROOT / "build/reliability-baseline/shared" / name).is_file():
            copy("build/reliability-baseline/shared/" + name, "evidence/before-fix/shared/" + name)
    for source in (ROOT / "artifacts/ui-mock").glob("*.svg"):
        copy(source.relative_to(ROOT), "ui-mock/" + source.name)
    offline = validate(stage / "esp32")
    (stage / "evidence/esp-package-validation.json").write_text(json.dumps(offline, indent=2) + "\n")
    env = os.environ.copy()
    env.pop("GIT_CEILING_DIRECTORIES", None)
    status = subprocess.run(["git", "status", "--short", "--untracked-files=all"], cwd=ROOT,
                            env=env, capture_output=True, text=True, check=True).stdout
    (stage / "git-status.txt").write_text(status, encoding="utf-8")
    diff = subprocess.run(["git", "diff", "--stat"], cwd=ROOT, env=env,
                          capture_output=True, text=True, check=True).stdout
    (stage / "git-diff-stat.txt").write_text(diff or "No tracked diff: WIP source is untracked. See source-snapshot.patch.\n")
    patch = []
    for item in sources:
        proc = subprocess.run(["git", "diff", "--no-index", "--", "NUL", item["path"]],
                              cwd=ROOT, env=env, capture_output=True)
        if proc.returncode not in (0, 1):
            raise ValueError(proc.stderr.decode(errors="replace"))
        patch.append(proc.stdout)
    (stage / "evidence/source-snapshot.patch").write_bytes(b"\n".join(patch))
    files = [dict(path=p.relative_to(stage).as_posix(), size=p.stat().st_size, sha256=sha(p))
             for p in sorted(stage.rglob("*")) if p.is_file()]
    manifest = dict(schema=2, phase="TYVOR OS V1 Phase 1 Reliability WIP", wire_protocol=2,
                    hardware_tested=False, uart_pin_gates="OFF", files=files, sources=sources)
    manifest_path = stage / "SHA256_MANIFEST.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    verify_manifest(stage / "esp32", manifest_path, ROOT)
    # Verify no input/output was changed during staging before replacing the old package.
    for target, config in (("native", "Debug"), ("native", "Release"), ("stm32", "MinSizeRel"), ("esp32", "IDF")):
        verify_receipt(EVIDENCE / (label(target, config) + "-receipt.json"))
    if OUTPUT.exists():
        backup = checked_workspace(ROOT / ("build/reliability-baseline/phase1-old-package-" + uuid.uuid4().hex[:8]))
        origin = checked_workspace(OUTPUT)
        if backup.exists() or OUTPUT.is_symlink():
            raise ValueError("Unsafe artifact backup target")
        backup.parent.mkdir(parents=True, exist_ok=True)
        print("Preserving previous package:", origin, "->", backup)
        origin.rename(backup)
    checked_workspace(stage).rename(checked_workspace(OUTPUT))
    print("OFFLINE PACKAGE PASS:", len(files), "artifacts,", len(sources), "source hashes;", OUTPUT)
    print("No serial connection, flashing, commit, push or deployment was performed.")

if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, KeyError, subprocess.CalledProcessError) as exc:
        print("PACKAGE BLOCKED:", exc, file=sys.stderr)
        sys.exit(1)
