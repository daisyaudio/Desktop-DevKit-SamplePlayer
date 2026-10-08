#!/bin/bash
set -e

# Ensure everything is run from the root of the repo, and check contents
# incase it's invoked elsewhere.
cd "$(dirname "${BASH_SOURCE[0]}")/.."
if [ ! -d .git ] || [ ! -x ci/build_libs.sh ] || [ ! -d Blink ]; then
    echo "error: not in the DaisyBlinkProject repo root; aborting." >&2
    exit 1
fi

TEMP_ROOT=../temp
TEMP_NAME=DaisyBlinkProject
TEMP_DEST="$TEMP_ROOT/$TEMP_NAME"
ZIP_NAME=DaisyBlinkProject-compiled.zip
ZIP_DEST="../$ZIP_NAME"

# Create a new temp folder outside of this repo if it does not exist
echo "creating temp directory if it doesn't exist..."
mkdir -p "$TEMP_ROOT"

# Remove an existing compiled version of this directory, and the zip
echo "removing build from previous run..."
if [ -e "$TEMP_DEST" ]; then rm -r "$TEMP_DEST"; fi
if [ -e "$ZIP_DEST" ]; then rm -r "$ZIP_DEST"; fi

# Make sure everything here is built:
echo "building libraries..."
./ci/build_libs.sh
echo "building Blink..."
make -C Blink clean && make -C Blink
echo "done."

echo "copying compiled build to temp directory for git-stripping..."
cp -R . "$TEMP_DEST"

echo "stripping all git files from temp directory..."
find "$TEMP_DEST" -name ".git*" -prune -exec rm -rf {} +
echo "done."

echo "archiving compiled version to zip file..."
(cd "$TEMP_ROOT" && zip -r -q "../$ZIP_NAME" "$TEMP_NAME")

echo "cleaning up temp directory..."
rm -rf "$TEMP_DEST"
# only delete the actual `temp/` folder if it is empty
if [ -d "$TEMP_ROOT" ] && [ -z "$(find "$TEMP_ROOT" -mindepth 1 -maxdepth 1 -print -quit)" ]; then
    rmdir "$TEMP_ROOT"
fi
echo "done."
echo "see $ZIP_NAME located outside of this folder."
