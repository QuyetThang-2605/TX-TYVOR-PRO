"""Validate all renderer exports without a panel or browser/device connection."""
import argparse
import hashlib
import json
from pathlib import Path
import xml.etree.ElementTree as ET
from build_provenance import ROOT, BUILD_ROOT, EVIDENCE

def verify_ui(folder):
    folder = Path(folder)
    states = {"valid": "VALID", "stale": "STALE", "disconnected": "DISCONNECTED",
              "safe": "SAFE", "invalid-model": "INVALID MODEL"}
    titles = ("HOME", "MODEL", "INPUTS", "MIXES", "OUTPUTS", "TELEMETRY", "SETTINGS")
    expected = {f"{state}-{page}.svg" for state in states for page in range(7)}
    if {p.name for p in folder.glob("*.svg")} != expected:
        raise ValueError("Expected exact 35-view inventory")
    rows = []
    for state, label in states.items():
        for page, title in enumerate(titles):
            path = folder / f"{state}-{page}.svg"
            root = ET.parse(path).getroot()
            if (root.get("width"), root.get("height")) != ("368", "448"):
                raise ValueError("Incorrect canvas: " + path.name)
            text = [x.text or "" for x in root.iter() if x.tag.endswith("text")]
            if len(text) != 9 or text[0] != title or label not in text[1] or "NO RF" not in text[-1]:
                raise ValueError("Wrong page/state: " + path.name)
            if state in ("stale", "disconnected") and any("8100" in t or "99%" in t or "RSSI VALID" in t for t in text):
                raise ValueError("Expired telemetry rendered as active: " + path.name)
            rows.append(dict(path=path.name, sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
    return rows

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("folder", type=Path)
    args = parser.parse_args()
    rows = verify_ui(args.folder)
    executable = BUILD_ROOT / "native-release/tyvor_bench_ui_export.exe"
    result = dict(views=len(rows), pages=7, states=5, hardware_tested=False, files=rows,
                  exporter_sha256=hashlib.sha256(executable.read_bytes()).hexdigest())
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    (EVIDENCE / "ui-validation.json").write_text(json.dumps(result, indent=2) + "\n")
    print("UI VALIDATION PASS: 35 views / 7 pages / 5 states; no expired readings shown as live")

if __name__ == "__main__":
    main()
