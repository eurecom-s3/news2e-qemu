.text
.global bit_scan_forward_64
.global bit_scan_forward_64_posix
.global s2e_setjmp_win32
.global s2e_longjmp_win32
.global s2e_setjmp_posix
.global s2e_longjmp_posix

/* int bit_scan_forward_64(uint64_t *SetIndex, uint64_t Mask);
   RCX first parameter, RDX second parameter */
bit_scan_forward_64_win32:
    xor %rax, %rax
    bsf %rdx, %rdx
    mov %rdx, (%rcx)
    setnz %al
    ret


bit_scan_forward_64_posix:
    xor %rax, %rax
    bsf %rsi, %rsi
    mov %rsi, (%rdi)
    setnz %al
    ret


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

s2e_setjmp_common:
    mov %rax, (%rcx)
    mov %rbx, 8(%rcx)
    mov %rdx, 24(%rcx)
    mov %rsi, 32(%rcx)
    mov %rdi, 40(%rcx)
    mov %rbp, 48(%rcx)
    mov %rsp, 56(%rcx)
    mov %r8,  64(%rcx)
    mov %r9,  72(%rcx)
    mov %r10, 80(%rcx)
    mov %r11, 88(%rcx)
    mov %r12, 96(%rcx)
    mov %r13, 104(%rcx)
    mov %r14, 112(%rcx)
    mov %r15, 120(%rcx)
    mov (%rsp), %rax
    mov %rax, 128(%rcx)
    xor %rax, %rax
    ret

s2e_longjmp_common:
    /* RAX is clobbered in the end and does not need to be restored */
    mov 8(%rax), %rbx
    mov 48(%rax), %rbp
    mov 56(%rax), %rsp
    mov 64(%rax), %r8  
    mov 72(%rax), %r9
    mov 80(%rax), %r10
    mov 88(%rax), %r11 
    mov 96(%rax), %r12 
    mov 104(%rax), %r13 
    mov 112(%rax), %r14 
    mov 120(%rax), %r15 
    mov 128(%rax), %rax
    ret


/* RCXis pointer to jmp_buf */
s2e_setjmp_win32:
    mov %rcx, 16(%rcx)
    jmp s2e_setjmp_common

/* 
 * RCX - pointer to jmp_buf.
 * RDX -  value.
 */
s2e_longjmp_win32:
    mov %rcx, %rax 
    call s2e_longjmp_common
    mov %rax, (%rsp)
    mov 32(%rcx), %rsi
    mov 40(%rcx), %rdi
    mov $1, %rax
    cmp $0, %rdx
    cmovnz %rdx, %rax
    ret

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

/*
 * RDI - pointer to jmp_buf.
 */
s2e_setjmp_posix:
    mov %rcx, 16(%rdi)
    mov %rdi, %rcx
    jmp s2e_setjmp_common

/*
 * RCX - pointer to jmp_buf.
 * RDX - value.
 */
s2e_longjmp_posix:
    mov %rdi, %rax
    call s2e_longjmp_common
    mov %rax, (%rsp)
    mov 16(%rax), %rcx
    mov 24(%rax), %rdx
    mov $1, %rax
    cmp $0, %rsi
    cmovnz %rsi, %rax
    ret
