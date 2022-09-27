.text
.align 16
.type call_native, %function
.globl call_native
call_native:
    mov $1, %eax
    ret
.size call_native,.-call_native
