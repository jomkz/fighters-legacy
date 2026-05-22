#!/usr/bin/env bash
# Prepend unreleased commits to CHANGELOG.md using git-cliff.
# Install git-cliff: cargo install git-cliff  |  dnf install git-cliff  |  brew install git-cliff
set -euo pipefail

git cliff --unreleased --prepend CHANGELOG.md
echo "CHANGELOG.md updated. Review, then commit:"
echo "  git commit -s -m 'chore: update changelog for release'"
