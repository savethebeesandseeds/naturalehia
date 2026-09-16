#!/usr/bin/env bash
# Compile this standalone synthesis using the project's existing LaTeX tools.
set -euo pipefail
paper_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$paper_dir"
build_dir=${1:-"$paper_dir/build"}
mkdir -p "$build_dir"
build_dir=$(CDPATH= cd -- "$build_dir" && pwd)
export TEXINPUTS="$paper_dir/vendor/IEEEtran//:${TEXINPUTS:-}"
export BSTINPUTS="$paper_dir/vendor/IEEEtran/bibtex//:${BSTINPUTS:-}"
latexmk -pdf -interaction=nonstopmode -halt-on-error -file-line-error \
  -outdir="$build_dir" fostering-cellular-agriculture-one-page.tex
if command -v pdfinfo >/dev/null 2>&1; then
  pages=$(pdfinfo "$build_dir/fostering-cellular-agriculture-one-page.pdf" | awk '/^Pages:/ {print $2}')
  [[ "$pages" == 1 ]] || { printf 'Expected one page; got %s\n' "$pages" >&2; exit 1; }
fi
