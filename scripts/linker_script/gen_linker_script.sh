#!/bin/bash

export LC_ALL=C

# HACK around strange linker behaviour (placing orphan .note.ABI-tag at weirdly high offset but low address)
#sed -i '/^ *\.note\.gnu\.build-id *: *{ \*(\.note\.gnu\.build-id) }.*/ s/^\( *\.note\)\(\.gnu\.build-id\)\( *: *{ \*(\.note\)\(\.gnu\.build-id\)\() }.*\)/\1.ABI-tag\3.ABI-tag\5\n&/' "$2"
#sed -ri 's/^(\s*\.note)(\.gnu\.build-id)(\s+:\s+\{ \*\(\.note)(\.gnu\.build-id)(\) \}.*)/\1.ABI-tag\3.ABI-tag\5\n&/' "$2"


(./phdrs-to-lds.sh "$1" && ./phdrs-to-m4.sh "$1") > $1.phdrs.inc

cat "$2" | sed "s#SECTIONS#PHDRS\n{\n\tinclude("$1.phdrs.inc")\n\tinclude(gphdr.inc)\n}\nSECTIONS#" |
 sed -r "s/(SEGMENT_START\(\"text-segment\", )([^\)]*\))/\1ALIGN\(CONSTANT\(MAXPAGESIZE\)\)\)/" | \
tr '\n' '\f' | sed -r \
's@(\.[-a-zA-Z0-9\._]+)[[:space:]\f]+(([^\{]*[[:space:]\f]*)?\{[^\}]*\})@expand_phdr([\1], [\2])@g' | \
tr '\f' '\n' | m4 > "$2.new"

