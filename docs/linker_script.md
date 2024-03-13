# Generating linker scripts

## The linker scripts were generated using the following approach:

- Run `txlat` using the `--verbose-link` and `--no-script` options, along with a
  binary specified in the `--input` and an output binary specified with
  `--output`. Note that specifying `--no-static` has no effect on the generation
  of a linker script.

- Extract the default linker script from the output between the `=============` delimiters and save it.
    - (For example using the command `sed sed -e '/^=========/,/^=========/!d;/^=========/d'`)

- Run `scripts/linker_script/gen_linker_script.sh <binary> <binary.lds>` with the generated binary and the saved linker script as inputs. 
  This will generate the file `<binary.lds>.new`, which will become the final
  linker script after some additional modifications.

- Add line `INCLUDE "guest-sections.lds"` as the first line in `SECTIONS` and
  `*(.grela)` as the first line in `.rela.dyn` in `<binary.lds>.new`.

- If you want a script for a shared library, you need to change the order of the
  `phdrs` so the added `gphdr` are first. Otherwise, unsorted `phdr` will break
  loading. Change the order of the `phdrs` so the added `gphdr` are first (after
  `PT_INTERP` for executables). Essentially, the `gphdr0 PT_LOAD ;` and all other
  `gphdr*` lines should follow `phdr1 PT_INTERP ;`

- Add PHDRS and FILEHDR to gphdr0 if the script is for an executable.

