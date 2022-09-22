
# How to Implement a Translator for an x86 Instruction

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

All the x86 translators are defined in the `inc/arancini/input/x86/translators/translators.h`
header file, and their implementations can be found in the
`src/input/x86/translators/` folder.
For example, the translator for all binary-operation instructions is implemented in
the `src/input/x86/translators/binop.cpp` file.
This file contains the implementation of the virtual function 
`binop_translator::do_translate()`.
The class `binop_translator` extends the abstract `translator` class, which defines the
virtual `do_translate()` function.
The `do_translate()` function is called by the `translator::translate()` function.
Combined, they form the primary interface used for translating individual x86 instructions
into packets in the Arancini IR.
The snippet of code bellow shows how the `binop_translator` translates an addition instruction.
```cpp
void binop_translator::do_translate()
{
	auto op0 = read_operand(0);
	auto op1 = auto_cast(op0->val().type(), read_operand(1));

	value_node *rslt;

	switch (xed_decoded_inst_get_iclass(xed_inst())) {
        ...
	case XED_ICLASS_ADD:
		rslt = pkt()->insert_add(op0->val(), op1->val());
		break;
        ...
        }
        ...
}

```

