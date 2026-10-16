import json
import unittest
from collections import Counter
from pathlib import Path
from main_game_material_plan import mineral_ids


class MaterialPlanTests(unittest.TestCase):
    def test_progression_supply_and_rarity(self):
        recipe = json.loads((Path(__file__).resolve().parents[1] / 'Config/MainGameMinerals.json').read_text(encoding='utf-8'))
        supply = Counter()
        for area, count in zip(recipe['areas'], [62, 55, 22]):
            ids = mineral_ids(area, count, recipe['seed'])
            self.assertEqual(ids, mineral_ids(area, count, recipe['seed']))
            self.assertEqual(len(ids), count)
            quantities = Counter(ids)
            self.assertEqual(set(quantities), set(area['ids']))
            self.assertEqual(list(quantities[i] for i in area['ids']), sorted(quantities.values(), reverse=True))
            supply.update(quantities)
        for kind, needed in {2:28, 4:20, 6:6, 9:2}.items():
            self.assertGreaterEqual(supply[kind], needed)
        first = Counter(mineral_ids(recipe['areas'][0], 62, recipe['seed']))
        self.assertGreaterEqual(first[2], 10)
        self.assertGreaterEqual(first[4], 6)

    def test_invalid_types_and_weights(self):
        for ids, weights in [([1], [1]), ([2,2], [1,1]), ([2], [0]), ([2], [float('nan')])]:
            with self.assertRaises(ValueError):
                mineral_ids({'tag':'test', 'ids':ids, 'weights':weights}, 10, 1)


if __name__ == '__main__':
    unittest.main()
