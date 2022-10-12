.text
.align 16
.type call_native, %function
.globl call_native
call_native:
    mov x0, #1
    ret
.size call_native,.-call_native
