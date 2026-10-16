import concurrent.futures
import datetime
import hashlib
import html
import json
import pathlib
import re
import urllib.request
import zipfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'SourceAssets/RoomAudio'
PACKS = [
    ('interface', 'Kenney', 'https://kenney.nl/assets/interface-sounds'),
    ('casino', 'Kenney', 'https://kenney.nl/assets/casino-audio'),
    ('impact', 'Kenney', 'https://kenney.nl/assets/impact-sounds'),
    ('scifi', 'Kenney', 'https://kenney.nl/assets/sci-fi-sounds'),
    ('loops', 'rubberduck', 'https://opengameart.org/content/30-cc0-sfx-loops'),
    ('water', 'rubberduck', 'https://opengameart.org/content/40-cc0-water-splash-slime-sfx'),
]


def fetch(pack):
    identifier, author, page_url = pack
    request = urllib.request.Request(page_url, headers={'User-Agent': 'Mozilla/5.0'})
    page = urllib.request.urlopen(request, timeout=40).read()
    text = page.decode('utf-8')
    assert 'CC0' in text, page_url
    candidates = [html.unescape(link) for link in re.findall(r'href=[\"\x27]([^\"\x27]+)', text) if link.lower().endswith('.zip')]
    assert len(candidates) == 1, (page_url, candidates)
    url = candidates[0]
    raw = urllib.request.urlopen(url, timeout=90).read()
    destination = OUTPUT / 'Downloads' / (identifier + '.zip')
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(raw)
    evidence = OUTPUT / 'Licenses' / (identifier + '.html')
    evidence.parent.mkdir(parents=True, exist_ok=True)
    evidence.write_bytes(page)
    with zipfile.ZipFile(destination) as archive:
        members = archive.namelist()
        assert archive.testzip() is None
        for member in members:
            if pathlib.PurePosixPath(member).name.lower() in ('license.txt', 'readme.txt'):
                (evidence.parent / (identifier + '-' + pathlib.PurePosixPath(member).name)).write_bytes(archive.read(member))
    title = html.unescape(re.search(r'<title>(.*?)</title>', text, re.S).group(1)).strip()
    record = dict(id=identifier, title=title, author=author, page_url=page_url, download_url=url,
                  license='CC0-1.0', license_url='https://creativecommons.org/publicdomain/zero/1.0/',
                  archive=str(destination.relative_to(OUTPUT)).replace('\\', '/'),
                  evidence=str(evidence.relative_to(OUTPUT)).replace('\\', '/'),
                  sha256=hashlib.sha256(raw).hexdigest(), bytes=len(raw),
                  fetched_at=datetime.datetime.now(datetime.timezone.utc).isoformat(), members=members)
    print(identifier, len(raw), 'bytes', len(members), 'members')
    return record


if __name__ == '__main__':
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as executor:
        records = list(executor.map(fetch, PACKS))
    (OUTPUT / 'sources.json').write_text(json.dumps(records, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
