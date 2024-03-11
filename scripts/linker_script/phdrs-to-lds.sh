#!/bin/bash

readelf -Wl "$1" | \
sed -ne '/^Program Headers/,/^ Section to Segment mapping/ p' | grep -v '[.*]' | \
tail -n+3 | head -n-2 | sed 's/^[[:blank:]]*//' | tr -s '[:blank:]' '\t' | \
(ctr=0; while read pt_type pt_offset pt_vaddr pt_paddr pt_filesz pt_memsz flags_and_align; do
	align=$( echo "$flags_and_align" | sed 's/.*\(0x.*\)/\1/' )
	flags=$( echo "$flags_and_align" | sed 's/\(.*\)0x.*/\1/' | tr -cd RWE )
    echo "flags for type $pt_type offset $pt_offset is $flags" 1>&2
	if [[ $pt_type == "PHDR" ]]; then
		phdr_keyword="PHDRS"; else phdr_keyword=""
	fi;
	if [[ $pt_type == "GNU_STACK" ]]; then
	  flags_keyword="FLAGS(6)"; else  flags_keyword=""
  fi;
  if [[ $pt_type != "RISCV_ATTRIBUT" ]]; then
	echo "phdr$ctr PT_$pt_type $phdr_keyword $flags_keyword; "
  fi;
ctr=$(( $ctr + 1 )); done) | sed '/^[[:blank:]]*$/ d'