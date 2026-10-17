import hashlib
from pathlib import Path
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parent.parent
ASSETS = ROOT / 'src/panel_web/assets'


class RestoredAssets(unittest.TestCase):
    def test_exact_original_files(self):
        expected = {
            'schema.svg': '1beade9af7122528d56a1655956c452f8059859137bcd5f96635d74702860984',
            'devicebg.jpg': '6cbd8f18cb21c998b63a45050f314292f46944c66882da1a970c03230f0be279',
        }
        for name, digest in expected.items():
            self.assertEqual(hashlib.sha256((ASSETS / name).read_bytes()).hexdigest(), digest)

    def test_schema_is_inert_and_contains_no_live_values(self):
        root = ET.parse(ASSETS / 'schema.svg').getroot()
        self.assertEqual(root.get('viewBox'), '0 0 841.66 437.104')
        readings = []
        for node in root.iter():
            tag = node.tag.rsplit('}', 1)[-1]
            self.assertNotIn(tag, ('script', 'foreignObject', 'image'))
            for name, value in node.attrib.items():
                attr = name.rsplit('}', 1)[-1]
                self.assertFalse(attr.lower().startswith('on'))
                if attr == 'href':
                    self.assertTrue(value.startswith('#'))
            id_ = node.get('id', '')
            if id_.startswith('lbSchema_') and not id_.endswith('Label'):
                readings.append(id_)
                self.assertEqual(''.join(node.itertext()).strip(), '---')
        self.assertGreaterEqual(len(readings), 17)


if __name__ == '__main__':
    unittest.main()
