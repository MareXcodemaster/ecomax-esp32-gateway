"""Generate a C++ asset from the single editable HTML source during the build."""
from pathlib import Path
import json
import gzip
import runpy
Import("env")

# Append structured arguments: PlatformIO's text flag parser splits Windows paths at spaces.
for path, replacement in [(Path(env.subst("$PROJECT_DIR")).resolve(), "/PROJECT"),
                          (Path(env.PioPlatform().get_package_dir("framework-espidf")).resolve(), "/IDF")]:
    for prefix in sorted({str(path), path.as_posix()}):
        env.Append(CCFLAGS=["-ffile-prefix-map=" + prefix + "=" + replacement])

source = Path(env.subst("$PROJECT_DIR")) / "src" / "web" / "index.html"
folder = Path(env.subst("$BUILD_DIR")) / "generated"
folder.mkdir(parents=True, exist_ok=True)
html = source.read_text(encoding="utf-8")
assert ')LAZARHTML"' not in html
content = '#pragma once\nstatic const char web_page[] = R"LAZARHTML(' + html + ')LAZARHTML";\n'
compressed = gzip.compress(html.encode("utf-8"), compresslevel=9, mtime=0)
content += 'static const unsigned char web_gzip[] = {' + ','.join(str(value) for value in compressed) + '};\n'
target = folder / "web_asset.hpp"
if not target.exists() or target.read_text(encoding="utf-8") != content:
    target.write_text(content, encoding="utf-8")
env.Append(CPPPATH=[str(folder)])
catalog = (source.parent.parent / "panel_catalog.json").read_text(encoding="ascii")
assert ')CATALOG"' not in catalog
content = '#pragma once\nstatic const char panel_catalog_json[] = R"CATALOG(' + catalog + ')CATALOG";\n'
target = folder / "panel_catalog.hpp"
if not target.exists() or target.read_text(encoding="ascii") != content:
    target.write_text(content, encoding="ascii")

import mimetypes
assets = []
for index, path in enumerate(sorted((source.parent.parent / "panel_web").rglob("*"))):
    if not path.is_file():
        continue
    relative = path.relative_to(source.parent.parent / "panel_web").as_posix()
    mime = {".js": "text/javascript; charset=utf-8", ".css": "text/css; charset=utf-8",
            ".html": "text/html; charset=utf-8", ".woff2": "font/woff2", ".svg": "image/svg+xml",
            ".ico": "image/x-icon", ".png": "image/png"}.get(path.suffix, mimetypes.guess_type(path.name)[0] or "application/octet-stream")
    payload = path.read_bytes()
    compressed = path.suffix in (".html", ".js", ".css", ".svg")
    if compressed:
        payload = gzip.compress(payload, compresslevel=9, mtime=0)
    name = "panel_asset_" + str(index)
    assets.append(("/" if relative == "index.html" else "/" + relative, mime, name, compressed, payload))
content = '#pragma once\nstruct PanelAsset { const char *path, *mime; const unsigned char *bytes; size_t length; bool gzip; };\n'
for _, _, name, _, payload in assets:
    content += 'static const unsigned char ' + name + '[] = {' + ','.join(str(v) for v in payload) + '};\n'
content += 'static const PanelAsset panel_assets[] = {\n'
for route, mime, name, compressed, _ in assets:
    content += '{' + json.dumps(route) + ',' + json.dumps(mime) + ',' + name + ',sizeof(' + name + '),' + ('true' if compressed else 'false') + '},\n'
content += '};\n'
target = folder / 'panel_assets.hpp'
if not target.exists() or target.read_text(encoding='utf-8') != content:
    target.write_text(content, encoding='utf-8')

values = {"host": "", "user": "", "password": ""}
content = '#pragma once\nnamespace econet_provision {\n' + ''.join('constexpr const char ' + key + '[] = ' + json.dumps(value, ensure_ascii=True) + ';\n' for key, value in values.items()) + '}\n'
target = folder / 'econet_defaults.hpp'
if not target.exists() or target.read_text(encoding='utf-8') != content:
    target.write_text(content, encoding='utf-8')

# Public builds never import a developer's provisioning environment.
ssid = ""
password = ""
content = ('#pragma once\nnamespace provision {\n'
           'constexpr const char ssid[] = ' + json.dumps(ssid, ensure_ascii=True) + ';\n'
           'constexpr const char password[] = ' + json.dumps(password, ensure_ascii=True) + ';\n'
           'constexpr bool fixed = sizeof(ssid) > 1;\n}\n')
target = folder / "network_defaults.hpp"
if not target.exists() or target.read_text(encoding="utf-8") != content:
    target.write_text(content, encoding="utf-8")

project = Path(env.subst("$PROJECT_DIR"))
generator = runpy.run_path(str(project / "tools/generate_fixtures.py"))
generator["generate"](project / "profiles/ecomax860p_lz_touch.json", folder)

