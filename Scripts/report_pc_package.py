import argparse
import collections
import csv
import hashlib
import json
import struct
import zipfile
from pathlib import Path


def read_chunks(path):
    with path.open(encoding="utf-8-sig", newline="") as stream:
        rows = list(csv.DictReader(stream, skipinitialspace=True))
    if not rows or not {"ChunkId", "ContainerName", "Filename", "Offset", "Size"}.issubset(rows[0]):
        raise ValueError("Missing IoStore chunk inventory or required columns")
    unique = {}
    for row in rows:
        key = (row["ContainerName"].casefold(), row["ChunkId"].casefold())
        if key in unique and unique[key] != row:
            raise ValueError(f"Conflicting chunk inventory: {key}")
        unique[key] = row
    return list(unique.values())


def read_blocks(path):
    data = path.read_bytes()
    if len(data) < 144 or data[:16] != b"-==--==--==--==-" or not 3 <= data[16] <= 8:
        raise ValueError(f"Unsupported or corrupt UE IoStore header: {path}")
    header_size, entry_count, block_count, entry_size = struct.unpack_from("<4I", data, 20)
    block_size = struct.unpack_from("<I", data, 44)[0]
    seeds = struct.unpack_from("<I", data, 84)[0] if data[16] >= 4 else 0
    overflow = struct.unpack_from("<I", data, 96)[0] if data[16] >= 5 else 0
    start = header_size + entry_count * 22 + (seeds + overflow) * 4
    if header_size < 144 or entry_size != 12 or not block_size or start + block_count * 12 > len(data):
        raise ValueError(f"Invalid IoStore block table: {path}")
    blocks = []
    for index in range(block_count):
        entry = data[start + index * 12:start + (index + 1) * 12]
        compressed = int.from_bytes(entry[5:8], "little")
        uncompressed = int.from_bytes(entry[8:11], "little")
        if not compressed or not 0 < uncompressed <= block_size:
            raise ValueError(f"Invalid compression block {index}: {path}")
        blocks.append((compressed, uncompressed))
    return block_size, blocks


def asset_name(filename):
    for suffix in (".m.ubulk", ".uasset", ".umap", ".ubulk", ".uptnl", ".uexp"):
        if filename.casefold().endswith(suffix):
            return filename[:-len(suffix)]
    return filename or "[unmapped]"


def attribute_blocks(rows, block_size, blocks):
    weights = collections.defaultdict(collections.Counter)
    for row in rows:
        offset, size = int(row["Offset"]), int(row["Size"])
        if offset < 0 or size < 0 or offset + size > len(blocks) * block_size:
            raise ValueError("Chunk lies outside the compression block table")
        if not size:
            continue
        filename = asset_name(row["Filename"])
        for index in range(offset // block_size, (offset + size - 1) // block_size + 1):
            overlap = min(offset + size, index * block_size + blocks[index][1]) - max(offset, index * block_size)
            if overlap > 0:
                weights[index][filename] += overlap
    sizes = collections.Counter()
    for index, (compressed, _) in enumerate(blocks):
        owners = weights.get(index)
        if not owners:
            sizes["[unmapped]"] += compressed
            continue
        total = sum(owners.values())
        allocation = {name: compressed * weight // total for name, weight in owners.items()}
        remainder = compressed - sum(allocation.values())
        order = sorted(owners, key=lambda name: (-(compressed * owners[name] % total), name))
        for name in order[:remainder]:
            allocation[name] += 1
        sizes.update(allocation)
    return sizes


def folder_name(filename):
    for prefix, category in (("../../../BrainMining/Content/", "Game/"), ("../../../Engine/Content/", "Engine/")):
        if filename.startswith(prefix):
            return category + filename[len(prefix):].split("/", 1)[0]
    if "/Plugins/" in filename and "/Content/" in filename:
        return "Plugin/" + filename.split("/Content/", 1)[0].rsplit("/", 1)[-1]
    return "Other"


def sha256_file(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def create_verified_zip(archive, output, files):
    if output.exists() or output.resolve().is_relative_to(archive.resolve()):
        raise ValueError("ZIP must be new and outside the Windows directory")
    inventory = []
    with zipfile.ZipFile(output, "x", compression=zipfile.ZIP_DEFLATED, compresslevel=6, allowZip64=True) as package:
        for path in files:
            name = "Windows/" + path.relative_to(archive).as_posix()
            digest = sha256_file(path)
            package.write(path, name)
            inventory.append({"path": name, "bytes": path.stat().st_size, "sha256": digest})
    with zipfile.ZipFile(output) as package:
        if set(package.namelist()) != {entry["path"] for entry in inventory}:
            raise ValueError("ZIP file inventory mismatch")
        for entry in inventory:
            with package.open(entry["path"]) as stream:
                if hashlib.file_digest(stream, "sha256").hexdigest() != entry["sha256"]:
                    raise ValueError(f"ZIP content mismatch: {entry['path']}")
    return inventory


def build_report(archive, report, output, baseline=None):
    if not (archive / "WOoOW.exe").is_file():
        raise ValueError("Windows launcher missing")
    if not (archive / "BrainMining/Binaries/Win64/WOoOW-Win64-Shipping.exe").is_file():
        raise ValueError("WOoOW Shipping executable missing")
    if list(archive.rglob("BrainMining*.exe")):
        raise ValueError("Obsolete game executable included in distribution")
    request = json.loads((report / "request.json").read_text(encoding="utf-8-sig"))
    if request.get("plan_only"):
        raise ValueError("A packaging plan is not a completed build")
    rows = read_chunks(report / "container-files.csv")
    filenames = {row["Filename"].casefold() for row in rows}
    required = json.loads((report / "required-container-assets.json").read_text(encoding="utf-8-sig"))
    if isinstance(required, str):
        required = [required]
    if not required:
        raise ValueError("Empty runtime asset checklist")
    missing = [name for name in required if ("../../../BrainMining/Content/" + name).casefold() not in filenames]
    if missing:
        raise ValueError(f"Missing runtime assets: {missing}")
    toc_files = sorted(archive.rglob("*.utoc"))
    if not toc_files or len({path.stem.casefold() for path in toc_files}) != len(toc_files):
        raise ValueError("Missing or ambiguous IoStore containers")
    if {row["ContainerName"].casefold() for row in rows} - {path.stem.casefold() for path in toc_files}:
        raise ValueError("Inventory references a container absent from this archive")
    assets = collections.Counter()
    block_bytes = 0
    for path in toc_files:
        block_size, blocks = read_blocks(path)
        assets.update(attribute_blocks([row for row in rows if row["ContainerName"].casefold() == path.stem.casefold()], block_size, blocks))
        block_bytes += sum(size for size, _ in blocks)
    if sum(assets.values()) != block_bytes:
        raise ValueError("Compression block attribution does not reconcile")
    files = sorted(path for path in archive.rglob("*") if path.is_file())
    forbidden = [str(path.relative_to(archive)) for path in files if path.suffix.casefold() in (".pdb", ".obj", ".lib", ".exp")]
    if forbidden:
        raise ValueError(f"Unexpected development files in distribution: {forbidden}")
    if request.get("custom_config") == "PCSize" and any("/hdribackdrop/" in name for name in filenames):
        raise ValueError("Excluded HDRIBackdrop content is still packaged")
    inventory = create_verified_zip(archive, output, files)
    folders = collections.Counter()
    for name, size in assets.items():
        folders[folder_name(name)] += size
    result = {
        "package_integrity_passed": True,
        "gameplay_review": "pending",
        "archive": str(archive.resolve()),
        "zip": str(output.resolve()),
        "zip_bytes": output.stat().st_size,
        "zip_sha256": sha256_file(output),
        "installed_bytes": sum(entry["bytes"] for entry in inventory),
        "container_bytes": sum(path.stat().st_size for path in files if path.suffix in (".pak", ".utoc", ".ucas")),
        "unique_compressed_block_bytes": block_bytes,
        "attribution": "Shared blocks apportioned by logical byte overlap; totals count every block once. File totals also include metadata and padding.",
        "required_asset_count": len(required),
        "folders": dict(folders.most_common()),
        "assets": dict(assets.most_common()),
        "files": inventory,
    }
    if baseline:
        previous = json.loads(baseline.read_text(encoding="utf-8-sig"))
        if not previous.get("package_integrity_passed"):
            raise ValueError("Baseline has not passed package integrity checks")
        result["comparison"] = {
            "baseline": str(baseline.resolve()),
            "zip_saved_bytes": previous["zip_bytes"] - result["zip_bytes"],
            "zip_reduction_percent": 100 * (1 - result["zip_bytes"] / previous["zip_bytes"]),
            "installed_saved_bytes": previous["installed_bytes"] - result["installed_bytes"],
            "container_saved_bytes": previous["container_bytes"] - result["container_bytes"],
            "removed_assets": sorted(set(previous["assets"]) - set(result["assets"])),
        }
    (report / "result.json").write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    (report / "package.sha256.txt").write_text(f"{result['zip_sha256']}  {output.name}\n", encoding="utf-8")
    with (report / "asset-sizes.csv").open("w", encoding="utf-8-sig", newline="") as stream:
        writer = csv.writer(stream)
        writer.writerow(["asset", "attributed_compressed_bytes"])
        writer.writerows(assets.most_common())
    print(json.dumps({key: value for key, value in result.items() if key not in ("files", "assets", "folders")}, ensure_ascii=False, indent=2))
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--zip", type=Path, required=True)
    parser.add_argument("--baseline", type=Path)
    options = parser.parse_args()
    build_report(options.archive, options.report, options.zip, options.baseline)


if __name__ == "__main__":
    main()
