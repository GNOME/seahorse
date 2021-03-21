#!/usr/bin/env bash
#
# junit-report.sh: JUnit report helpers
#
# Source this file into your CI scripts to get a nice JUnit report file which
# can be shown in the GitLab UI.

JUNIT_REPORT_TESTS_FILE=$(mktemp)

# We need this to make sure we don't send funky stuff into the XML report,
# making it invalid XML (and thus unparsable by CI)
function escape_xml() {
  echo "$1" | sed -e 's/&/\&amp;/g; s/</\&lt;/g; s/>/\&gt;/g; s/"/\&quot;/g; s/'"'"'/\&#39;/g'
}

# Append a failed test case with the given name and message
function append_failed_test_case() {
  test_name="$1"
  test_message="$2"

  # Escape both fields before putting them into the xml
  test_name_esc="$(escape_xml "$test_name")"
  test_message_esc="$(escape_xml "$test_message")"

  echo "<testcase name=\"$test_name_esc\">" >> $JUNIT_REPORT_TESTS_FILE
  echo "    <failure message=\"$test_message_esc\"/>" >> $JUNIT_REPORT_TESTS_FILE
  echo "</testcase>" >> $JUNIT_REPORT_TESTS_FILE

  # Also output to stderr, so it shows up in the job output
  echo >&2 "Test '$test_name' failed: $test_message"
}

# Append a successful test case with the given name
function append_passed_test_case() {
  test_name="$1"
  test_name_esc="$(escape_xml "$test_name")"

  echo "<testcase name=\"$test_name_esc\"></testcase>" >> $JUNIT_REPORT_TESTS_FILE

  # Also output to stderr, so it shows up in the job output
  echo >&2 "Test '$test_name' succeeded"
}

# Aggregates the test cases into a proper JUnit report XML file
function generate_junit_report() {
  junit_report_file="$1"
  testsuite_name="$2"

  num_tests=$(fgrep '<testcase' -- "$JUNIT_REPORT_TESTS_FILE" | wc -l)
  num_failures=$(fgrep '<failure' -- "$JUNIT_REPORT_TESTS_FILE" | wc -l )

  echo Generating JUnit report \"$(pwd)/$junit_report_file\" with $num_tests tests and $num_failures failures.

  cat > $junit_report_file << __EOF__
<?xml version="1.0" encoding="utf-8"?>
<testsuites tests="$num_tests" errors="0" failures="$num_failures">
<testsuite name="$testsuite_name" tests="$num_tests" errors="0" failures="$num_failures" skipped="0">
$(< $JUNIT_REPORT_TESTS_FILE)
</testsuite>
</testsuites>
__EOF__
}

# Returns a non-zero exit status if any of the tests in the given JUnit report failed
# You probably want to call this at the very end of your script.
function check_junit_report() {
  junit_report_file="$1"

  ! fgrep -q '<failure' -- "$junit_report_file"
}
