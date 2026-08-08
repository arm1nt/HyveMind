#!/bin/bash

set -e

FILE_NAME=$1
ABS_SRCTREE=$2
ABS_SOURCE_FILE=$3
REL_TARGET_DIR=$4

REL_TARGET_FILE="$REL_TARGET_DIR/$FILE_NAME"
ABS_TARGET_FILE="$ABS_SRCTREE/$REL_TARGET_DIR/$FILE_NAME"

cleaned_guard_name=$(echo $REL_TARGET_FILE | tr '[:lower:]' '[:upper:]' | tr '/.-' '___')
include_guard="_HYVEMIND_$cleaned_guard_name"

echo "#ifndef $include_guard" > $ABS_TARGET_FILE
echo "#define $include_guard" >> $ABS_TARGET_FILE
echo "" >> $ABS_TARGET_FILE
echo "/* DO NOT MODIFY. HEADER IS AUTO-GENERATED */" >> $ABS_TARGET_FILE
echo "" >> $ABS_TARGET_FILE
gawk '/.ascii "-->> / { print $3, $4, $5 }' $ABS_SOURCE_FILE >> $ABS_TARGET_FILE
echo "" >> $ABS_TARGET_FILE
echo "#endif /* $include_guard */" >> $ABS_TARGET_FILE

