"""Negative/offline regression checks on disposable copies, never serial devices."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import shutil
import struct
import tempfile
import unittest
import xml.etree.ElementTree as ET
from build_provenance import ROOT, EVIDENCE, check_junit, inventory, verify_receipt, PROFILE
from validate_esp_package import validate, verify_manifest
PACKAGE = ROOT / "artifacts" / PROFILE


class PackageValidationTests(unittest.TestCase):
    def setUp(self):
        base = ROOT / "build/reliability-tests"
        base.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=base)
        self.root = Path(self.temp.name).resolve()
        # The automatic recursive cleanup below is restricted to this checked workspace directory.
        assert self.root.is_relative_to(base.resolve()) and self.root != base.resolve()
        self.esp = self.root / "esp32"
        shutil.copytree(PACKAGE / "esp32", self.esp)

    def tearDown(self):
        self.temp.cleanup()

    def metadata(self):
        return json.loads((self.esp / "flasher_args.json").read_text())

    def save_metadata(self, data):
        (self.esp / "flasher_args.json").write_text(json.dumps(data))

    def test_portable_package_valid(self):
        result = validate(self.esp)
        self.assertTrue(result["offline"])
        self.assertFalse(result["serial_opened"])
        self.assertEqual(result["paths"]["0x0"], "bootloader/bootloader.bin")
        self.assertEqual(result["paths"]["0x8000"], "partition_table/partition-table.bin")

    def test_missing_firmware_reference(self):
        data = self.metadata()
        data["flash_files"]["0x0"] = "does-not-exist.bin"
        self.save_metadata(data)
        with self.assertRaisesRegex(ValueError, "Missing firmware"):
            validate(self.esp)

    def test_parent_path_rejected(self):
        data = self.metadata()
        data["flash_files"]["0x0"] = "../bootloader.bin"
        self.save_metadata(data)
        with self.assertRaisesRegex(ValueError, "Unsafe firmware"):
            validate(self.esp)

    def test_wrong_app_offset(self):
        data = self.metadata()
        data["flash_files"]["0x20000"] = data["flash_files"].pop("0x10000")
        data["app"]["offset"] = "0x20000"
        self.save_metadata(data)
        with self.assertRaisesRegex(ValueError, "offsets"):
            validate(self.esp)

    def test_secondary_reference_mismatch(self):
        data = self.metadata()
        data["bootloader"]["file"] = "tyvor_display.bin"
        self.save_metadata(data)
        with self.assertRaisesRegex(ValueError, "Inconsistent firmware"):
            validate(self.esp)

    def test_wrong_flash_size(self):
        data = self.metadata()
        data["flash_settings"]["flash_size"] = "8MB"
        self.save_metadata(data)
        with self.assertRaisesRegex(ValueError, "Flash settings"):
            validate(self.esp)

    def test_bad_chip_id(self):
        path = self.esp / "tyvor_display.bin"
        data = bytearray(path.read_bytes())
        struct.pack_into("<H", data, 12, 0)
        path.write_bytes(data)
        with self.assertRaisesRegex(ValueError, "ESP32-S3"):
            validate(self.esp)

    def test_image_checksum_rejected(self):
        path = self.esp / "tyvor_display.bin"
        data = bytearray(path.read_bytes())
        data[40] ^= 1
        path.write_bytes(data)
        with self.assertRaisesRegex(ValueError, "checksum"):
            validate(self.esp)

    def test_image_sha_rejected(self):
        path = self.esp / "tyvor_display.bin"
        data = bytearray(path.read_bytes())
        data[-1] ^= 1
        path.write_bytes(data)
        with self.assertRaisesRegex(ValueError, "SHA-256"):
            validate(self.esp)

    def test_elf_bin_identity_rejected(self):
        with (self.esp / "tyvor_display.elf").open("ab") as stream:
            stream.write(b"tampered")
        with self.assertRaisesRegex(ValueError, "ELF identity"):
            validate(self.esp)

    def test_partition_md5_rejected(self):
        path = self.esp / "partition_table/partition-table.bin"
        data = bytearray(path.read_bytes())
        data[16] ^= 1
        path.write_bytes(data)
        with self.assertRaisesRegex(ValueError, "MD5"):
            validate(self.esp)

    def test_partition_overlap_with_valid_md5(self):
        path = self.esp / "partition_table/partition-table.bin"
        data = bytearray(path.read_bytes())
        struct.pack_into("<I", data, 32 + 4, 0xe000)
        md5_pos = next(i for i in range(0, len(data), 32) if data[i:i+2] == b"\xeb\xeb")
        data[md5_pos+16:md5_pos+32] = hashlib.md5(data[:md5_pos]).digest()
        path.write_bytes(data)
        with self.assertRaisesRegex(ValueError, "overlap"):
            validate(self.esp)

    def test_partition_csv_mismatch(self):
        path = self.esp / "partitions.csv"
        path.write_text(path.read_text().replace("0xC00000", "0xB00000"))
        with self.assertRaisesRegex(ValueError, "CSV/binary"):
            validate(self.esp)

    def test_pin_gate_not_allowed(self):
        path = self.esp / "sdkconfig.build"
        path.write_text(path.read_text().replace("# CONFIG_TYVOR_UART_PINOUT_VERIFIED is not set",
                                               "CONFIG_TYVOR_UART_PINOUT_VERIFIED=y"))
        with self.assertRaisesRegex(ValueError, "bench build config"):
            validate(self.esp)

    def test_manifest_corruption(self):
        package = self.root / "portable"
        shutil.copytree(PACKAGE, package)
        manifest = package / "SHA256_MANIFEST.json"
        result = verify_manifest(package / "esp32", manifest)
        self.assertGreater(result, 0)
        data = json.loads(manifest.read_text())
        data["files"][0]["sha256"] = "0" * 64
        manifest.write_text(json.dumps(data))
        with self.assertRaisesRegex(ValueError, "Manifest artifact mismatch"):
            verify_manifest(package / "esp32", manifest)

    def test_actual_build_receipts(self):
        for name in ("native-debug", "native-release", "stm32", "esp32"):
            verify_receipt(EVIDENCE / (name + "-receipt.json"))

    def test_stale_source_receipt_rejected(self):
        receipt = json.loads((EVIDENCE / "native-release-receipt.json").read_text())
        receipt["inputs"][0]["sha256"] = "0" * 64
        path = self.root / "stale-receipt.json"
        path.write_text(json.dumps(receipt))
        with self.assertRaisesRegex(ValueError, "Stale build receipt"):
            verify_receipt(path)

    def test_stale_output_receipt_rejected(self):
        receipt = json.loads((EVIDENCE / "native-release-receipt.json").read_text())
        receipt["outputs"][0]["sha256"] = "0" * 64
        path = self.root / "stale-output.json"
        path.write_text(json.dumps(receipt))
        with self.assertRaisesRegex(ValueError, "Stale/tampered build output"):
            verify_receipt(path)

    def test_empty_junit_rejected(self):
        path = self.root / "empty.xml"
        path.write_text('<testsuite tests="0" failures="0"/>')
        with self.assertRaisesRegex(ValueError, "inventory/count"):
            check_junit(path, inventory())

    def test_skipped_junit_rejected(self):
        source = EVIDENCE / "native-release.xml"
        suite = ET.parse(source)
        suite.getroot().set("skipped", "1")
        path = self.root / "skipped.xml"
        suite.write(path)
        with self.assertRaisesRegex(ValueError, "skipped"):
            check_junit(path, inventory())

    def test_wrong_inventory_rejected(self):
        suite = ET.parse(EVIDENCE / "native-release.xml")
        suite.getroot().find("testcase").set("name", "not-the-declared-test")
        path = self.root / "inventory.xml"
        suite.write(path)
        with self.assertRaisesRegex(ValueError, "inventory/count"):
            check_junit(path, inventory())


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--report", type=Path)
    args = ap.parse_args()
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(PackageValidationTests)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    if args.report:
        args.report.write_text(json.dumps(dict(tests=result.testsRun, failures=len(result.failures),
                                              errors=len(result.errors), skipped=len(result.skipped),
                                              offline=True, serial_opened=False), indent=2) + "\n")
    raise SystemExit(0 if result.wasSuccessful() else 1)
