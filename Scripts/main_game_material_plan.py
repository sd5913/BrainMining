"""Deterministic weighted mineral counts, independent of currency and Unreal."""
import math
import random


def mineral_ids(area, count, seed):
    ids, weights = area['ids'], area['weights']
    if not count > 0 or len(ids) != len(weights) or len(set(ids)) != len(ids):
        raise ValueError('Invalid deposit count or mineral IDs')
    if any(i not in (2, 4, 6, 9) for i in ids):
        raise ValueError('Only copper, iron, malachite and diamond are supported')
    if any(not math.isfinite(w) or w <= 0 for w in weights):
        raise ValueError('Weights must be positive and finite')
    exact = [count * w / sum(weights) for w in weights]
    counts = [math.floor(n) for n in exact]
    for i in sorted(range(len(ids)), key=lambda i: (-(exact[i] - counts[i]), i))[:count-sum(counts)]:
        counts[i] += 1
    result = [mineral for mineral, amount in zip(ids, counts) for _ in range(amount)]
    random.Random(str(seed) + area['tag']).shuffle(result)
    return result
