# Generating linker scripts

## The linker scripts were generated using the following approach:

- Run txlat using the `--verbose-link` and `--no-script` options
- Extract the default linker script from the output between the `=============` delimiters and save it.
    - (For example using the command `sed sed -e '/^=========/,/^=========/!d;/^=========/d'`)
- Run `scripts/linkerscript/gen_linker_script.sh <binary> <binary.lds>` with the generated binary and the saved linker script as inputs. 
  This will generate the file `<binary.lds>.new`.
- Add `INCLUDE "guest-sections.lds"` as the first line in `SECTIONS` and `*(.grela)` as the first line in `.rela.dyn`