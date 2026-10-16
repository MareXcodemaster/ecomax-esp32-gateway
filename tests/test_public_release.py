import base64
import gzip
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent.parent

def module(name):
    spec = importlib.util.spec_from_file_location(name, ROOT / 'tools' / (name + '.py'))
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result

class PublicRelease(unittest.TestCase):
    def test_wifi_validation(self):
        validate = module('setup').validate_wifi
        for ssid, password in [('Example', 'abcdefgh'), ('Open network', ''), ('x' * 32, 'y' * 63)]:
            validate(ssid, password)
        for ssid, password in [('', ''), ('x' * 33, ''), ('Example', 'short'), ('Example', 'x' * 64), ('x\n', 'abcdefgh'), ('x', 'abcdefgh\0')]:
            with self.assertRaises(ValueError):
                validate(ssid, password)

    def test_usb_ignores_logs_and_unrelated_replies(self):
        class Port:
            def __init__(self):
                self.lines = iter([b'boot log\n', b'[USB] invalid\n', b'[USB] {"command":"scan"}\n', b'[USB] {"command":"setup","username":"admin"}\n'])
                self.writes = []
            def write(self, data): self.writes.append(data)
            def flush(self): pass
            def readline(self): return next(self.lines)
        port = Port()
        result = module('setup').exchange(port, {'command': 'setup'}, 'setup')
        self.assertEqual(result['username'], 'admin')
        self.assertEqual(len(port.writes), 1)
        self.assertEqual(json.loads(port.writes[0]), {'command': 'setup'})

    def test_synthetic_fixtures_are_reproducible(self):
        generator = module('generate_fixtures')
        with tempfile.TemporaryDirectory() as temp:
            destination = Path(temp)
            generator.generate(ROOT / 'profiles/ecomax860p_lz_touch.json', destination)
            before = {p.name: p.read_bytes() for p in destination.iterdir()}
            generator.generate(ROOT / 'profiles/ecomax860p_lz_touch.json', destination)
            self.assertEqual(before, {p.name: p.read_bytes() for p in destination.iterdir()})
            self.assertIn(b'Synthetic', before['protocol_fixtures.hpp'])
            self.assertNotIn(b'captured', before['sensor_fixtures.hpp'])
        frame = generator.frame(8, b'example')
        checksum = 0
        for value in frame[:-2]: checksum ^= value
        self.assertEqual(frame[-2], checksum)
        self.assertEqual(int.from_bytes(frame[1:3], 'little'), len(frame))

    def test_audit_sees_compressed_and_hex_secrets(self):
        audit = module('audit_release')
        secret = b'synthetic-privacy-sentinel'
        self.assertIn(secret, list(audit.expansions(b'prefix' + gzip.compress(secret) + b'suffix')))
        array = b'uint8_t example[] = {' + b','.join(b'0x%02x' % v for v in secret) + b'};'
        self.assertIn(secret, list(audit.expansions(array)))

    def test_catalog_has_no_personal_defaults(self):
        catalog = json.loads((ROOT / 'src/panel_catalog.json').read_text())
        self.assertNotIn('uid', catalog['sysParams'])
        self.assertEqual(catalog['regParams']['curr'], {})
        self.assertTrue(all(row.get('value') is None for row in catalog['rmParamsData']['data']))
        source = (ROOT / 'embed_web.py').read_text()
        self.assertNotIn('os.environ', source)
        self.assertFalse((ROOT / 'src/sensor_fixtures.hpp').exists())

if __name__ == '__main__':
    unittest.main()
