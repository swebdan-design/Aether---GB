#!/bin/sh
# AETHER - by Synth Centric : assemble the release/ directory.
set -e
cd "$(dirname "$0")/.."
mkdir -p release/demo_audio build/pdf
cp build/AETHER_by_Synth_Centric.gb release/AETHER_by_Synth_Centric.gb

# PDF manual (pandoc -> HTML -> Chromium)
if command -v pandoc >/dev/null 2>&1 && node -e "require('playwright')" >/dev/null 2>&1; then
    pandoc docs/AETHER_Manual.md -s --toc --toc-depth=2 --metadata lang=en \
        --css ../../tools/manual.css -o build/pdf/AETHER_Manual.html
    node tools/html2pdf.js build/pdf/AETHER_Manual.html release/AETHER_Manual.pdf
    echo "manual: release/AETHER_Manual.pdf"
else
    echo "pandoc or Playwright missing: PDF manual not generated (Markdown manual is in docs/)"
fi

# source archive of the committed tree (excludes release/ binaries and build output)
rm -f release/AETHER_Source.zip
git archive --format=zip --prefix=AETHER/ -o release/AETHER_Source.zip HEAD -- \
    README.md Makefile .gitignore src include assets tests tools docs
echo "source: release/AETHER_Source.zip"
ls -la release release/demo_audio
