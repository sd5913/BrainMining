"""Pure-Python recipe regressions; no Unreal process required."""
import copy
import json
from pathlib import Path
import unittest
from campaign_mineral_plan import make_plan


class MineralPlanTests(unittest.TestCase):
    def setUp(self):
        self.recipe = json.loads((Path(__file__).resolve().parents[1]/'Config/CampaignMinerals.json').read_text(encoding='utf-8'))

    def test_fixed_recipe_is_repeatable_and_within_budget(self):
        a = make_plan(self.recipe)
        self.assertEqual(a, make_plan(copy.deepcopy(self.recipe)))
        for phase in a:
            self.assertEqual(sum(e['value'] for e in phase['entries']), phase['total'])
            self.assertLessEqual(phase['total'], phase['band']['budget'])

    def test_zero_value_rejected_instead_of_infinite_generation(self):
        self.recipe['values'][0][0] = 0
        with self.assertRaises(ValueError): make_plan(self.recipe)

    def test_disabled_affordable_tiers_do_not_crash_or_generate(self):
        self.recipe['bands'] = [self.recipe['bands'][0]]
        band = self.recipe['bands'][0]
        band.update(ids=[1], weights=[1], budget=5, value_weights=[0, 0, 0, 1])
        self.assertEqual(make_plan(self.recipe)[0]['entries'], [{'id': 1, 'value': 5}])
        band['budget'] = 4
        with self.assertRaises(ValueError): make_plan(self.recipe)

    def test_invalid_range_and_spacing(self):
        for update in ({'range': [.8, .2]}, {'spacing': 0}, {'weights': [0, 0, 0]}, {'ids': [10]}):
            recipe = copy.deepcopy(self.recipe)
            recipe['bands'][0].update(update)
            with self.assertRaises(ValueError): make_plan(recipe)


if __name__ == '__main__':
    unittest.main()
