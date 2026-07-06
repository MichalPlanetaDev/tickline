import subprocess
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[3]
LAUNCHER_PATH = (
    REPOSITORY_ROOT
    / "scripts"
    / "tickline-analytics.sh"
)


class AnalyticsLauncherTests(unittest.TestCase):
    def test_launcher_exposes_cli_help(self) -> None:
        result = subprocess.run(
            ["bash", str(LAUNCHER_PATH), "--help"],
            cwd=REPOSITORY_ROOT,
            text=True,
            capture_output=True,
            check=False,
        )

        self.assertEqual(
            result.returncode,
            0,
            msg=result.stderr,
        )
        self.assertIn(
            "tickline-analytics",
            result.stdout,
        )
        self.assertIn(
            "Generate deterministic analytics reports",
            result.stdout,
        )
        self.assertEqual(result.stderr, "")


if __name__ == "__main__":
    unittest.main()
