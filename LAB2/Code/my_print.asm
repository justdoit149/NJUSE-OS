; 这几行汇编直接把现成的代码拿来用了ww
section .text
    global myPrint

slen:
    push    ebx
    mov     ebx, eax

nextchar:
    cmp     byte[eax], 0
    jz      finished
    inc     eax
    jmp     nextchar

finished:
    sub     eax, ebx 
    pop     ebx
    ret

myPrint:
    mov     eax, [esp + 4]   
    push    eax
    call    slen
    mov     edx, eax       
    pop     eax
    mov     ecx, eax
    mov     ebx, 1        
    mov     eax, 4
    int     80h
    ret
