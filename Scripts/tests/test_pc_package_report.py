import csv
import importlib.util
import json
import struct
import tempfile
import unittest
import zipfile
from pathlib import Path


spec = importlib.util.spec_from_file_location("pc_report", Path(__file__).resolve().parents[1] / "report_pc_package.py")
pc_report = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pc_report)


def write_toc(path, compressed=7):
    header = bytearray(144)
    header[:16] = b"-==--==--==--==-"
    header[16] = 8
    struct.pack_into("<4I", header, 20, 144, 1, 1, 12)
    struct.pack_into("<I", header, 44, 65536)
    block = bytes(5) + compressed.to_bytes(3, "little") + (10).to_bytes(3, "little") + bytes([1])
    path.write_bytes(header + bytes(22) + block)


class PackageReportTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    def test_shared_compression_block_is_counted_once(self):
        rows = [{"Filename": "A.uasset", "Offset": "0", "Size": "5"}, {"Filename": "B.uasset", "Offset": "5", "Size": "5"}]
        sizes = pc_report.attribute_blocks(rows, 10, [(101, 10)])
        self.assertEqual(dict(sizes), {"A": 51, "B": 50})
        self.assertEqual(sum(sizes.values()), 101)

    def test_spanning_chunks_and_unmapped_blocks(self):
        rows = [{"Filename": "A.uasset", "Offset": "5", "Size": "10"}, {"Filename": "B.ubulk", "Offset": "15", "Size": "5"}]
        sizes = pc_report.attribute_blocks(rows, 10, [(8, 10), (10, 10), (4, 10)])
        self.assertEqual(dict(sizes), {"A": 13, "B": 5, "[unmapped]": 4})

    def test_out_of_bounds_chunks_fail(self):
        with self.assertRaises(ValueError):
            pc_report.attribute_blocks([{"Filename": "A", "Offset": "9", "Size": "2"}], 10, [(5, 10)])

    def test_header_and_truncated_toc(self):
        path = self.root / "Game.utoc"
        write_toc(path)
        self.assertEqual(pc_report.read_blocks(path), (65536, [(7, 10)]))
        path.write_bytes(path.read_bytes()[:-1])
        with self.assertRaises(ValueError):
            pc_report.read_blocks(path)

    def test_duplicate_inventory_is_not_double_counted(self):
        path = self.root / "chunks.csv"
        path.write_text("ChunkId, ContainerName, Filename, Offset, Size\na, Game, A.uasset, 0, 10\na, Game, A.uasset, 0, 10\n", encoding="utf-8")
        self.assertEqual(len(pc_report.read_chunks(path)), 1)
        with path.open("a", encoding="utf-8") as stream:
            stream.write("a, Game, B.uasset, 0, 10\n")
        with self.assertRaises(ValueError):
            pc_report.read_chunks(path)

    def test_zip_roundtrip_and_existing_output_protection(self):
        archive = self.root / "Windows"
        archive.mkdir()
        binary = archive / "BrainMining.exe"
        binary.write_bytes(b"test payload" * 100)
        output = self.root / "game.zip"
        result = pc_report.create_verified_zip(archive, output, [binary])
        self.assertEqual(result[0]["bytes"], 1200)
        with zipfile.ZipFile(output) as package:
            self.assertEqual(package.read("Windows/BrainMining.exe"), binary.read_bytes())
        with self.assertRaises(ValueError):
            pc_report.create_verified_zip(archive, output, [binary])
        with self.assertRaises(ValueError):
            pc_report.create_verified_zip(archive, archive / "nested.zip", [binary])

    def make_package(self):
        archive = self.root / "Windows"
        archive.mkdir()
        (archive / "BrainMining.exe").write_bytes(b"launcher")
        write_toc(archive / "Game.utoc")
        (archive / "Game.ucas").write_bytes(bytes(16))
        report = self.root / "report"
        report.mkdir()
        (report / "request.json").write_text(json.dumps({"plan_only": False, "custom_config": "PCSize"}), encoding="utf-8")
        (report / "required-container-assets.json").write_text('["Maps/Main.umap"]', encoding="utf-8")
        with (report / "container-files.csv").open("w", newline="", encoding="utf-8") as stream:
            writer = csv.writer(stream)
            writer.writerow(["ChunkId", "ContainerName", "Filename", "Offset", "Size"])
            writer.writerow(["a", "Game", "../../../BrainMining/Content/Maps/Main.umap", 0, 10])
        return archive, report

    def test_complete_report_keeps_gameplay_review_pending(self):
        archive, report = self.make_package()
        result = pc_report.build_report(archive, report, self.root / "package.zip")
        self.assertTrue(result["package_integrity_passed"])
        self.assertEqual(result["gameplay_review"], "pending")
        self.assertEqual(result["unique_compressed_block_bytes"], 7)
        self.assertEqual(sum(result["folders"].values()), 7)
        self.assertEqual(result["installed_bytes"], sum(path.stat().st_size for path in archive.iterdir()))

    def test_missing_dynamic_asset_blocks_delivery(self):
        archive, report = self.make_package()
        (report / "required-container-assets.json").write_text('["Materials/NativeOnly.uasset"]', encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "Missing runtime assets"):
            pc_report.build_report(archive, report, self.root / "package.zip")
        self.assertFalse((report / "result.json").exists())

    def test_plan_cannot_be_reported_as_a_build(self):
        archive, report = self.make_package()
        (report / "request.json").write_text('{"plan_only":true}', encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "plan"):
            pc_report.build_report(archive, report, self.root / "package.zip")

    def test_debug_symbols_block_delivery(self):
        archive, report = self.make_package()
        (archive / "Game.pdb").write_bytes(b"symbols")
        with self.assertRaisesRegex(ValueError, "development files"):
            pc_report.build_report(archive, report, self.root / "package.zip")


if __name__ == "__main__":
    unittest.main()
