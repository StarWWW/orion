# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Orion contributors. See LICENSE.txt.
"""Builds the Orion desktop front end on Windows (Qt 6 + MSVC + CMake/Ninja from Visual Studio).

  python gui/build.py                      # -> build/gui/orion-gui.exe, with the Qt DLLs next to it
  python gui/build.py --qt C:/Qt/6.10.3/msvc2022_64

Qt is taken from --qt, QT_DIR, or the newest %USERPROFILE%/Qt/6.*/msvc*_64 or C:/Qt/6.*/msvc*_64.
The front end runs the orion engine: build that too (python bench/build.py opt).
"""
import argparse, glob, os, re, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'bench'))
from build import find_vcvars  # noqa: E402


def find_qt():
    env = os.environ.get('QT_DIR')
    if env:
        return env
    def ver(p):
        m = re.search(r'[\\/](\d+)\.(\d+)\.(\d+)[\\/]', p)
        return tuple(int(x) for x in m.groups()) if m else ()
    dirs = [d for pattern in (os.path.expanduser(r'~\Qt\6.*\msvc*_64'), r'C:\Qt\6.*\msvc*_64')
            for d in glob.glob(pattern) if os.path.exists(os.path.join(d, 'bin', 'windeployqt.exe'))]
    if dirs:
        return sorted(dirs, key=ver)[-1]
    sys.exit('Qt 6 for MSVC not found: pass --qt or set QT_DIR (e.g. C:/Qt/6.10.3/msvc2022_64).')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--qt', help='Qt prefix, e.g. C:/Qt/6.10.3/msvc2022_64')
    ap.add_argument('--config', default='Release', choices=['Release', 'Debug', 'RelWithDebInfo'])
    a = ap.parse_args()

    qt = os.path.abspath(a.qt or find_qt())
    out = os.path.join(ROOT, 'build', 'gui')
    os.makedirs(out, exist_ok=True)
    deploy_mode = '--debug' if a.config == 'Debug' else '--release'
    bat = os.path.join(out, 'build-gui.bat')
    with open(bat, 'w', encoding='utf-8') as f:
        f.write('@echo off\n'
                f'call "{find_vcvars()}" >nul\n'
                f'cmake -S "{os.path.join(ROOT, "gui")}" -B "{out}" -G Ninja -DCMAKE_BUILD_TYPE={a.config} '
                f'-DCMAKE_PREFIX_PATH="{qt}" || exit /b 1\n'
                f'cmake --build "{out}" || exit /b 1\n'
                f'"{os.path.join(qt, "bin", "windeployqt.exe")}" {deploy_mode} --no-translations '
                f'--qmldir "{os.path.join(ROOT, "gui", "qml")}" "{os.path.join(out, "orion-gui.exe")}" >nul || exit /b 1\n')
    r = subprocess.run(['cmd', '/c', bat])
    if r.returncode:
        sys.exit(f'GUI build failed (rc={r.returncode})')
    print('built', os.path.join(out, 'orion-gui.exe'))


if __name__ == '__main__':
    main()
