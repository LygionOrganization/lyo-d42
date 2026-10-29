"""Copy reviewed documentation and verified downloads to a local wiki checkout."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--wiki", type=Path, required=True)
    args = parser.parse_args()
    wiki = args.wiki.resolve()
    if not (wiki / "mkdocs.yml").is_file():
        raise SystemExit("Expected an existing MkDocs wiki checkout")
    release = ROOT / ".pio/releases/0.4.0"
    manifest = json.loads((release / "manifest.json").read_text(encoding="utf-8"))
    for name, entry in manifest["artifacts"].items():
        if Path(name).name != name or hashlib.sha256((release / name).read_bytes()).hexdigest() != entry["sha256"]:
            raise SystemExit(f"Invalid release file: {name}")
    for locale, prefix in (("zh", "docs"), ("en", "docs/en")):
        target = wiki / prefix / "reference/robots/lyo-d42"
        target.mkdir(parents=True, exist_ok=True)
        for p in (ROOT / "docs/lyo-wiki" / locale).glob("*.md"):
            shutil.copy2(p, target / p.name)
        assets = target / "assets"
        assets.mkdir(exist_ok=True)
        for name in (*manifest["artifacts"], "manifest.json", "SHA256SUMS.txt"):
            shutil.copy2(release / name, assets / name)
    print("Updated local Lyo-D42 pages and verified downloads; no deployment performed.")


if __name__ == "__main__":
    main()
