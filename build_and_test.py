"""Build script: regenerate parser, reinstall package, run integration test."""
import subprocess, sys, os, pathlib, time, shutil

tree_dir = pathlib.Path(r'e:\temp\tree-sitter-sql')
ts_bin   = tree_dir / 'tree-sitter.exe'
node_dir = r'e:\temp\node\node-v22.15.0-win-x64'
pyd_file = tree_dir / 'bindings' / 'python' / 'tree_sitter_sql' / '_binding.pyd'

env = os.environ.copy()
env['PATH'] = node_dir + ';' + env.get('PATH', '')

# Step 1: generate
print('1. Running tree-sitter generate ...')
r = subprocess.run([str(ts_bin), 'generate'], cwd=str(tree_dir), env=env, capture_output=True, text=True)
if r.returncode != 0:
    print('FAIL:', r.stderr)
    sys.exit(1)
print('   OK:', r.stderr.strip() or '(no warnings)')
print('   parser.c:', (tree_dir / 'src' / 'parser.c').stat().st_mtime)

# Step 2: delete old .pyd
if pyd_file.exists():
    pyd_file.unlink()
    print('2. Deleted old _binding.pyd')

# Step 3: reinstall
print('3. pip install -e ...')
r = subprocess.run([sys.executable, '-m', 'pip', 'install', '-e', '.'],
                   cwd=str(tree_dir), capture_output=True, text=True)
if r.returncode != 0:
    print('FAIL:', r.stderr[-500:])
    sys.exit(1)
print('   OK:', r.stdout.strip().split('\n')[-1])
print('   _binding.pyd:', pyd_file.stat().st_mtime)

# Step 4: run test
print('4. Running test_speedy.py ...')
r = subprocess.run([sys.executable, str(tree_dir / 'test_speedy.py')], capture_output=True, text=True)
print(r.stdout)
if r.stderr:
    print('STDERR:', r.stderr[:500])
sys.exit(r.returncode)
