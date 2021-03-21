#!/usr/bin/env bash
#
# style-check.sh: Performs some basic style checks

# Source the JUnit helpers
scriptdir="$(dirname "$BASH_SOURCE")"
source "$scriptdir/junit-report.sh"

TESTNAME="No trailing whitespace"
source_dirs="common data gkr libseahorse pgp pkcs11 src ssh"
trailing_ws_occurrences="$(grep -nRI '[[:blank:]]$' $source_dirs)"
if [[ -z "$trailing_ws_occurrences" ]]; then
  append_passed_test_case "$TESTNAME"
else
  append_failed_test_case "$TESTNAME" \
    $'Please remove the trailing whitespace at the following places:\n\n'"$trailing_ws_occurrences"
fi


generate_junit_report "$CI_JOB_NAME-junit-report.xml" "$CI_JOB_NAME"
check_junit_report "$CI_JOB_NAME-junit-report.xml"
