
#How to Implement a Translator for an x86 Instruction

For each instruction in the input machine code, it uses a factory pattern
to get a translator specific for that category of instruction, which
is then used to translate the instruction to the Arancini IR.
Each instruction is translated into a packet, which is then added to
the output chunk.


The translator factory is implemented by the `get_translator` function,
which is located in the file `src/input/x86/x86-input-arch.cpp`.
The example bellow shows a snippet of the `get_translator` function
where binary-operation instructions are handled.
```cpp
static std::unique_ptr<translator> get_translator(off_t address, xed_decoded_inst_t *xed_inst)
{
	switch (xed_decoded_inst_get_iclass(xed_inst)) {
        ...
	case XED_ICLASS_XOR:
	case XED_ICLASS_AND:
	case XED_ICLASS_OR:
	case XED_ICLASS_ADD:
	case XED_ICLASS_ADC:
	case XED_ICLASS_SUB:
	case XED_ICLASS_SBB:
	case XED_ICLASS_CMP:
	case XED_ICLASS_TEST:
		return std::make_unique<binop_translator>();

        ...
}
```

All the x86 translators implementations can be found in the
`src/input/x86/translators/` folder.

