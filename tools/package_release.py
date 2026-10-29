"""Package a locally built Lyo-D42 release candidate; never flash or publish."""
from pathlib import Path
import hashlib
import json
import zipfile

ROOT = Path(__file__).resolve().parents[1]
VERSION = "0.4.0"
OUT = ROOT / ".pio/releases" / VERSION
SKIP = {".git", ".pio", ".vscode", "__pycache__", "node_modules", "target", "build", "dist", ".venv"}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def sources(folder):
    return sorted(p for p in folder.rglob("*") if p.is_file()
                  and not any(part in SKIP or part.endswith(".egg-info") for part in p.relative_to(folder).parts)
                  and p.suffix not in {".pyc", ".pyo"})


def archive(kind, files, base, prefix, extra=None):
    path = OUT / f"Lyo-D42-{VERSION}-{kind}.zip"
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as z:
        for p in sorted(files):
            z.write(p, prefix + p.relative_to(base).as_posix())
        for name, data in (extra or {}).items():
            z.writestr(prefix + name, data)
    with zipfile.ZipFile(path) as z:
        assert z.testzip() is None
        assert not any(SKIP.intersection(Path(n).parts) for n in z.namelist())
    return {"bytes": path.stat().st_size, "sha256": digest(path)}


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    build = ROOT / ".pio/build/esp32-s3-devkitc-1-n8r8"
    inputs = [ROOT / "platformio.ini", ROOT / "tools/pio_python_path.py"]
    for folder in ("src", "include", "lib"):
        inputs.extend(sources(ROOT / folder))
    app = build / "firmware.bin"
    if not app.exists() or any(p.stat().st_mtime > app.stat().st_mtime for p in inputs):
        raise SystemExit("Run pio run successfully after the latest firmware changes before packaging.")
    if not (ROOT / "sdk/rust/Cargo.lock").exists():
        raise SystemExit("Validate the Rust SDK and generate Cargo.lock before packaging.")
    offsets = {"bootloader.bin": "0x0", "partitions.bin": "0x8000", "firmware.bin": "0x10000"}
    manifest = {"product": "Lyo-D42", "version": VERSION, "release_status": "candidate",
                "hardware_validated": False, "board": "ESP32-S3 8MB flash / 8MB OPI PSRAM",
                "firmware": {name: {"offset": offset, "bytes": (build / name).stat().st_size,
                                    "sha256": digest(build / name)} for name, offset in offsets.items()},
                "build_inputs": {p.relative_to(ROOT).as_posix(): digest(p) for p in sorted(inputs)}}
    artifacts = {}
    fw_readme = """Lyo-D42 0.4.0 release candidate (not hardware qualified)
ESP32-S3, 8 MB flash. Use these offsets in the ESP32-S3 flash tool:
bootloader.bin 0x0
partitions.bin 0x8000
firmware.bin 0x10000
Never write firmware.bin at 0x0. Disconnect motor power during recovery.
Erasing flash removes saved network settings, chassis settings and missions.
After recovery query state first, then test low-speed operation with wheels raised.
See the matching source package docs/RELEASE_VALIDATION.md and docs/UPSTREAM.md.
Licenses: project GPLv3 (LICENSE), inherited RoboCore AGPLv3 (LICENSE-AGPL).
Retain individual library notices from the matching source distribution.
"""
    artifacts["firmware"] = archive("firmware", [build / n for n in offsets], build, "",
        {"manifest.json": json.dumps(manifest, indent=2), "README.txt": fw_readme,
         "LICENSE": (ROOT / "LICENSE").read_text(encoding="utf-8"),
         "LICENSE-AGPL": (ROOT / "LICENSE-AGPL").read_text(encoding="utf-8")})
    for lang in ("python", "rust"):
        base = ROOT / "sdk" / lang
        artifacts[lang + "-sdk"] = archive(lang + "-sdk", sources(base), base, f"Lyo-D42-{lang}-sdk/")
    paths = [ROOT / p for p in ("README.md", "README.zh-CN.md", "AGENTS.md", "LICENSE", "LICENSE-AGPL", "platformio.ini", ".gitignore")]
    for folder in ("src", "include", "lib", "web", "sdk", "test", "tools", "docs", "python_example", "arduino_example"):
        paths.extend(sources(ROOT / folder))
    artifacts["source"] = archive("source", paths, ROOT, "Lyo-D42/")
    manifest["artifacts"] = {f"Lyo-D42-{VERSION}-{k}.zip": v for k, v in artifacts.items()}
    (OUT / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    sums = [f"{v['sha256']}  {name}" for name, v in manifest["artifacts"].items()]
    sums.append(f"{digest(OUT / 'manifest.json')}  manifest.json")
    (OUT / "SHA256SUMS.txt").write_text("\n".join(sums) + "\n", encoding="ascii")
    print(f"Validated {len(paths)} source files and four archives: {OUT}")


if __name__ == "__main__":
    main()
