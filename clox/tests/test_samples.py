import unittest
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CLOX_DIR = ROOT / 'clox'
BIN_PATH = CLOX_DIR / 'build' / 'bin' / 'clox'
SAMPLES_DIR = ROOT / 'samples'

class CloxSamplesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # ensure submodules are available and binary is built
        subprocess.run(['git', 'submodule', 'update', '--init'], cwd=ROOT, check=True)
        subprocess.run(['make', '-C', 'clox'], cwd=ROOT, check=True)

    def run_script(self, name):
        script = SAMPLES_DIR / name
        result = subprocess.run([BIN_PATH, script], capture_output=True, text=True, cwd=ROOT, check=True)
        # clox prints to stderr
        output = result.stderr.strip().splitlines()
        return output

    def test_var_shadowing(self):
        output = self.run_script('var_shadowing.lox')
        self.assertEqual(output, ['0', '1'])

    def test_closure_counter(self):
        output = self.run_script('closure_counter.lox')
        self.assertEqual(output, ['0', '1', '2', '1', '0', '-1'])

    def test_implicit_semicolon(self):
        output = self.run_script('implicit_semicolon.lox')
        self.assertEqual(output, ['first item', 'second item', 'third item'])

    def test_fib_last_line(self):
        output = self.run_script('fib.lox')
        self.assertTrue(output[-1].endswith('14 -> 377'))

if __name__ == '__main__':
    unittest.main()
