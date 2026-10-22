"""Local opt-in Shipping test transport. Requires -MiningQA and an isolated -UserDir.

Checkpoint placement and mineral fixtures are explicitly named; they are not proof
of an uninterrupted manual playthrough. Frame samples are game-loop intervals,
not GPU timings or monitor presentation times.
"""
import argparse
import json
import pathlib
import time
import uuid


class Client:
    def __init__(self, user_dir):
        self.directory = pathlib.Path(user_dir).resolve() / 'Saved' / 'PackagedQA'
        self.directory.mkdir(parents=True, exist_ok=True)

    def command(self, op, timeout=60, **fields):
        request = dict(id=uuid.uuid4().hex, op=op, **fields)
        temporary = self.directory / 'command.tmp'
        temporary.write_text(json.dumps(request, ensure_ascii=False), encoding='utf-8')
        temporary.replace(self.directory / 'command.json')
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            try:
                result = json.loads((self.directory / 'response.json').read_text(encoding='utf-8-sig'))
                if result['id'] == request['id']:
                    with (self.directory / 'transcript.jsonl').open('a', encoding='utf-8') as stream:
                        stream.write(json.dumps(dict(request=request, response=result), ensure_ascii=False) + '\n')
                    return result
            except (FileNotFoundError, json.JSONDecodeError, PermissionError):
                pass
            time.sleep(.1)
        raise TimeoutError(request)

    def key(self, key):
        self.command('key', key=key, down=True)
        return self.command('key', key=key, down=False)

    def sample(self, label, seconds=20):
        self.command('sample_start', label=label)
        time.sleep(seconds)
        data = self.command('sample_end')
        values = sorted(data.pop('frame_ms'))
        data['metrics'] = dict(frames=len(values), mean_ms=sum(values)/len(values),
            fps=1000*len(values)/sum(values), p95_ms=values[int((len(values)-1)*.95)],
            p99_ms=values[int((len(values)-1)*.99)], maximum_ms=max(values),
            over_50ms=sum(v > 50 for v in values))
        (self.directory / (label + '.json')).write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding='utf-8')
        return data


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('user_dir')
    parser.add_argument('request', help='JSON object containing op and arguments')
    args = parser.parse_args()
    print(json.dumps(Client(args.user_dir).command(**json.loads(args.request)), ensure_ascii=False))
