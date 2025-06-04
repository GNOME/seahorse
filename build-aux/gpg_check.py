#!/usr/bin/env python3

import argparse
import os
import re
import sys
import subprocess

os.environ['LC_ALL'] = 'C'

# Run gpg --version in a subprocess and return stdout
def run_gpg_version(gpg_path):
    proc = subprocess.Popen([gpg_path, '--version'],
                            stdout=subprocess.PIPE,
                            universal_newlines=True)
    return proc.stdout.read()

def version_command(args):
    # Parses the GPG version from the output of the --version flag.
    # Should work on the output for `gpg` and `gpg2`
    def parse_version(gpg_output):
        version_line = gpg_output.splitlines()[0]
        return version_line.strip().split(' ')[-1]

    # Checks whether the GPG version is compatible with the given version.
    # This means that the major and minor version should be equal.
    def check_version(gpg_version, accepted_version):
        acc_major, acc_minor, acc_micro = accepted_version.split('.', 2)

        # The GPG version we got still might have to be sanitized
        # For example, sometimes it adds a "-unknown" suffix
        gpg_major_str, gpg_minor_str, gpg_micro_str = gpg_version.split('.', 2)

        gpg_major = int(gpg_major_str)
        gpg_minor = int(gpg_minor_str)

        # Get rid of any fuzz that comes after the version
        _micro_match = re.match(r'^([0-9]+)[^0-9]*', gpg_micro_str)
        gpg_micro = int(_micro_match.group(1)) if _micro_match != None else 0

        return gpg_major == int(acc_major) and gpg_minor == int(acc_minor) and gpg_micro >= int(acc_micro)

    # Parse and print the version
    gpg_version = parse_version(run_gpg_version(args.gpg_path))
    print(gpg_version, end='')

    # Then return whether we're compatible with that version
    for accepted_version in args.accepted_versions:
        if check_version(gpg_version, accepted_version):
            sys.exit(0)

    sys.exit(1)


def pubkey_algos_command(args):
    output = run_gpg_version(args.gpg_path)
    formats_match = re.search(r'Pubkey: (.+)', output)
    if formats_match == None:
        print("Couldn't parse pubkey algorithms from the output of `gpg --version`")
        sys.exit(1)
    formats = formats_match.group(1)
    # Get rid of the spaces here alreayd
    print(','.join(formats.split(', ')))


def parse_args():
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest='command')

    version_parser = subparsers.add_parser('version')
    version_parser.add_argument('gpg_path')
    version_parser.add_argument('accepted_versions', nargs='*')

    pubkey_algos_parser = subparsers.add_parser('pubkey-algos')
    pubkey_algos_parser.add_argument('gpg_path')

    return parser.parse_args()

args = parse_args()
if args.command == 'version':
    version_command(args)
elif args.command == 'pubkey-algos':
    pubkey_algos_command(args)
else:
    print(f'Unknown command {args.command}')
    sys.exit(1)
