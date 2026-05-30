#!/bin/bash
set -euo pipefail

abort() {
  echo "error: $*" 1>&2
  exit 1
}

vcpkg_installed_dir=${1:-}
triplet=${2:-}

if [[ -z $vcpkg_installed_dir ]]; then
  if [[ -d build/vcpkg_installed ]]; then
    vcpkg_installed_dir=build/vcpkg_installed
  else
    abort "vcpkg_installed directory not found"
  fi
fi
if [[ ! -d $vcpkg_installed_dir ]]; then
  abort "vcpkg_installed directory not found: $vcpkg_installed_dir"
fi

if [[ -z $triplet ]]; then
  for path in "$vcpkg_installed_dir"/*; do
    dir=$(basename "$path")
    case $dir in
    vcpkg)
      ;;
    *)
      if [[ -z $triplet ]]; then
        triplet=$dir
      else
        abort "multiple vcpkg triplets found: specify one explicitly"
      fi
      ;;
    esac
  done
fi
if [[ -z $triplet ]]; then
  abort "could not determine the vcpkg triplet"
fi
if [[ ! -d "$vcpkg_installed_dir/$triplet" ]]; then
  abort "vcpkg triplet directory not found: $vcpkg_installed_dir/$triplet"
fi

share_dir="$vcpkg_installed_dir/$triplet/share"

list_packages() {
  for copyright_file in "$share_dir"/*/copyright; do
    package=$(basename "$(dirname "$copyright_file")")
    case "$package" in
    vcpkg-cmake* | vcpkg-make*) # build-time helper only
      continue
      ;;
    gettimeofday) # not linked
      continue
      ;;
    doctest) # test-only dependency
      continue
      ;;
    esac
    echo "$package"
  done | sort
}

print_package_info() {
  local spdx_json_file
  spdx_json_file=$1
  jq -r '
    def strip_port_version:
      sub("#[0-9]+$"; "");

    def strip_outer_parens:
      if startswith("(") and endswith(")") then .[1:-1] else . end;

    .packages[]
    | select(.SPDXID == "SPDXRef-port")
    | "\(.name) \((.versionInfo // "") | strip_port_version)\nLicense: \((.licenseConcluded // .licenseDeclared // "") | strip_outer_parens)\nURL: \(.homepage // .downloadLocation // "NONE")"
  ' "$spdx_json_file"
}

first_package=:

for package in $(list_packages); do
  if $first_package; then
    first_package=false
  else
    echo
    echo
  fi
  print_package_info "$share_dir/$package/vcpkg.spdx.json"
  echo 'License notice:'
  echo
  cat "$share_dir/$package/copyright"
done
