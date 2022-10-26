
# Translation of x86 Instructions to Arancini IR:

## Floating-point operations (SSE-based)

* vaddss
Example `test/kernels/fp/addss.c`:
```asm
vaddss xmm0, xmm0, xmm1
```
* vaddsd
Example `test/kernels/fp/addsd.c`:
```asm
vaddsd xmm0, xmm0, xmm1
```
* vsubss
Example `test/kernels/fp/subss.c`:
```asm
vsubss xmm0, xmm0, xmm1
```
* vsubsd
Example `test/kernels/fp/subsd.c`:
```asm
vsubsd xmm0, xmm0, xmm1
```
* vmulss
Example `test/kernels/fp/mulss.c`:
```asm
vmulss xmm0, xmm0, xmm1
```
* vmulsd
Example `test/kernels/fp/mulsd.c`:
```asm
vmulsd xmm0, xmm0, xmm1
```
* vdivss
Example `test/kernels/fp/divss.c`:
```asm
vdivss xmm0, xmm0, xmm1
```
* vdivsd
Example `test/kernels/fp/divsd.c`:
```asm
vdivsd xmm0, xmm0, xmm1
```
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
