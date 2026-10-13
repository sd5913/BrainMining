"""Deterministic prototype ore budget, independent of the Unreal editor."""
import random
import math


def validate_recipe(recipe):
    """Reject invalid recipes before any editor actors are changed."""
    def require(condition, message):
        if not condition:
            raise ValueError(message)
    def positive_int(value):
        return isinstance(value, int) and not isinstance(value, bool) and value > 0
    def weights_valid(weights, size):
        return (isinstance(weights, list) and len(weights) == size
                and all(isinstance(x, (int, float)) and math.isfinite(x) and x >= 0 for x in weights)
                and sum(weights) > 0)
    require(isinstance(recipe.get('seed'), int), 'seed must be an integer')
    values = recipe.get('values', [])
    require(len(values) == 9, 'values must contain the 9 mineral types in ID order')
    for tiers in values:
        require(len(tiers) == 4 and all(positive_int(v) for v in tiers)
                and tiers == sorted(set(tiers)), 'Each mineral needs 4 strictly increasing positive integer values')
    require(weights_valid(recipe.get('value_weights'), 4), 'Invalid value_weights')
    require(bool(recipe.get('bands')), 'At least one band is required')
    for index, band in enumerate(recipe['bands']):
        prefix = 'Band %d: ' % index
        require(isinstance(band.get('area'), str) and bool(band['area']), prefix + 'area is required')
        require(isinstance(band.get('phase'), str), prefix + 'phase is required')
        limits = band.get('range', [])
        require(len(limits) == 2 and 0 <= limits[0] < limits[1] <= 1, prefix + 'range must be within [0, 1]')
        ids = band.get('ids', [])
        require(bool(ids) and all(positive_int(i) and i <= 9 for i in ids) and len(set(ids)) == len(ids), prefix + 'invalid IDs')
        require(weights_valid(band.get('weights'), len(ids)), prefix + 'invalid type weights')
        tier_weights = band.get('value_weights', recipe['value_weights'])
        require(weights_valid(tier_weights, 4), prefix + 'invalid tier weights')
        require(positive_int(band.get('budget')), prefix + 'budget must be a positive integer')
        spacing = band.get('spacing', 0)
        require(isinstance(spacing, (int, float)) and math.isfinite(spacing) and spacing > 0, prefix + 'spacing must be positive cm')
        affordable = any(w > 0 and tw > 0 and value <= band['budget']
                         for i, w in zip(ids, band['weights'])
                         for value, tw in zip(values[i-1], tier_weights))
        require(affordable, prefix + 'budget cannot buy any enabled mineral tier')
    return recipe

def make_plan(recipe):
    validate_recipe(recipe)
    rng = random.Random(recipe['seed'])
    result = []
    for band in recipe['bands']:
        entries, total = [], 0
        while total < band['budget']:
            choices, weights = [], []
            for gem_id, type_weight in zip(band['ids'], band['weights']):
                for value, tier_weight in zip(recipe['values'][gem_id-1], band.get('value_weights',recipe['value_weights'])):
                    if type_weight and tier_weight and total+value <= band['budget']:
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
