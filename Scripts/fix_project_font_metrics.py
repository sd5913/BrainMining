"""Remove Source Han Sans's half-em leading from the embedded UI font faces.

Outlines and horizontal metrics are preserved. The supplied source OTF files
remain unchanged. This can run in an existing editor without a code reload.
"""
import json
import struct
from pathlib import Path
import unreal

assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world(), 'Stop PIE first'
root = Path(unreal.Paths.project_dir())
out = root / 'Saved/Validation/HudFontLayout'
out.mkdir(parents=True, exist_ok=True)

def normalize(source):
    data = bytearray(source)
    count = struct.unpack_from('>H', data, 4)[0]
    tables = {}
    for index in range(count):
        entry = 12 + index * 16
        tag, checksum, offset, length = struct.unpack_from('>4sIII', data, entry)
        assert offset + length <= len(data)
        tables[tag] = (entry, offset, length)
    leading = {}
    for tag, relative in ((b'hhea', 8), (b'OS/2', 72)):
        offset = tables[tag][1] + relative
        leading[tag.decode()] = struct.unpack_from('>h', data, offset)[0]
        struct.pack_into('>h', data, offset, 0)
    head = tables[b'head'][1]
    struct.pack_into('>I', data, head + 8, 0)
    def checksum(blob):
        blob = bytes(blob) + b'\0' * (-len(blob) % 4)
        return sum(struct.unpack('>' + 'I' * (len(blob) // 4), blob)) & 0xffffffff
    for entry, offset, length in tables.values():
        struct.pack_into('>I', data, entry + 4, checksum(data[offset:offset + length]))
    struct.pack_into('>I', data, head + 8, (0xB1B0AFBA - checksum(data)) & 0xffffffff)
    assert checksum(data) == 0xB1B0AFBA
    for tag in (b'CFF ', b'hmtx', b'kern', b'GPOS'):
        if tag in tables:
            _, offset, length = tables[tag]
            assert source[offset:offset + length] == data[offset:offset + length], tag
    return data, leading

report = []
for weight in ('Bold', 'Medium'):
    source = root / 'SourceAssets/Fonts' / ('SourceHanSansCN-' + weight + '.otf')
    data, leading = normalize(source.read_bytes())
    name = 'FF_SourceHanSansCN_' + weight
    temp = out / (name + '.otf')
    temp.write_bytes(data)
    factory = unreal.FontFileImportFactory()
    choice = type(factory.get_editor_property('batch_create_font_asset'))
    factory.set_editor_property('batch_create_font_asset', choice.NO)
    task = unreal.AssetImportTask()
    task.filename = str(temp)
    task.destination_path = '/Game/Fonts'
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.replace_existing_settings = False
    task.factory = factory
    task.save = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    face = unreal.load_asset('/Game/Fonts/' + name)
    assert face and task.imported_object_paths, name
    face.set_editor_property('loading_policy', unreal.FontLoadingPolicy.INLINE)
    face.set_editor_property('layout_method', unreal.FontLayoutMethod.METRICS)
    face.set_editor_property('source_filename', str(source))
    assert unreal.EditorAssetLibrary.save_loaded_asset(face)
    report.append(dict(weight=weight, previous_leading=leading, new_leading=0,
                       outlines_unchanged=True, horizontal_metrics_unchanged=True))
(out / 'metrics_fixed.json').write_text(json.dumps(report, indent=2), encoding='utf8')
unreal.log('PROJECT_FONT_METRICS_FIXED line_gap=0 outlines_and_advances_unchanged')
