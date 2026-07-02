import unittest

import tickline_tools


class PackageTests(unittest.TestCase):
    def test_version_is_declared(self) -> None:
        self.assertEqual(tickline_tools.__version__, "0.2.0")


if __name__ == "__main__":
    unittest.main()
