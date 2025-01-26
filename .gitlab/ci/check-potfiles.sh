#!/usr/bin/env bash

# find source files that contain gettext keywords
vala_files="$(grep -lR '\(gettext\|[^I_)]_\)('  ./**/*.vala | sed 's/^.\///')"
# Be careful with C files, sice we don't want to include the Vala-generated ones
c_srcdirs="libseahorse pgp pkcs11"
c_files="$(grep -lR --include='*.c' '\(gettext\|[^I_)]_\)(' $c_srcdirs)"

# find ui files that contain translatable string
ui_files="$(grep -lRi --include='*.ui' 'translatable="[ty1]' ./**/*.ui | sed 's/^.\///')"

files="$vala_files $c_files $ui_files"

# filter out excluded files
if [ -f po/POTFILES.skip ]; then
  files="$(for f in $files; do ! grep -q "^$f$" po/POTFILES.skip && echo "$f"; done)"
fi

# Test 1: find all files that are missing from POTFILES.in
missing="$(for f in $files; do ! grep -q "^$f$" po/POTFILES.in && echo "$f"; done)"
if [ ${#missing} -ne 0 ]; then
  echo >&2 "The following files are missing from po/POTFILES.in:"
  for f in "${missing[@]}"; do
    echo "  $f" >&2
  done
  echo >&2
  exit 1
fi

# Test 2: find all Vala files that miss a corresponding .c file in POTFILES.skip
vala_c_files="$(for f in $vala_files; do echo "${f%.vala}.c"; done)"
vala_c_files_missing="$(for f in "${vala_c_files[@]}"; do ! grep -q "^$f$" po/POTFILES.skip && echo "$f"; done)"
if [ ${#vala_c_files_missing} -ne 0 ]; then
  echo >&2 "The following files are missing from po/POTFILES.skip:"
  for f in "${vala_c_files_missing[@]}"; do
    echo "  $f" >&2
  done
  echo >&2
  exit 1
fi

# Test 3: find all files that are mentioned in POTFILES.in that don't exist
readarray -t potfiles_in_items < <(grep -v "^#" po/POTFILES.in)
potfiles_in_nonexistent="$(for f in "${potfiles_in_items[@]}"; do [[ -f "$f" ]] || echo "$f"; done)"
if [ ${#potfiles_in_nonexistent} -ne 0 ]; then
  echo >&2 "The following files are referenced in po/POTFILES.in, but don't exist:"
  echo >&2 ""
  for f in "${potfiles_in_nonexistent[@]}"; do
    echo "$f" >&2
  done
  echo >&2
  exit 1
fi
