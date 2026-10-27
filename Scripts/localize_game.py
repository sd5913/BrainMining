import argparse
import ast
import csv
import io
import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "SourceAssets/Localization/Game.json"
CULTURES = ("zh-Hans", "en", "zh-Hant")
NAMESPACE = "BrainMining.Game"


def placeholders(text):
    return set(re.findall(r"\{([A-Za-z][A-Za-z0-9_]*|[0-9]+)\}", text))


def validate(entries):
    for key, translations in entries.items():
        if not re.fullmatch(r"[A-Za-z][A-Za-z0-9_.]*", key):
            raise ValueError(f"Invalid stable key: {key}")
        native = translations.get("zh-Hans")
        if not isinstance(native, str) or not native.strip():
            raise ValueError(f"Missing source text: {key}")
        for culture in CULTURES:
            text = translations.get(culture)
            if not isinstance(text, str) or not text.strip():
                raise ValueError(f"Missing {culture} translation: {key}")
            if placeholders(text) != placeholders(native):
                raise ValueError(f"Placeholder mismatch: {key}/{culture}")
            if key == "WenSi.Statement" and (text.count("{Property}") != 1 or text.count("{Noun}") != 1):
                raise ValueError(f"Interactive statement requires one property and noun: {culture}")


def validate_references(entries):
    patterns = (
        r'MiningText::Get\(\s*TEXT\("([^"\n]+)"\)',
        r'FText::FromStringTable\(TEXT\("BrainMining.Game"\),\s*TEXT\("([^"\n]+)"\)',
    )
    for directory in (ROOT / "Source/BrainMining", ROOT / "Plugins/CardRoom/Source"):
        for path in directory.rglob("*"):
            if path.suffix not in (".h", ".cpp", ".inl"):
                continue
            source = path.read_text(encoding="utf-8-sig")
            for pattern in patterns:
                for key in re.findall(pattern, source):
                    if key not in entries:
                        raise ValueError(f"Missing runtime text key {key} referenced by {path}")


def po_text(entries, culture):
    quote = lambda value: json.dumps(value, ensure_ascii=False)
    header = f"Language: {culture}\nContent-Type: text/plain; charset=UTF-8\nContent-Transfer-Encoding: 8bit\n"
    lines = ['msgid ""', f"msgstr {quote(header)}", ""]
    for key, translations in sorted(entries.items()):
        lines.extend([
            f"msgctxt {quote(NAMESPACE + ',' + key)}",
            f"msgid {quote(translations['zh-Hans'])}",
            f"msgstr {quote(translations[culture])}",
            "",
        ])
    return "\n".join(lines) + "\n"


def parse_po(text):
    current = {}
    field = None
    fuzzy = False
    for line in (text + "\n\n").splitlines():
        line = line.strip()
        if not line:
            if current.get("msgctxt") and not fuzzy:
                yield current
            current, field, fuzzy = {}, None, False
        elif line.startswith("#,") and "fuzzy" in line:
            fuzzy = True
        elif line.startswith("#"):
            continue
        elif line.startswith(('msgctxt ', 'msgid ', 'msgstr ')):
            field, value = line.split(" ", 1)
            current[field] = ast.literal_eval(value)
        elif line.startswith('"') and field:
            current[field] += ast.literal_eval(line)
        else:
            raise ValueError(f"Unsupported PO entry: {line}")


def outputs(entries):
    table = io.StringIO(newline="")
    writer = csv.writer(table, lineterminator="\n")
    writer.writerow(["Key", "SourceString", "Comment"])
    for key, translations in sorted(entries.items()):
        writer.writerow([key, translations["zh-Hans"], translations.get("context", key)])
    result = {ROOT / "Content/Localization/Tables/Game.csv": table.getvalue()}
    for culture in CULTURES:
        result[ROOT / f"Content/Localization/Game/{culture}/Game.po"] = po_text(entries, culture)
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--import-po", type=Path)
    parser.add_argument("--culture", choices=CULTURES)
    args = parser.parse_args()
    catalogues = {path: json.loads(path.read_text(encoding="utf-8")) for path in sorted(SOURCE.parent.glob("*.json"))}
    entries, owners = {}, {}
    for path, catalogue in catalogues.items():
        for key, translations in catalogue.items():
            if key in entries:
                raise ValueError(f"Duplicate localization identity: {key}")
            entries[key], owners[key] = translations, path
    changed = set()
    if args.import_po:
        if args.check or not args.culture:
            parser.error("PO import requires --culture and cannot use --check")
        for translated in parse_po(args.import_po.read_text(encoding="utf-8-sig")):
            namespace, key = translated["msgctxt"].split(",", 1)
            if namespace != NAMESPACE or key not in entries:
                raise ValueError(f"Unknown translation identity: {translated['msgctxt']}")
            if translated.get("msgid") != entries[key]["zh-Hans"]:
                raise ValueError(f"Stale source text: {key}")
            if translated.get("msgstr"):
                entries[key][args.culture] = translated["msgstr"]
                changed.add(owners[key])
    validate(entries)
    validate_references(entries)
    generated = outputs(entries)
    for path in changed:
        path.write_text(json.dumps(catalogues[path], ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    for path, content in generated.items():
        if args.check:
            if not path.exists() or path.read_text(encoding="utf-8") != content:
                raise ValueError(f"Generated localization file is stale: {path}")
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content, encoding="utf-8", newline="\n")
    print(f"Validated {len(entries)} entries in {len(CULTURES)} languages.")


if __name__ == "__main__":
    main()
