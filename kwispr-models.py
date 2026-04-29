#!/usr/bin/env python3
"""Manage Kwispr local STT catalog models."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import sys
import tarfile
import tempfile
import urllib.request
from pathlib import Path
from typing import Any

DEFAULT_CATALOG = Path(__file__).resolve().parent / "models" / "local-stt-catalog.json"
DEFAULT_MODEL_DIR = Path(os.environ.get("KWISPR_MODEL_DIR", "~/.local/share/kwispr/models")).expanduser()
MANIFEST = ".kwispr-model.json"


def load_catalog(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def model_map(catalog: dict[str, Any]) -> dict[str, dict[str, Any]]:
    return {m["id"]: m for m in catalog.get("models", [])}


def model_path(root: Path, model: dict[str, Any]) -> Path:
    artifact = model["artifact"]
    return root / model["id"] if artifact.get("is_directory") else root / artifact["filename"]


def manifest_path(root: Path, model: dict[str, Any]) -> Path:
    target = model_path(root, model)
    return target / MANIFEST if model["artifact"].get("is_directory") else target.with_suffix(target.suffix + ".kwispr-model.json")


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def read_manifest(root: Path, model: dict[str, Any]) -> dict[str, Any] | None:
    path = manifest_path(root, model)
    if not path.exists():
        return None
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError:
        return None


def is_installed(root: Path, model: dict[str, Any]) -> bool:
    artifact = model["artifact"]
    target = model_path(root, model)
    expected = artifact["sha256"]
    if artifact.get("is_directory"):
        manifest = read_manifest(root, model)
        return bool(target.is_dir() and manifest and manifest.get("artifact_sha256") == expected)
    return target.is_file() and sha256_file(target) == expected


def write_manifest(root: Path, model: dict[str, Any]) -> None:
    path = manifest_path(root, model)
    path.parent.mkdir(parents=True, exist_ok=True)
    data = {
        "model_id": model["id"],
        "artifact_filename": model["artifact"]["filename"],
        "artifact_sha256": model["artifact"]["sha256"],
        "source_url": model["artifact"]["url"],
    }
    path.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def safe_extract_tar_gz(archive: Path, dest: Path) -> None:
    dest_resolved = dest.resolve()
    with tarfile.open(archive, "r:gz") as tf:
        for member in tf.getmembers():
            target = (dest / member.name).resolve()
            if not str(target).startswith(str(dest_resolved) + os.sep) and target != dest_resolved:
                raise RuntimeError(f"unsafe archive member path: {member.name}")
        tf.extractall(dest)


def download_url(url: str, dest: Path) -> None:
    with urllib.request.urlopen(url) as response, dest.open("wb") as out:
        shutil.copyfileobj(response, out)


def cmd_list(args: argparse.Namespace) -> int:
    catalog = load_catalog(args.catalog)
    root = args.model_dir
    for m in catalog.get("models", []):
        artifact = m["artifact"]
        status = "installed" if is_installed(root, m) else "not-installed"
        print(f"{m['id']}\t{status}\t{artifact.get('size_mb', '?')} MB\t{m['name']}")
    return 0


def cmd_verify(args: argparse.Namespace) -> int:
    catalog = load_catalog(args.catalog)
    models = model_map(catalog)
    selected = [models[args.model]] if args.model else list(models.values())
    ok = True
    for m in selected:
        installed = is_installed(args.model_dir, m)
        print(f"{m['id']}: {'ok' if installed else 'missing-or-invalid'}")
        ok = ok and installed
    return 0 if ok else 1


def cmd_download(args: argparse.Namespace) -> int:
    catalog = load_catalog(args.catalog)
    models = model_map(catalog)
    if args.model not in models:
        print(f"unknown model: {args.model}", file=sys.stderr)
        return 2
    model = models[args.model]
    artifact = model["artifact"]
    root = args.model_dir
    root.mkdir(parents=True, exist_ok=True)
    if is_installed(root, model):
        print(f"{model['id']}: already installed at {model_path(root, model)}")
        return 0

    with tempfile.TemporaryDirectory(prefix="kwispr-model-", dir=str(root)) as tmp_s:
        tmp = Path(tmp_s)
        archive = tmp / artifact["filename"]
        print(f"downloading {artifact['url']}")
        download_url(artifact["url"], archive)
        actual = sha256_file(archive)
        if actual != artifact["sha256"]:
            print(f"checksum mismatch for {model['id']}: expected {artifact['sha256']}, got {actual}", file=sys.stderr)
            return 1

        target = model_path(root, model)
        if artifact.get("archive_format") == "tar.gz":
            staging = tmp / "extract"
            staging.mkdir()
            safe_extract_tar_gz(archive, staging)
            if target.exists():
                shutil.rmtree(target) if target.is_dir() else target.unlink()
            target.mkdir(parents=True)
            for child in staging.iterdir():
                shutil.move(str(child), target / child.name)
            write_manifest(root, model)
        else:
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.move(str(archive), target)
            write_manifest(root, model)
    print(f"{model['id']}: installed at {model_path(root, model)}")
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--catalog", type=Path, default=DEFAULT_CATALOG, help=f"catalog JSON (default: {DEFAULT_CATALOG})")
    parser.add_argument("--model-dir", type=Path, default=DEFAULT_MODEL_DIR, help="model install directory (default: $KWISPR_MODEL_DIR or ~/.local/share/kwispr/models)")
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("list", help="list catalog models and install status").set_defaults(func=cmd_list)
    p_download = sub.add_parser("download", help="download and install one model")
    p_download.add_argument("model", help="catalog model id")
    p_download.set_defaults(func=cmd_download)
    p_verify = sub.add_parser("verify", help="verify installed model checksums/manifests")
    p_verify.add_argument("model", nargs="?", help="catalog model id (default: all)")
    p_verify.set_defaults(func=cmd_verify)
    args = parser.parse_args(argv)
    args.model_dir = args.model_dir.expanduser()
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
