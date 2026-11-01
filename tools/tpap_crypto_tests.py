"""Compile TPAP against the pinned SDK's mbedTLS, without ESP or real devices."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--zig', type=Path)
    parser.add_argument('--probe-output', type=Path, help='Optional read-only bridge executable outside the public repository')
    args = parser.parse_args()
    cc = [str(args.zig.resolve()), 'cc'] if args.zig else [shutil.which('cc')]
    cxx = [str(args.zig.resolve()), 'c++'] if args.zig else [shutil.which('c++')]
    if not cc[0] or not cxx[0]:
        raise SystemExit('Native C and C++ compilers required')
    core = Path(os.environ.get('PLATFORMIO_CORE_DIR', Path.home() / '.platformio'))
    mbed = core / 'packages/framework-espidf/components/mbedtls/mbedtls'
    cjson = core / 'packages/framework-espidf/components/json/cJSON'
    names = ['aes', 'base64', 'ccm', 'cipher', 'cipher_wrap', 'constant_time', 'bignum',
             'bignum_core', 'bignum_mod', 'bignum_mod_raw', 'ecp', 'ecp_curves',
             'ecp_curves_new', 'platform', 'platform_util', 'md', 'sha256', 'sha1']
    flags = ['-O1', '-g', '-I' + str(mbed / 'include'), '-I' + str(mbed / 'library'),
             '-I' + str(ROOT / 'tests/host'), '-DMBEDTLS_CONFIG_FILE="mbedtls_tpap_config.h"']
    flags += ['-I' + str(cjson)]
    if os.name != 'nt':
        flags += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    with tempfile.TemporaryDirectory() as temp:
        directory = Path(temp)
        objects = []
        for name in names:
            source = mbed / ('library/' + name + '.c')
            if not source.exists():
                continue
            output = directory / (name + '.o')
            subprocess.run([*cc, *flags, '-c', str(source), '-o', str(output)], check=True)
            objects.append(str(output))
        binary = directory / ('tpap-tests.exe' if os.name == 'nt' else 'tpap-tests')
        sources = [str(ROOT / ('src/' + name)) for name in ['tpap_crypto.cpp', 'smart_tapo.cpp',
                   'third_party/esp_tapo/tapo_client.cpp', 'third_party/esp_tapo/tapo_cipher.cpp']]
        def link(main_source, output):
            subprocess.run([*cxx, *flags, '-std=c++17', '-UNDEBUG', '-I' + str(ROOT / 'src'),
                            *sources, str(ROOT / main_source), '-x', 'c++', str(cjson / 'cJSON.c'),
                            '-x', 'none', *objects, '-o', str(output)], check=True)
        link('tests/host/tpap_crypto.cpp', binary)
        subprocess.run([str(binary)], check=True)
        if args.probe_output:
            output = args.probe_output.resolve()
            if output.is_relative_to(ROOT):
                raise ValueError('Probe executable must stay outside the public repository')
            link('tests/host/tpap_probe.cpp', output)

if __name__ == '__main__':
    main()
