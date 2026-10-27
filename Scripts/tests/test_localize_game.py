import importlib.util
import unittest
from pathlib import Path

spec = importlib.util.spec_from_file_location("localize_game", Path(__file__).resolve().parents[1] / "localize_game.py")
localize = importlib.util.module_from_spec(spec)
spec.loader.exec_module(localize)


class LocalizationTests(unittest.TestCase):
    def test_po_round_trip_keeps_identity_and_unicode(self):
        entries = {"UI.Test": {"zh-Hans": '测试\n"{Count}"', "en": 'Test\n"{Count}"', "zh-Hant": '測試\n"{Count}"'}}
        localize.validate(entries)
        parsed = list(localize.parse_po(localize.po_text(entries, "en")))
        self.assertEqual(parsed[0]["msgctxt"], "BrainMining.Game,UI.Test")
        self.assertEqual(parsed[0]["msgstr"], 'Test\n"{Count}"')

    def test_missing_placeholder_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "Placeholder mismatch"):
            localize.validate({"UI.Test": {"zh-Hans": "{Count}个", "en": "Items", "zh-Hant": "{Count}個"}})

    def test_positional_placeholder_is_checked(self):
        with self.assertRaisesRegex(ValueError, "Placeholder mismatch"):
            localize.validate({"UI.Test": {"zh-Hans": "{0}个", "en": "Items", "zh-Hant": "{0}個"}})

    def test_interactive_property_must_not_be_duplicated(self):
        with self.assertRaisesRegex(ValueError, "one property and noun"):
            localize.validate({"WenSi.Statement": {"zh-Hans": "{Property}的{Noun}", "en": "{Noun} is {Property}{Property}", "zh-Hant": "{Property}的{Noun}"}})

    def test_interactive_sentence_can_reorder_tokens(self):
        localize.validate({"WenSi.Statement": {"zh-Hans": "{Property}的{Noun}", "en": "This {Noun} is {Property}.", "zh-Hant": "{Property}的{Noun}"}})

    def test_missing_translation_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "Missing en"):
            localize.validate({"UI.Test": {"zh-Hans": "测试", "zh-Hant": "測試"}})

    def test_fuzzy_translation_is_not_imported(self):
        self.assertEqual(list(localize.parse_po('#, fuzzy\nmsgctxt "BrainMining.Game,UI.Test"\nmsgid "测试"\nmsgstr "Wrong"\n')), [])


if __name__ == "__main__":
    unittest.main()
