#!/bin/bash
# Required env variables:
# COMMANDER: Path to Simplicity Commander binary

BUILD_OUTPUT=build/release/nc_controller_soc_repeater.hex
OUTFILE=artifact/zwa2_repeater.gbl
SIGN_KEY=keys/vendor_sign.key
ENC_KEY=keys/vendor_encrypt.key

mkdir -p artifact

$COMMANDER gbl create $OUTFILE --app $BUILD_OUTPUT --sign $SIGN_KEY --encrypt $ENC_KEY --compress lzma