#!/usr/bin/env python3
"""Tiny validation tests for kwispr-models.py."""

from __future__ import annotations

import importlib.util
import io
import json
import tarfile
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path

MODULE_PATH = Path(__file__).with_name("kwispr-models.py")
spec = importlib.util.spec_from_file_location("kwispr_models", MODULE_PATH)
assert spec and spec.loader
kwispr_models = importlib.util.module_from_spec(spec)
spec.loader.exec_module(kwispr_models)


def sha256(path: Path) -> str:
    return kwispr_models.sha256_file(path)


class KwisprModelsValidationTest(unittest.TestCase):
    def write_catalog(self, root: Path, artifact: dict) -> Path:
        catalog = {
            "models": [
                {
                    "id": "tiny",
                    "name": "Tiny test model",
                    "artifact": artifact,
                }
            ]
        }
        path = root / "catalog.json"
        path.write_text(json.dumps(catalog), encoding="utf-8")
        return path

    def run_cli(self, *args: str) -> int:
        out = io.StringIO()
        with redirect_stdout(out):
            rc = kwispr_models.main(list(args))
        self.last_output = out.getvalue()
        return rc

    def test_tar_gz_directory_install_detects_modified_deleted_and_extra_files(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_s:
            tmp = Path(tmp_s)
            src = tmp / "src"
            src.mkdir()
            (src / "model.bin").write_text("weights", encoding="utf-8")
            (src / "config.json").write_text('{"ok": true}', encoding="utf-8")
            archive = tmp / "tiny.tar.gz"
            with tarfile.open(archive, "w:gz") as tf:
                tf.add(src / "model.bin", arcname="model.bin")
                tf.add(src / "config.json", arcname="config.json")
            catalog = self.write_catalog(
                tmp,
                {
                    "filename": archive.name,
                    "url": archive.as_uri(),
                    "sha256": sha256(archive),
                    "archive_format": "tar.gz",
                    "is_directory": True,
                },
            )
            model_dir = tmp / "models"

            self.assertEqual(self.run_cli("--catalog", str(catalog), "--model-dir", str(model_dir), "download", "tiny"), 0)
            self.assertEqual(self.run_cli("--catalog", str(catalog), "--model-dir", str(model_dir), "verify", "tiny"), 0)
            self.assertIn("tiny: ok", self.last_output)

            target = model_dir / "tiny"
            mutations = [
                lambda: (target / "model.bin").write_text("tampered", encoding="utf-8"),
                lambda: (target / "config.json").unlink(),
                lambda: (target / "unexpected.txt").write_text("extra", encoding="utf-8"),
            ]
            for mutate in mutations:
                self.assertEqual(self.run_cli("--catalog", str(catalog), "--model-dir", str(model_dir), "download", "tiny"), 0)
                mutate()
                self.assertEqual(self.run_cli("--catalog", str(catalog), "--model-dir", str(model_dir), "verify", "tiny"), 1)
                self.assertIn("tiny: missing-or-invalid", self.last_output)
                self.assertEqual(self.run_cli("--catalog", str(catalog), "--model-dir", str(model_dir), "list"), 0)
                self.assertIn("tiny\tnot-installed", self.last_output)

    def test_single_file_install_still_verifies_by_checksum(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_s:
            tmp = Path(tmp_s)
            artifact_file = tmp / "tiny.bin"
            artifact_file.write_text("single file model", encoding="utf-8")
            catalog = self.write_catalog(
                tmp,
                {
                    "filename": artifact_file.name,
                    "url": artifact_file.as_uri(),
                    "sha256": sha256(artifact_file),
                },
            )
            model_dir = tmp / "models"

            self.assertEqual(self.run_cli("--catalog", str(catalog), "--model-dir", str(model_dir), "download", "tiny"), 0)
            self.assertEqual(self.run_cli("--catalog", str(catalog), "--model-dir", str(model_dir), "verify", "tiny"), 0)
            (model_dir / "tiny.bin").write_text("tampered", encoding="utf-8")
            self.assertEqual(self.run_cli("--catalog", str(catalog), "--model-dir", str(model_dir), "verify", "tiny"), 1)


if __name__ == "__main__":
    unittest.main()
