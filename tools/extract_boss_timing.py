"""Read original Boss attack release times; never modify source NX archives."""
import argparse
import json
from pathlib import Path
from extract_local_mobs import NX
from extract_regions import resolve, properties
from extract_expansion_assets import export_sequence

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('archives', type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
report = json.loads((root / 'docs/assets/battle_art_sources.json').read_text(encoding='utf8'))
manifest = json.loads((root / 'assets/manifest.json').read_text(encoding='utf8'))
nx = NX(args.archives / 'Mob.nx')
rows, releases, actions = [], [], []
for key, entry in report['enemy_attacks'].items():
    if not key.startswith('boss'):
        continue
    known = {}
    for action in range(1, 4):
        source = f"{entry['source_mob']}/attack{action}/info"
        try:
            known[action] = properties(nx, resolve(nx, source)).get('attackAfter', 0) / 1000
        except KeyError:
            pass
        sprite = f'{key}.attack{action}'
        body = f"{entry['source_mob']}/attack{action}"
        if action in known and sprite not in manifest['sprites']:
            children = nx.children(resolve(nx, body))
            if any(name.isdigit() and nx.node(node)[3] == 5 for name, node in children.items()):
                manifest['sprites'][sprite] = export_sequence(nx, root / 'assets', sprite, 'Mob', body, False, skip_zero_delay=True)
    selected = [n if f'{key}.attack{n}' in manifest['sprites'] else 1 if f'{key}.attack1' in manifest['sprites'] else 0 for n in (1, 2, 3)]
    times = []
    for n in selected:
        source = manifest['sprites'].get(f'{key}.attack{n}', {}).get('source', '')
        try:
            info = properties(nx, resolve(nx, source.removeprefix('Mob.nx/') + '/info'))
            times.append(info.get('attackAfter', info.get('skillAfter', 0)) / 1000)
        except KeyError:
            times.append(0)
    rows.append({'actor': key, 'source': f"Mob.nx/{entry['source_mob']}", 'action_by_move': selected,
                 'source_release_seconds': times, 'original_info': known,
                 'projectile': entry['projectile'], 'impact': entry['impact'], 'correspondence': entry['correspondence'],
                 'timing': 'Original release pose is aligned with the authored warning; released attacks use combat time.'})
    actions.append(' {' + ','.join(map(str, selected)) + '},')
    releases.append(' {' + ','.join(f'{t:.3f}f' for t in times) + '},')
assert len(rows) == 24
(root / 'src/boss_art_timing.hpp').write_text('#pragma once\nnamespace maple {\ninline constexpr int boss_art_actions[24][3]={\n' + '\n'.join(actions) + '\n};\ninline constexpr float boss_art_releases[24][3]={\n' + '\n'.join(releases) + '\n};\n}\n', encoding='utf8')
(root / 'docs/assets/boss_combat_sources.json').write_text(json.dumps(rows, ensure_ascii=False, indent=2) + '\n', encoding='utf8')
(root / 'assets/manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n', encoding='utf8')
print(f'Exported original release metadata for {len(rows)} Bosses.')
