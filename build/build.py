#!/usr/bin/env python3
#
# Mitochodria Vendor Builder
#
# Builds vendor components and packages a validated Android vendor image.
#
# Authors:
# worryzu <worryzu@gmail.com> @LinearTeam
#
# Copyright (C) 2026 Evarentha
# SPDX-License-Identifier: MIT

"""Build Mitochodria vendor components from the maintained source tree.

Targets include the CM4 loader, ARM32/ARM64 camera and graphics HALs,
external kernel modules, factory-tree staging, and raw ext4 image packaging.
Recipes resolve explicit toolchain, Android source, factory, and kernel paths.
All artifacts and logs are written into an explicitly selected output directory.
Packaging validates filesystem contents and selected SELinux labels.

Run with --help for target and dependency arguments. The all target builds
every component before staging and packaging. Factory inputs are read-only;
existing stage trees and vendor images are not overwritten. See the parent
README for dependency versions, usage examples, and on-device validation needs.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

SOURCE = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('target', choices=['check', 'sensorhub', 'camera', 'graphics', 'modules', 'stage', 'pack', 'all'])
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--kernel-source', type=Path, help='Matching kernel source tree (required for graphics/all)')
    parser.add_argument('--kernel-output', type=Path)
    parser.add_argument('--android-source', type=Path, help='Android headers and NDK r18 CRTs (validated with JingPad BSP)')
    parser.add_argument('--factory', type=Path, help='Read-only factory tree containing vendor/ and system/system/')
    parser.add_argument('--toolchain', type=Path, help='Clang installation containing bin/ and lib/ (required for compilation)')
    parser.add_argument('--host-library-dir', type=Path, action='append', default=[],
                        help='Additional host shared-library directory; repeat if needed')
    parser.add_argument('--image-tools', type=Path, help='Directory containing mke2fs and e2fsdroid')
    parser.add_argument('--jobs', type=int, default=8)
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error('--jobs must be positive')
    # Resolve CLI paths relative to the caller before changing subprocess cwd.
    for name in ['output', 'kernel_source', 'kernel_output', 'android_source', 'factory', 'toolchain', 'image_tools']:
        value = getattr(args, name)
        if value is not None:
            setattr(args, name, value.expanduser().resolve())
    args.host_library_dir = [p.expanduser().resolve() for p in args.host_library_dir]
    out = args.output
    # Module builds replace generated source copies. Never let output overlap
    # sources or external inputs, including through symlinks or an ancestor.
    inputs = [SOURCE] + [getattr(args, name) for name in
                        ['kernel_source', 'kernel_output', 'android_source', 'factory', 'toolchain', 'image_tools']]
    inputs += args.host_library_dir
    for source in filter(None, inputs):
        if out == source or source in out.parents or out in source.parents:
            parser.error('--output must not overlap a source or dependency directory: '+str(source))
    needs_user = args.target in ['sensorhub', 'camera', 'graphics', 'all']
    if needs_user and (not args.android_source or not args.factory):
        parser.error('userspace builds require --android-source and --factory')
    if args.target in ['sensorhub', 'camera', 'graphics', 'modules', 'all'] and not args.toolchain:
        parser.error('compilation requires --toolchain')
    if args.target in ['graphics', 'all'] and not args.kernel_source:
        parser.error('graphics builds require --kernel-source')
    if args.target in ['modules', 'all'] and not args.kernel_output:
        parser.error('module builds require a fully built --kernel-output')
    if args.target in ['stage', 'pack', 'all'] and not args.factory:
        parser.error('image staging requires --factory')
    if args.target in ['pack', 'all'] and not args.image_tools:
        parser.error('packing requires --image-tools')
    for source in filter(None, inputs):
        if not source.is_dir():
            parser.error('source or dependency directory does not exist: '+str(source))
    out.mkdir(parents=True, exist_ok=True)
    logs = out/'logs'
    logs.mkdir(exist_ok=True)
    env = os.environ.copy()
    if args.toolchain:
        env['PATH'] = str(args.toolchain/'bin') + os.pathsep + env.get('PATH', '')
    if args.host_library_dir:
        env['LD_LIBRARY_PATH'] = os.pathsep.join(
            [str(p) for p in args.host_library_dir] +
            ([env['LD_LIBRARY_PATH']] if env.get('LD_LIBRARY_PATH') else []))
    mappings = dict(source=str(SOURCE), output=str(out))
    for key, name in [('kernel', 'kernel_source'), ('toolchain', 'toolchain'),
                      ('android', 'android_source'), ('factory', 'factory')]:
        if getattr(args, name) is not None:
            mappings[key] = str(getattr(args, name))

    def run(command, label, extra_env=None):
        command = [str(x) for x in command]
        (logs/(label+'.command.json')).write_text(json.dumps(command, indent=2)+'\n')
        with (logs/(label+'.log')).open('wb') as log:
            result = subprocess.run(command, cwd=out, env=extra_env or env, stdout=log, stderr=subprocess.STDOUT)
        if result.returncode:
            raise RuntimeError(f'{label} failed ({result.returncode}); see {logs/(label+".log")}')

    def expand(command):
        command = [x.format_map(mappings) for x in command]
        if '-o' in command:
            Path(command[command.index('-o')+1]).parent.mkdir(parents=True, exist_ok=True)
        return command

    def userspace(component):
        recipes = json.loads((SOURCE/'build/recipes.json').read_text())
        keys = [component] if component == 'sensorhub' else [component+'-arm', component+'-arm64']
        for key in keys:
            recipe = recipes[key]
            with ThreadPoolExecutor(max_workers=args.jobs) as pool:
                futures = [pool.submit(run, expand(cmd), f'{key}-compile-{i}') for i, cmd in enumerate(recipe['compile'])]
                for future in futures:
                    future.result()
            for i, cmd in enumerate(recipe['link']):
                run(expand(cmd), f'{key}-link-{i}')
        print('PASS built', component)

    def modules():
        obj = args.kernel_output.resolve()
        if not (obj/'Module.symvers').is_file() or not (obj/'.config').is_file():
            raise RuntimeError('kernel-output must contain .config and final Module.symvers')
        menv = env.copy()
        for name, value in dict(ISP_VERSION='isp2.6', ISP_ADAPT_VERSION='roc1', CSI_VERSION='receiver_r2p1',
                                CPP_VERSION='lite_r5p0', FD_VERSION='fd2.0', DVFS='roc1').items():
            menv['BSP_BOARD_CAMERA_MODULE_'+name] = value
        for tree in ['camera-port', 'npu-port', 'wifi-port']:
            build = out/'modules'/tree
            if build.exists():
                shutil.rmtree(build)
            shutil.copytree(SOURCE/'modules'/tree, build, symlinks=True)
            command = ['make', '-C', obj, 'M='+str(build), 'BCMDHD_ROOT='+str(build), 'ARCH=arm64',
                       'LLVM=1', 'LLVM_IAS=1', 'CLANG_TRIPLE=aarch64-linux-gnu-',
                       'KCFLAGS=-Wno-default-const-init-var-unsafe -Wno-default-const-init-field-unsafe',
                       '-j'+str(args.jobs), 'modules']
            run(command, tree, menv)
        print('PASS built external modules')

    def stage():
        factory = args.factory.resolve()/'vendor'
        if not factory.is_dir():
            raise RuntimeError('factory/vendor is missing')
        stage_dir = out/'stage/vendor'
        if stage_dir.exists():
            raise RuntimeError('stage/vendor already exists; use a new output directory or remove that generated stage explicitly')
        plan = json.loads((SOURCE/'image/plan.json').read_text())
        for row in plan:
            if row['kind'] != 'remove':
                src = out/row['source'] if row['kind'] == 'built' else SOURCE/row['source']
                if not src.is_file():
                    raise RuntimeError(f'missing input {src}; build the required component first')
        shutil.copytree(factory, stage_dir, symlinks=True)
        manifest = []
        for row in plan:
            target = stage_dir/row['target']
            if row['kind'] == 'remove':
                if target.exists() or target.is_symlink():
                    target.unlink()
                manifest.append(dict(target=row['target'], action='remove'))
                continue
            src = out/row['source'] if row['kind'] == 'built' else SOURCE/row['source']
            target.parent.mkdir(parents=True, exist_ok=True)
            if target.is_symlink():
                target.unlink()
            shutil.copy2(src, target)
            manifest.append(dict(target=row['target'], source=row['source'], kind=row['kind'],
                                 after=hashlib.sha256(target.read_bytes()).hexdigest()))
        (out/'stage/manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
        print('PASS staged vendor overlay')

    def pack():
        stage_dir = out/'stage/vendor'
        if not stage_dir.is_dir():
            stage()
        image = out/'vendor.img'
        if image.exists():
            raise RuntimeError('vendor.img already exists; use a new output directory')
        run([args.image_tools.resolve()/'mke2fs', '-t', 'ext4', '-b', '4096', '-m', '0', '-I', '256',
             '-O', '^has_journal,^64bit,^metadata_csum,^orphan_file', '-L', 'vendor', '-E', 'lazy_itable_init=0',
             '-F', image, '140800'], 'pack-mke2fs')
        run([args.image_tools.resolve()/'e2fsdroid', '-e', '-T', '1724033282', '-C', SOURCE/'image/vendor_fs_config',
             '-S', SOURCE/'image/vendor_file_contexts', '-a', '/vendor', '-f', stage_dir, image], 'pack-e2fsdroid')
        run(['e2fsck', '-fn', image], 'pack-fsck')
        verify = out/'readback'
        verify.mkdir(exist_ok=True)
        for i, row in enumerate(json.loads((out/'stage/manifest.json').read_text())):
            target = '/'+row['target']
            if row.get('action') == 'remove':
                p = subprocess.run(['debugfs', '-R', 'stat '+target, str(image)],
                                   cwd=out, env=env, capture_output=True)
                if b'File not found' not in p.stderr:
                    raise RuntimeError('removed file still present: '+target)
            else:
                dest = verify/str(i)
                # debugfs parses its own command string, separately from argv.
                escaped = str(dest).replace('\\', '\\\\').replace('"', '\\"')
                run(['debugfs', '-R', f'dump {target} "{escaped}"', image], f'readback-{i}')
                if hashlib.sha256(dest.read_bytes()).hexdigest() != row['after']:
                    raise RuntimeError('packed content mismatch: '+target)
                label = None
                if target.startswith('/etc/init/') and target.endswith('.rc'):
                    label = 'vendor_configs_file'
                elif target == '/bin/mitochodria-sensorhub-boot':
                    label = 'modem_control_exec'
                elif target.startswith('/lib/modules/') and target.endswith('.ko'):
                    label = 'vendor_file'
                if label:
                    attrs = subprocess.check_output(
                        ['debugfs', '-R', 'ea_list '+target, str(image)],
                        cwd=out, env=env, stderr=subprocess.DEVNULL, text=True)
                    if 'u:object_r:'+label+':s0' not in attrs:
                        raise RuntimeError('packed SELinux label mismatch: '+target)
        b = image.read_bytes()
        (out/'artifact.json').write_text(json.dumps(dict(path=str(image), bytes=len(b), md5=hashlib.md5(b).hexdigest(),
                                                        sha256=hashlib.sha256(b).hexdigest()), indent=2)+'\n')
        print('PASS packed and read back', image)

    if args.target == 'check':
        plan = json.loads((SOURCE/'image/plan.json').read_text())
        for row in plan:
            if row['kind'] in ['overlay', 'prebuilt']:
                if hashlib.sha256((SOURCE/row['source']).read_bytes()).hexdigest() != row['reference_sha256']:
                    raise RuntimeError('imported reference mismatch: '+row['source'])
        print('PASS imported overlays and prebuilts match validated reference')
    elif args.target in ['sensorhub', 'camera', 'graphics']:
        userspace(args.target)
    elif args.target == 'modules':
        modules()
    elif args.target == 'stage':
        stage()
    elif args.target == 'pack':
        pack()
    elif args.target == 'all':
        for component in ['sensorhub', 'camera', 'graphics']:
            userspace(component)
        modules()
        stage()
        pack()


if __name__ == '__main__':
    main()
