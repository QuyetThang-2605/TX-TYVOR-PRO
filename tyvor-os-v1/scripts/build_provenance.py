"""Bind actual successful build/test outputs to their inputs. No device/Git writes."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time
import uuid
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parent.parent
PROFILE = os.environ.get("TYVOR_BUILD_PHASE", "phase1")
if PROFILE not in ("phase1", "phase2a", "phase2b", "phase2c"):
    raise ValueError("Unknown build phase")
BUILD_ROOT = ROOT / ("build/" + PROFILE if PROFILE != "phase1" else "build")
EVIDENCE = BUILD_ROOT / "evidence"

def sha(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()

def inventory():
    text = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8-sig")
    names = []
    for key in ("TYVOR_TEST_CASES", "TYVOR_RELIABILITY_CASES", "TYVOR_CORE_CASES", "TYVOR_SCENARIO_CASES",
                "TYVOR_BENCH_REGRESSION_CASES", "TYVOR_BENCH_CASES", "TYVOR_BENCH_SCENARIOS", "TYVOR_TIMING_CASES"):
        match = re.search(r"set\(" + key + r"\s+(.*?)\)", text, re.S)
        if not match:
            raise ValueError("Missing test inventory " + key)
        names.extend(match.group(1).split())
    if len(names) != len(set(names)) or not names:
        raise ValueError("Empty or duplicate test inventory")
    return names

def inputs(target):
    paths = set()
    for folder in ("shared", "cmake"):
        paths.update(p for p in (ROOT / folder).rglob("*") if p.is_file())
    paths.update(ROOT / name for name in
                 ("scripts/tools.lock.json", "scripts/esp-idf-python-lock.txt",
                  "scripts/build-phase1.ps1", "scripts/build_provenance.py"))
    paths.add(ROOT / "scripts/build-phase2a.ps1")
    paths.add(ROOT / "scripts/build-phase2b.ps1")
    paths.add(ROOT / "scripts/build-phase2c.ps1")
    if target in ("native", "stm32"):
        paths.update(p for p in (ROOT / "stm32/engine").rglob("*") if p.is_file())
    if target == "native":
        paths.add(ROOT / "CMakeLists.txt")
        paths.update(p for p in (ROOT / "sim").rglob("*") if p.suffix in (".c", ".h"))
        paths.update((ROOT / "sim/scenarios").glob("*.txt"))
        paths.update((ROOT / "sim/bench_scenarios").glob("*.txt"))
        paths.update(ROOT / name for name in
                     ("stm32/Core/Inc/tyvor_bridge.h", "stm32/Core/Src/tyvor_bridge.c"))
    elif target == "stm32":
        paths.update(p for p in (ROOT / "stm32").rglob("*")
                     if p.is_file() and p.suffix in (".c", ".h", ".ld", ".txt", ".cmake"))
    elif target == "esp32":
        paths.update(p for p in (ROOT / "esp32").rglob("*")
                     if p.is_file() and "build" not in p.relative_to(ROOT / "esp32").parts
                     and p.suffix in (".c", ".h", ".txt", ".csv", ".defaults", ".projbuild"))
    else:
        raise ValueError("Unknown target")
    return [dict(path=p.relative_to(ROOT).as_posix(), sha256=sha(p)) for p in sorted(paths)]

def check_junit(path, expected):
    suite = ET.parse(path).getroot()
    cases = suite.findall("testcase")
    if (int(suite.get("tests", "-1")) != len(expected) or len(cases) != len(expected)
            or {c.get("name") for c in cases} != set(expected)):
        raise ValueError("JUnit inventory/count mismatch")
    for key in ("failures", "errors", "skipped", "disabled"):
        if int(suite.get(key, "0")):
            raise ValueError("JUnit " + key + " is nonzero")
    if any(c.find(tag) is not None for c in cases for tag in ("failure", "error", "skipped")):
        raise ValueError("JUnit contains a failed/skipped test")
    return len(cases)

def check_native(build, config):
    expected = inventory()
    count = check_junit(EVIDENCE / ("native-" + config.lower() + ".xml"), expected)
    listing = json.loads(subprocess.run(
        [str(ROOT / ".tools/cmake-3.30.9-windows-x86_64/bin/ctest.exe"),
         "--test-dir", str(build), "--show-only=json-v1"],
        capture_output=True, text=True, check=True).stdout)
    if {x["name"] for x in listing["tests"]} != set(expected) or len(listing["tests"]) != count:
        raise ValueError("CTest inventory mismatch")
    for case in listing["tests"]:
        executable = Path(case["command"][0]).name
        name = case["name"]
        if name in legacy_inventory():
            expected_executable, arguments = "test_protocol.exe", [name]
        elif name in group_inventory("TYVOR_RELIABILITY_CASES"):
            expected_executable, arguments = "test_reliability.exe", [name]
        elif name in group_inventory("TYVOR_CORE_CASES"):
            expected_executable, arguments = "test_core.exe", [name]
        elif name in group_inventory("TYVOR_BENCH_REGRESSION_CASES"):
            expected_executable, arguments = "test_bench_regressions.exe", [name]
        elif name in group_inventory("TYVOR_BENCH_CASES"):
            expected_executable, arguments = "test_bench.exe", [name]
        elif name in group_inventory("TYVOR_TIMING_CASES"):
            expected_executable, arguments = "test_timing.exe", [name]
        else:
            expected_executable = "tyvor_core_sim.exe"
            folder = "sim/bench_scenarios" if name in group_inventory("TYVOR_BENCH_SCENARIOS") else "sim/scenarios"
            arguments = ["--script", str(ROOT / folder / (name + ".txt"))]
        actual_arguments = [str(Path(x)) if i == 1 and expected_executable == "tyvor_core_sim.exe" else x
                            for i, x in enumerate(case["command"][1:])]
        if executable != expected_executable or actual_arguments != arguments:
            raise ValueError("CTest command does not execute the declared case")
    commands = json.loads((build / "compile_commands.json").read_text())
    for source in ("test_protocol.c", "test_reliability.c", "test_core.c", "test_bench.c", "test_bench_regressions.c", "test_timing.c", "tyvor_core_sim.c", "tyvor_core.c"):
        command = next(x["command"] for x in commands if Path(x["file"]).name == source)
        if ("-DNDEBUG" in command) != (config == "Release"):
            raise ValueError("Wrong NDEBUG mode for " + source)
        if source in ("test_core.c", "tyvor_core.c") and "-DTYVOR_BENCH_ONLY=1" not in command:
            raise ValueError("Missing bench-only build guard")
    return dict(tests=count, passed=count, failed=0, skipped=0, names=expected,
                ndebug=config == "Release")

def legacy_inventory():
    return group_inventory("TYVOR_TEST_CASES")

def group_inventory(key):
    text = (ROOT / "CMakeLists.txt").read_text()
    return re.search(r"set\(" + key + r"\s+(.*?)\)", text, re.S).group(1).split()

def build_path(target, config):
    return BUILD_ROOT / ("native-" + config.lower() if target == "native" else target)

def output_paths(target, config):
    build = build_path(target, config)
    if target == "native":
        names = ["test_protocol.exe", "test_reliability.exe", "tyvor_ui_sim.exe",
                 "test_core.exe", "tyvor_core_sim.exe",
                 "test_bench.exe", "test_bench_regressions.exe", "test_timing.exe", "tyvor_bench_ui_export.exe",
                 "compile_commands.json", "CMakeCache.txt"]
        return [build / x for x in names] + [EVIDENCE / ("native-" + config.lower() + ".xml")]
    if target == "stm32":
        return [build / x for x in ("tyvor_stm32.elf", "tyvor_stm32.bin", "tyvor_stm32.map", "CMakeCache.txt")]
    return [build / x for x in ("tyvor_display.elf", "tyvor_display.bin", "tyvor_display.map",
                                "bootloader/bootloader.bin", "partition_table/partition-table.bin",
                                "flasher_args.json", "CMakeCache.txt")] + [ROOT / "esp32/sdkconfig"]

def label(target, config):
    return target + ("-" + config.lower() if target == "native" else "")

def compiler_record(path):
    p = Path(path).resolve()
    return dict(path=str(p), sha256=sha(p),
                version=subprocess.run([str(p), "--version"], capture_output=True,
                                       text=True, check=True).stdout.splitlines()[0])

def begin(target, config, compiler):
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    pending = dict(schema=1, profile=PROFILE, target=target, config=config, run_id=uuid.uuid4().hex,
                   started_ns=time.time_ns(), inputs=inputs(target), compiler=compiler_record(compiler))
    # A failed/restarted build must not leave a previous successful receipt eligible.
    receipt = EVIDENCE / (label(target, config) + "-receipt.json")
    if receipt.exists():
        receipt.unlink()
    (EVIDENCE / (label(target, config) + "-pending.json")).write_text(json.dumps(pending, indent=2))

def finish(target, config):
    pending = json.loads((EVIDENCE / (label(target, config) + "-pending.json")).read_text())
    if pending["inputs"] != inputs(target):
        raise ValueError("Build inputs changed while the build/test was running")
    if pending["compiler"] != compiler_record(pending["compiler"]["path"]):
        raise ValueError("Compiler changed during build")
    outputs = []
    for p in output_paths(target, config):
        if not p.is_file():
            raise ValueError("Missing build output " + str(p))
        outputs.append(dict(path=p.relative_to(ROOT).as_posix(), size=p.stat().st_size, sha256=sha(p)))
    pending["outputs"] = outputs
    pending["finished_ns"] = time.time_ns()
    pending["tests"] = check_native(build_path(target, config), config) if target == "native" else None
    (EVIDENCE / (label(target, config) + "-receipt.json")).write_text(json.dumps(pending, indent=2) + "\n")
    print("Build receipt recorded:", label(target, config))

def verify_receipt(path):
    receipt = json.loads(Path(path).read_text())
    if receipt.get("schema") != 1 or receipt["finished_ns"] < receipt["started_ns"]:
        raise ValueError("Invalid build receipt")
    if receipt.get("profile", "phase1") != PROFILE:
        raise ValueError("Build receipt phase mismatch")
    target, config = receipt["target"], receipt["config"]
    if receipt["inputs"] != inputs(target):
        raise ValueError("Stale build receipt: source/config inputs changed: " + target)
    if receipt["compiler"] != compiler_record(receipt["compiler"]["path"]):
        raise ValueError("Compiler does not match receipt")
    for item in receipt["outputs"]:
        p = ROOT / item["path"]
        if not p.is_file() or p.stat().st_size != item["size"] or sha(p) != item["sha256"]:
            raise ValueError("Stale/tampered build output: " + item["path"])
    expected = {p.relative_to(ROOT).as_posix() for p in output_paths(target, config)}
    if {x["path"] for x in receipt["outputs"]} != expected:
        raise ValueError("Incomplete output inventory")
    if target == "native" and receipt["tests"] != check_native(build_path(target, config), config):
        raise ValueError("Test evidence does not match receipt")
    return receipt

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("action", choices=("begin", "finish", "verify"))
    ap.add_argument("--target", required=True, choices=("native", "stm32", "esp32"))
    ap.add_argument("--config", default="Debug")
    ap.add_argument("--compiler")
    args = ap.parse_args()
    if args.action == "begin":
        begin(args.target, args.config, args.compiler)
    elif args.action == "finish":
        finish(args.target, args.config)
    else:
        verify_receipt(EVIDENCE / (label(args.target, args.config) + "-receipt.json"))
        print("Build receipt verified:", label(args.target, args.config))

if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, KeyError, ET.ParseError, subprocess.CalledProcessError) as exc:
        print("PROVENANCE FAIL:", exc, file=sys.stderr)
        sys.exit(1)
