"""Run real decoders on synthetic fixtures with a host C++ compiler (required in CI)."""
import os
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile
from generate_fixtures import generate

ROOT = Path(__file__).resolve().parent.parent

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--zig', type=Path, help='Optional official Zig executable for native Windows tests')
    args = parser.parse_args()
    compiler = [str(args.zig.resolve()), 'c++'] if args.zig else [shutil.which(os.environ.get('CXX', 'g++')) or shutil.which('clang++')]
    if not compiler[0]:
        raise SystemExit('Host C++ compiler required; this is not an ESP32 toolchain test.')
    core = Path(os.environ.get('PLATFORMIO_CORE_DIR', Path.home() / '.platformio'))
    cjson = core / 'packages/framework-espidf/components/json/cJSON'
    if not (cjson / 'cJSON.c').exists():
        raise SystemExit('Install the pinned PlatformIO ESP-IDF dependencies first (pio pkg install).')
    with tempfile.TemporaryDirectory() as temp:
        generated = Path(temp)
        generate(ROOT / 'profiles/ecomax860p_lz_touch.json', generated)
        executable = generated / ('host-tests.exe' if os.name == 'nt' else 'host-tests')
        command = [*compiler, '-std=c++17', '-O1', '-g', '-UNDEBUG',
                   '-I' + str(ROOT / 'src'), '-I' + str(ROOT / 'tests/host'),
                   '-I' + str(cjson), '-I' + str(generated), str(ROOT / 'src/protocol.cpp'),
                   str(ROOT / 'src/device_access.cpp'), str(ROOT / 'tests/host/access.cpp'),
                   str(ROOT / 'tests/host/diagnostics.cpp'),
                   str(ROOT / 'tests/host/diagnostic_storage.cpp'),
                   str(ROOT / 'tests/host/panel_sync.cpp'),
                   str(ROOT / 'tests/host/compatibility.cpp'),
                   str(ROOT / 'tests/host/remote_menu_language.cpp'),
                   str(ROOT / 'tests/host/controller_profile.cpp'),
                   str(ROOT / 'tests/host/menu_catalog.cpp'),
                   str(ROOT / 'tests/host/settings.cpp'),
                   str(ROOT / 'tests/host/circulation.cpp'),
                   str(ROOT / 'tests/host/tailnet.cpp'),
                   str(ROOT / 'tests/host/boiler_clock.cpp'),
                   str(ROOT / 'tests/host/schedule_run.cpp'),
                   str(ROOT / 'tests/host/main.cpp'), '-x', 'c++', str(cjson / 'cJSON.c'), '-o', str(executable)]
        if os.name != 'nt':
            command += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        subprocess.run(command, check=True)
        subprocess.run([str(executable)], check=True)
        sampling = generated / ('sampling-tests.exe' if os.name == 'nt' else 'sampling-tests')
        subprocess.run([*compiler, '-std=c++17', '-O1', '-UNDEBUG', '-I'+str(ROOT/'src'),
                        '-I'+str(ROOT/'tests/host'), '-I'+str(cjson), str(ROOT/'src/sampling.cpp'),
                        str(ROOT/'tests/host/sampling.cpp'), '-x', 'c++', str(cjson/'cJSON.c'), '-o', str(sampling)], check=True)
        subprocess.run([str(sampling)], check=True)
        schedule_worker = generated / ('schedule-worker.exe' if os.name == 'nt' else 'schedule-worker')
        subprocess.run([*compiler, '-std=c++17', '-O1', '-UNDEBUG', '-I'+str(ROOT/'src'),
                        '-I'+str(cjson), str(ROOT/'tests/host/schedule_run_worker.cpp'),
                        '-x','c++',str(cjson/'cJSON.c'),'-o',str(schedule_worker)], check=True)
        subprocess.run([str(schedule_worker)], check=True)
        # The browser suite must exercise the real C++ encoder, not a JS copy.
        encoder = ROOT / 'test-results' / ('panel-sync-encoder.exe' if os.name == 'nt' else 'panel-sync-encoder')
        encoder.parent.mkdir(exist_ok=True)
        subprocess.run([*compiler, '-std=c++17', '-O1', '-I'+str(ROOT/'src'),
                        '-I'+str(cjson), str(ROOT/'tests/host/panel_sync_encoder.cpp'),
                        '-x','c++',str(cjson/'cJSON.c'),'-o',str(encoder)], check=True)

if __name__ == '__main__':
    main()
