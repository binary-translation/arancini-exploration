.globl _start
.type _start,%function
_start:
repe cmpsb
.size _start,.-_start
