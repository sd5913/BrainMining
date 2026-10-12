"""Deterministic prototype ore budget, independent of the Unreal editor."""
import random

def make_plan(recipe):
    rng = random.Random(recipe['seed'])
    result = []
    for band in recipe['bands']:
        entries, total = [], 0
        while total < band['budget']:
            choices, weights = [], []
            for gem_id, type_weight in zip(band['ids'], band['weights']):
                for value, tier_weight in zip(recipe['values'][gem_id-1], band.get('value_weights',recipe['value_weights'])):
                    if type_weight and total+value <= band['budget']:
                        choices.append((gem_id, value))
                        weights.append(type_weight*tier_weight)
            if not choices:
                break
            gem_id, value = rng.choices(choices, weights=weights, k=1)[0]
            entries.append({'id': gem_id, 'value': value})
            total += value
        # More valuable finds tend toward the end of each phase, with small pockets of surprise.
        entries.sort(key=lambda e: e['value']*rng.uniform(.65,1.35))
        result.append(dict(band=band, entries=entries, total=total))
    return result
