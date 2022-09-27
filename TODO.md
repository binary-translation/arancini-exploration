
# Translation of x86 Instructions to Arancini IR:

## Floating-point operations (SSE-based)

* movaps
Example:
```asm
movaps xmm1, xmm0
```

## AVX, AVX2, AVX-512 

* vmovsd   
Example:   
```asm
vmovsd xmm0, qword ptr [rip+0xbf]
```
* vsubps  
Example:   
```asm
vsubps ymm0, ymm0, ymm1
```
