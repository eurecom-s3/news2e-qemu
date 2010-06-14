


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
[bits 32]
s2e_timing:
push ebp
mov ebp, esp

mov eax, [ebp - 4]
mov edx, [ebp - 8]

db 0x0f
db 0x3f ; S2EOP

db 0x00 ; Built-in instructions
db 0x04 ; Insert timing
db 0x00
db 0x00
dd 0x0

leave
ret

s2e_get_path_id:
push ebp
mov ebp, esp

db 0x0f
db 0x3f ; S2EOP

db 0x00 ; Built-in instructions
db 0x04 ; Get path id
db 0x00
db 0x00
dd 0x0

leave
ret

s2e_enable:
push ebp
mov ebp, esp

db 0x0f
db 0x3f ; S2EOP

db 0x00 ; Built-in instructions
db 0x01 ; Enable symbex
db 0x00
db 0x00
dd 0x0

leave
ret

s2e_disable:
push ebp
mov ebp, esp

db 0x0f
db 0x3f ; S2EOP

db 0x00 ; Built-in instructions
db 0x02 ; Disable symbex
db 0x00
db 0x00
dd 0x0

leave
ret

s2e_make_symbolic:
push ebp
mov ebp, esp
push ebx

mov eax, [ebp + 0x8] ;address
mov ebx, [ebp + 0xC] ;size
mov ecx, [ebp + 0x10] ;asciiz

db 0x0f
db 0x3f ; S2EOP

db 0x00 ; Built-in instructions
db 0x03 ; Make symbolic
db 0x00
db 0x00
dd 0x0

pop ebx
leave
ret


s2e_kill_state:
push ebp
mov ebp, esp
push ebx

db 0x0f
db 0x3f ; S2EOP

db 0x00 ; Built-in instructions
db 0x06 ; Kill the current state
db 0x00
db 0x00
dd 0x0

pop ebx
leave
ret


s2e_print_expression:
push ebp
mov ebp, esp

mov eax, [ebp + 0x8] ;expression
mov ecx, [ebp + 0xC] ;message

db 0x0f
db 0x3f ; S2EOP

db 0x00 ; Built-in instructions
db 0x07 ; print expression
db 0x00
db 0x00
dd 0x0

leave
ret

s2e_print_memory:
push ebp
mov ebp, esp
push ebx

mov eax, [ebp + 0x8] ;addr
mov ebx, [ebp + 0xC] ;size
mov ecx, [ebp + 0xC] ;message

db 0x0f
db 0x3f ; S2EOP

db 0x00 ; Built-in instructions
db 0x08 ; print memory
db 0x00
db 0x00
dd 0x0

pop ebx
leave
ret

s2e_int:
push ebp
mov ebp, esp
sub esp, 4

push 0
push 4
lea eax, [ebp-4]
push eax
call s2e_make_symbolic
add esp, 4*3
mov eax, [ebp-4]

leave
ret




