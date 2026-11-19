"""Offline pre-flash validation only. Never opens a serial port or invokes flash."""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import struct
import sys

FLASH_SIZE = 16 * 1024 * 1024
OFFSETS = {"bootloader": 0, "partition-table": 0x8000, "app": 0x10000}

def safe_file(root, name):
    text = str(name).replace("\\", "/")
    if not text or ":" in text or text.startswith("/") or ".." in text.split("/"):
        raise ValueError("Unsafe firmware path: " + text)
    path = (root / text).resolve()
    if not path.is_relative_to(root.resolve()) or not path.is_file():
        raise ValueError("Missing firmware reference: " + text)
    return path

def image_info(path):
    data = path.read_bytes()
    if len(data) < 24 or data[0] != 0xe9 or not 1 <= data[1] <= 16:
        raise ValueError("Invalid ESP image header: " + path.name)
    if struct.unpack_from("<H", data, 12)[0] != 9 or data[3] >> 4 != 4:
        raise ValueError("Image must target ESP32-S3 / 16 MB")
    if data[2] != 2 or data[3] & 15 != 15:
        raise ValueError("Expected SDK DIO ROM header / 80 MHz")
    if data[23] != 1:
        raise ValueError("Image must contain appended SHA-256")
    pos = 24
    checksum = 0xef
    segments = []
    first = None
    for _ in range(data[1]):
        if pos + 8 > len(data):
            raise ValueError("Truncated segment header")
        address, size = struct.unpack_from("<II", data, pos)
        pos += 8
        if pos + size > len(data):
            raise ValueError("Truncated segment data")
        payload = data[pos:pos + size]
        if first is None:
            first = payload
        for byte in payload:
            checksum ^= byte
        segments.append(dict(address=hex(address), size=size))
        pos += size
    checksum_pos = ((pos + 16) // 16) * 16 - 1
    if checksum_pos + 33 != len(data) or data[checksum_pos] != checksum:
        raise ValueError("ESP image length/checksum mismatch")
    if hashlib.sha256(data[:checksum_pos + 1]).digest() != data[checksum_pos + 1:]:
        raise ValueError("ESP image validation SHA-256 mismatch")
    result = dict(size=len(data), chip="esp32s3", flash_size=FLASH_SIZE,
                  flash_freq_mhz=80, rom_flash_mode="dio", checksum_valid=True,
                  validation_hash_valid=True, segments=segments)
    if first and len(first) >= 176 and struct.unpack_from("<I", first)[0] == 0xabcd5432:
        result["elf_sha256"] = first[144:176].hex()
        result["app_version"] = first[16:48].split(b"\0", 1)[0].decode("ascii")
    return result

def partition_info(path):
    data = path.read_bytes()
    if len(data) != 3072:
        raise ValueError("Unexpected partition table size")
    rows = []
    md5_seen = False
    for pos in range(0, len(data), 32):
        block = data[pos:pos + 32]
        if block == b"\xff" * 32:
            if not md5_seen or data[pos:] != b"\xff" * (len(data) - pos):
                raise ValueError("Bad partition table terminator")
            break
        if block[:16] == b"\xeb\xeb" + b"\xff" * 14:
            if md5_seen or block[16:] != hashlib.md5(data[:pos]).digest():
                raise ValueError("Partition table MD5 mismatch")
            md5_seen = True
            continue
        if md5_seen:
            raise ValueError("Entry after partition MD5")
        magic, kind, subtype, offset, size, label, flags = struct.unpack("<HBBII16sI", block)
        if magic != 0x50aa or not size or offset < 0x9000 or offset + size > FLASH_SIZE:
            raise ValueError("Invalid partition entry/range")
        if kind == 0 and offset % 0x10000:
            raise ValueError("App partition alignment")
        if offset % 0x1000 or size % 0x1000:
            raise ValueError("Partition alignment")
        rows.append(dict(name=label.split(b"\0", 1)[0].decode("ascii"), type=kind,
                         subtype=subtype, offset=offset, size=size, flags=flags))
    if not md5_seen or not rows or len({x["name"] for x in rows}) != len(rows):
        raise ValueError("Missing MD5 or duplicate/empty partition inventory")
    ordered = sorted(rows, key=lambda x: x["offset"])
    if any(a["offset"] + a["size"] > b["offset"] for a, b in zip(ordered, ordered[1:])):
        raise ValueError("Partition overlap")
    return rows

def validate(root):
    root = Path(root).resolve()
    meta = json.loads((root / "flasher_args.json").read_text())
    settings = meta["flash_settings"]
    if settings != dict(flash_mode="dio", flash_size="16MB", flash_freq="80m"):
        raise ValueError("Flash settings mismatch")
    args = meta["write_flash_args"]
    if args != ["--flash_mode", "dio", "--flash_size", "16MB", "--flash_freq", "80m"]:
        raise ValueError("Flash argument settings mismatch")
    if meta["extra_esptool_args"]["chip"] != "esp32s3":
        raise ValueError("Metadata chip mismatch")
    refs = {}
    for offset, name in meta["flash_files"].items():
        address = int(offset, 0)
        if address in refs:
            raise ValueError("Duplicate offset")
        refs[address] = safe_file(root, name)
    if set(refs) != set(OFFSETS.values()) or len(set(refs.values())) != 3:
        raise ValueError("Expected bootloader/partition/app offsets 0/0x8000/0x10000")
    for key, offset in OFFSETS.items():
        if int(meta[key]["offset"], 0) != offset or safe_file(root, meta[key]["file"]) != refs[offset]:
            raise ValueError("Inconsistent firmware reference: " + key)
        if meta[key]["encrypted"] != "false":
            raise ValueError("Encrypted image not supported by this bench package")
    boot, app = image_info(refs[0]), image_info(refs[0x10000])
    if boot["size"] > 0x8000:
        raise ValueError("Bootloader overlaps partition table")
    if refs[0x8000].stat().st_size > 0x8000:
        raise ValueError("Partition table overlaps app")
    rows = partition_info(refs[0x8000])
    factory = [x for x in rows if x["type"] == 0 and x["subtype"] == 0 and x["offset"] == 0x10000]
    if len(factory) != 1 or app["size"] > factory[0]["size"]:
        raise ValueError("App does not fit factory partition")
    elf = safe_file(root, "tyvor_display.elf")
    if app.get("elf_sha256") != hashlib.sha256(elf.read_bytes()).hexdigest():
        raise ValueError("App BIN / ELF identity mismatch")
    csv_rows = []
    with (root / "partitions.csv").open(newline="") as stream:
        for row in csv.reader(x for x in stream if x.strip() and not x.lstrip().startswith("#")):
            name, kind, sub, offset, size = [x.strip() for x in row[:5]]
            csv_rows.append((name, int(offset, 0), int(size, 0)))
    if sorted(csv_rows) != sorted((x["name"], x["offset"], x["size"]) for x in rows):
        raise ValueError("CSV/binary partition mismatch")
    config = (root / "sdkconfig.build").read_text()
    for value in ('CONFIG_IDF_TARGET="esp32s3"', 'CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y',
                  'CONFIG_SPIRAM_MODE_OCT=y', 'CONFIG_SPIRAM_SPEED_40M=y',
                  '# CONFIG_TYVOR_UART_PINOUT_VERIFIED is not set',
                  'CONFIG_TYVOR_UART_TX_GPIO=-1', 'CONFIG_TYVOR_UART_RX_GPIO=-1'):
        if value not in config.splitlines():
            raise ValueError("Unexpected bench build config: " + value)
    return dict(offline=True, serial_opened=False, flashed=False, bootloader=boot, app=app,
                paths={hex(k): v.relative_to(root).as_posix() for k, v in refs.items()},
                partitions=rows, psram_hardware_verified=False)

def verify_manifest(root, manifest_path, workspace=None):
    root, manifest_path = Path(root).resolve(), Path(manifest_path).resolve()
    m = json.loads(manifest_path.read_text())
    if m.get("schema") != 2:
        raise ValueError("Expected portable manifest schema 2")
    package = manifest_path.parent
    if root != package / "esp32":
        raise ValueError("Manifest/package roots do not match")
    listed = set()
    for item in m["files"]:
        p = safe_file(package, item["path"])
        if item["path"] in listed or p.stat().st_size != item["size"] or hashlib.sha256(p.read_bytes()).hexdigest() != item["sha256"]:
            raise ValueError("Manifest artifact mismatch: " + item["path"])
        listed.add(item["path"])
    actual = {p.relative_to(package).as_posix() for p in package.rglob("*")
              if p.is_file() and p != manifest_path}
    if listed != actual:
        raise ValueError("Manifest does not cover the exact artifact inventory")
    if workspace:
        for item in m["sources"]:
            p = safe_file(Path(workspace), item["path"])
            if hashlib.sha256(p.read_bytes()).hexdigest() != item["sha256"]:
                raise ValueError("Source hash mismatch: " + item["path"])
    return len(listed)

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("package", type=Path)
    ap.add_argument("--manifest", type=Path)
    ap.add_argument("--workspace", type=Path)
    ap.add_argument("--report", type=Path)
    args = ap.parse_args()
    result = validate(args.package)
    if args.manifest:
        result["manifest_files_verified"] = verify_manifest(args.package, args.manifest, args.workspace)
    if args.report:
        args.report.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))

if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, KeyError, struct.error, UnicodeError) as exc:
        print("OFFLINE VALIDATION FAIL:", exc, file=sys.stderr)
        sys.exit(1)
