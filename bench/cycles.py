# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Orion contributors. See LICENSE.txt.
"""Clock-independent comparison: SM cycles of one comp_keys launch, measured by Nsight Compute
with locked base clocks. Use this for clock-limited GPUs (e.g. RTX PRO 6000 at 506 W / 600 W),
where fewer cycles per launch translate 1:1 into speed; the wall-clock A/B in bench.py also
captures power/clock effects (power-capped laptops).

  python bench/cycles.py build/ref/orion.exe build/opt/orion.exe [--repeat 2]
  python bench/cycles.py build/ref/orion.exe build/tpb128/orion.exe@0.5
    (@k = keys per launch relative to the first build, e.g. a half-size grid; results are per key)
"""
import argparse, glob, os, re, shutil, statistics, subprocess, sys


def _ver_key(path):
    m = re.search(r'(\d+(?:\.\d+)*)', os.path.basename(os.path.dirname(path)))
    return tuple(int(x) for x in m.group(1).split('.')) if m else ()


def find_ncu():
    """Locate Nsight Compute CLI (NCU env var, PATH, or standard install directories)."""
    env = os.environ.get('NCU')
    if env and os.path.exists(env):
        return env
    for name in ('ncu', 'ncu.bat', 'ncu.exe'):
        found = shutil.which(name)
        if found:
            return found
    candidates = (
        glob.glob(r'C:\Program Files\NVIDIA Corporation\Nsight Compute *\ncu.bat')
        + glob.glob(r'C:\Program Files\NVIDIA Corporation\Nsight Compute *\ncu.exe')
        + glob.glob('/usr/local/cuda*/bin/ncu')
        + glob.glob('/opt/nvidia/nsight-compute/*/ncu')
    )
    if candidates:
        return sorted(candidates, key=_ver_key)[-1]
    sys.exit('Nsight Compute (ncu) not found. Add it to PATH or set the NCU environment variable.')


def parse_locale_number(s):
    """Parse a number formatted with either US (1,234.56) or EU/TR (1.234,56) separators."""
    s = s.strip()
    if ',' in s and '.' in s:
        if s.rfind(',') > s.rfind('.'):
            return float(s.replace('.', '').replace(',', '.'))
        return float(s.replace(',', ''))
    if ',' in s:
        parts = s.split(',')
        if len(parts) > 2 or (len(parts) == 2 and len(parts[1]) == 3 and len(parts[0]) <= 3):
            return float(s.replace(',', ''))
        return float(s.replace(',', '.'))
    if '.' in s:
        parts = s.split('.')
        if len(parts) > 2:
            return float(s.replace('.', ''))
        return float(s)
    return float(s)


def cycles(ncu, exe):
    r = subprocess.run([ncu, '--kernel-name', 'regex:comp_keys', '--launch-skip', '2', '--launch-count', '1',
                        '--clock-control', 'base', '--metrics', 'sm__cycles_elapsed.avg,sm__inst_executed.sum',
                        exe, '-gpuId', '0', '-start', '400000000000000000', '-range', '35',
                        '1BitcoinEaterAddressDontSendf59kuE'],
                       capture_output=True, text=True, encoding='utf-8', errors='replace',
                       cwd=os.path.dirname(exe), timeout=600)
    out = r.stdout.replace('\r', '\n')
    def num(name):
        m = re.search(name + r'\s+\S+\s+([\d.,]+)', out)
        if not m:
            sys.exit(f'Metric {name} not found in ncu output (rc={r.returncode}):\n{(out + r.stderr)[-2000:]}')
        return parse_locale_number(m.group(1))
    return num('sm__cycles_elapsed.avg'), num('sm__inst_executed.sum')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('exes', nargs='+')
    ap.add_argument('--repeat', type=int, default=1)
    a = ap.parse_args()
    ncu = find_ncu()
    res, work = {}, {}
    for _ in range(a.repeat):
        for spec in a.exes:
            e, _, k = spec.partition('@')
            e = os.path.abspath(e)
            work[e] = float(k or 1)
            c, i = cycles(ncu, e)
            c /= work[e]
            res.setdefault(e, []).append((c, i))
            print(f'  {c / 1e6:9.2f} Mcycles  {i / 1e9:6.2f} Ginst  {e}', flush=True)
    base = statistics.median(c for c, _ in res[os.path.abspath(a.exes[0].partition('@')[0])])
    print('\ncycles per key vs first (negative = faster on a clock-limited GPU)')
    for e, v in res.items():
        c = statistics.median(x for x, _ in v)
        print(f'  {100 * (c / base - 1):+6.2f}%   {e}')


if __name__ == '__main__':
    main()
