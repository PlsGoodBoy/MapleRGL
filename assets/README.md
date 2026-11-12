# Prototype art assets

These are actual MapleStory pixel resources selected from the public, independently extracted corpus https://github.com/Javipen/maplestory-cosmetic-playground . The exact source revision is in manifest.json. Artwork belongs to Nexon and other respective holders. No commercial/public redistribution permission is asserted. See UPSTREAM-ASSET-NOTICE.md. Keep prototype packages private until rights are established.

Reimport with `python tools/import_assets.py /path/to/maplestory-cosmetic-playground` (Python + Pillow). This reads the extraction corpus without editing it. It does not read or alter original WZ files. Frame files are relative to this directory; duration is milliseconds, origin is the sprite anchor measured from its top-left. Source alpha interpolation endpoints and original WZ property paths are retained when available. Character frames are composited using source neck/navel/brow anchors. Neutral hand placement uses handMove->hand only when needed, matching the upstream preview rule.

The player hit animation currently uses the alert pose as a deliberate fallback. Death is the actual ghost pose. Ground is original side-on grass edge, not a seamless top-down ground texture; terrain.background is a sky swatch. Do not represent either as finished top-down terrain. No original sound assets have been imported. Missing audio may use clearly identified synthesized effects. This corpus does not supply the required monsters; those are imported separately from the user's local files.


Local monster sprites were extracted read-only from existing Mob.nx. IDs: slime 0210100, mushroom 0120100, blue mushroom 2220100, pig 1210100, wild-boar elite 2230102, Mushmom 6130101. tools/extract_local_mobs.py reproduces output without modifying source files.
