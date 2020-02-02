#!/usr/bin/env python3

import re
import sys
import subprocess

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

if len(sys.argv) <= 2:
    sys.exit(1)

gpg_path = sys.argv[1]
accepted_versions = sys.argv[2:]

# Parse and print the version
proc = subprocess.Popen([gpg_path, '--version'],
                        stdout=subprocess.PIPE,
                        universal_newlines=True)
gpg_version = parse_version(proc.stdout.read())
print(gpg_version, end='')

# Then return whether we're compatible with that version
for accepted_version in accepted_versions:
    if check_version(gpg_version, accepted_version):
        sys.exit(0)

sys.exit(1)
