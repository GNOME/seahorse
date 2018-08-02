#!/usr/bin/env python3

import sys
import subprocess

# Parses the GPG version from the output of the --version flag.
# Should work on the output for `gpg`, `gpg2` and `gpgme-config`.
def parse_version(gpg_output):
    version_line = gpg_output.splitlines()[0]
    return version_line.strip().split(' ')[-1]

# Checks whether the GPG version is compatible with the given version.
# For GPG, this means that the major and minor version should be equal;
# for GPGME, this means only the major version should be equal.
def check_version(gpg_version, accepted_version, is_gpgme = False):
    gpg_major, gpg_minor, gpg_micro = gpg_version.split('.', 2)
    acc_major, acc_minor, acc_micro = accepted_version.split('.', 2)
    if is_gpgme:
        return int(gpg_major) == int(acc_major) and int(gpg_minor) >= int(acc_minor) and int(gpg_micro) >= int(acc_micro)
    else:
        return int(gpg_major) == int(acc_major) and int(gpg_minor) == int(acc_minor) and int(gpg_micro) >= int(acc_micro)

if len(sys.argv) <= 3:
    sys.exit(1)

gpg_path = sys.argv[1]
is_gpgme =  True if sys.argv[2].lower() == 'true' else False
accepted_versions = sys.argv[3:]

# Parse and print the version
proc = subprocess.Popen([gpg_path, '--version'],
                        stdout=subprocess.PIPE,
                        universal_newlines=True)
gpg_version = parse_version(proc.stdout.read())
print(gpg_version, end='')

# Then return whether we're compatible with that version
for accepted_version in accepted_versions:
    if check_version(gpg_version, accepted_version, is_gpgme):
        sys.exit(0)

sys.exit(1)
