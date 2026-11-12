"""Zip the current portable release and write its downloadable checksum."""
import hashlib
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED

root = Path(__file__).resolve().parents[1]
release = root / 'release'
version = (release / 'VERSION.txt').read_text(encoding='utf-8-sig').splitlines()[0].rsplit(' - ', 1)[1]
destination = root / 'dist'
destination.mkdir(exist_ok=True)
package = destination / f'MapleRGL-v{version}-windows-x64.zip'
files = [release / name for name in (
    'MapleRGL.exe', 'msvcp140.dll', 'vcruntime140.dll', 'vcruntime140_1.dll',
    'README.txt', 'THIRD_PARTY_NOTICES.txt', 'VERSION.txt')]
files += sorted(path for path in (release / 'assets').rglob('*') if path.is_file())
with ZipFile(package, 'w', compression=ZIP_DEFLATED, compresslevel=6) as archive:
    for path in files:
        archive.write(path, 'MapleRGL/' + path.relative_to(release).as_posix())
digest = hashlib.sha256(package.read_bytes()).hexdigest()
(destination / 'SHA256SUMS.txt').write_text(f'{digest}  {package.name}\n', encoding='ascii')
print(f'Package: {package} ({package.stat().st_size} bytes; {len(files)} files)')
print(f'SHA256: {digest}')
