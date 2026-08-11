#!/bin/bash
# Optional env variables:
# COMMANDER: Path to Simplicity Commander binary (default: commander)

# make invokes this as a post-build step, so it defaults COMMANDER of its own
COMMANDER=${COMMANDER:-commander}

BUILD_OUTPUT=build/release/nc_controller_soc_repeater.hex
OUTFILE=artifact/zwa2_repeater.gbl
SIGN_KEY=keys/vendor_sign.key
ENC_KEY=keys/vendor_encrypt.key

mkdir -p artifact

$COMMANDER gbl create $OUTFILE --app $BUILD_OUTPUT --sign $SIGN_KEY --encrypt $ENC_KEY --compress lzma
