section .data
    inputNumber: times 100 db 0      ; 存储输入数据
    binaryTemp: times 200 db 0       ; 存储输入数据的二进制表示（原数据不断除2取余）
    radix: db 0                      ; 存储进制
    outputNumber: times 200 db 0     ; 存储输出结果
    inputLen: db 0                   ; 存储输入长度
    binaryLen: db 0                  ; 存储二进制长度
    binaryFinished: db 0             ; 检查二进制转换是否完成
    outputLen: db 0                  ; 存储输出长度
    ioTemp: db 0                     ; IO临时变量，getchar、putchar的时候先放在这里
    hasError: db 0                   ; 是否有错误
    error: db "Error", 0Ah           ; 错误信息



section .text
    global read_number               ; 读取输入数据
    global read_radix                ; 读取进制转换
    global trans_binary_temp         ; 转换为二进制暂存，需循环执行n次trans_binary_temp_byte
    global trans_output              ; 转换为结果，注意这里仍然是逆序的，但存储的已经是字符'0'、'1'，且包含了0x、0b等前缀
    global put_answer                ; 输出结果
    global exit                      ; 退出程序



getchar:            ; 读一个字节
    push rax        ; 系统调用，要求调用者保存寄存器。但不知道为什么我把这些统一封装成一个mypush函数会段错误……
    push rbx        ; 不知道系统调用用到哪些，所以遇到系统调用时索性全保存了……
    push rcx
    push rdx
    push rsp
    push rbp
    push rsi
    push rdi
    mov rax, 0      ; 调用号0，sys_read
    mov rdi, 0      ; 文件描述符0，从stdin读取数据
    mov rsi, ioTemp ; 缓冲区地址ioTemp，读入数据的存储位置（不加中括号表示地址，加了才表示地址中的内容）
    mov rdx, 1      ; 读取数据的大小（1表示读取1个字节）
    syscall         ; 进行系统调用，参数在上面的几个寄存器中（参数位置是系统约定好的），读取结果在ioTemp中，返回值为成功读取的长度
    cmp byte [ioTemp], 'q'   ; 是否结束
    jne notEOF       ; rax==1，表示成功读取了一个字符（而不是EOF），跳转到结束位置
    call exit
    notEOF:
    pop rdi         ; 恢复寄存器。注意栈是后进先出的！
    pop rsi
    pop rbp
    pop rsp
    pop rdx         
    pop rcx
    pop rbx
    pop rax
    ret



putchar:            ; 写一个字节
    push rax        
    push rbx        
    push rcx
    push rdx
    push rsp
    push rbp
    push rsi
    push rdi
    mov rax, 1      ; 调用号1，sys_write
    mov rdi, 1      ; 文件描述符1，向stdout输出数据
    mov rsi, ioTemp ; 待输出数据地址
    mov rdx, 1      ; 待输出长度
    syscall
    pop rdi         
    pop rsi
    pop rbp
    pop rsp
    pop rdx         
    pop rcx
    pop rbx
    pop rax
    ret



puterror:           ; 输出错误信息
    push rax        
    push rbx        
    push rcx
    push rdx
    push rsp
    push rbp
    push rsi
    push rdi
    mov byte [hasError], 1
    mov rax, 1      ; 也是调用sys_write，只是数据来源不是ioTemp，而是error
    mov rdi, 1
    mov rsi, error
    mov rdx, 6      ; 长度，含换行符
    syscall
    pop rdi         
    pop rsi
    pop rbp
    pop rsp
    pop rdx         
    pop rcx
    pop rbx
    pop rax
    ret



read_number:
    mov rbx, 0                    ; rbx 为当前位置下标/数组长度
    READ_NUMBER:
        call getchar
        cmp byte [ioTemp], '0'    ; 检验范围在0-9之间，否则跳出。
        jl READ_NUMBER_END
        cmp byte [ioTemp], '9'
        jg READ_NUMBER_END
        sub byte [ioTemp], '0'    ; 输入的ASCII码减去'0'，存储的是数字而不是字符数字。
        mov r8b, byte [ioTemp]    ; 由于mov不能把内存里的某个数复制到内存里，所以这里用寄存器r8b(r8的低8位)过渡；后面都统一用r8b来实现过渡
        mov byte [inputNumber + rbx], r8b ; 基址+偏移量，得到当前位置
        add rbx, 1                ; 偏移量自增
        jmp READ_NUMBER
    READ_NUMBER_END:
    mov byte [inputLen], bl    ; rbx的低8位（bl）就是输入的长度
    ret



read_radix:
    call getchar              ; 读取一个字符，判断是否属于这三种
    cmp byte [ioTemp], 'b'
    je READ_RADIX_SAVE
    cmp byte [ioTemp], 'o'
    je READ_RADIX_SAVE
    cmp byte [ioTemp], 'h'
    je READ_RADIX_SAVE
    call puterror             ; 都不符合，调用puterror并跳至结尾 
    jmp READ_RADIX_END
    
    READ_RADIX_SAVE:          ; 符合某一种，跳转到此处保存
    mov r8b, byte [ioTemp]     
    mov byte [radix], r8b
    
    READ_RADIX_END:
    ret



trans_binary_temp:
    mov byte [binaryFinished], 0
    mov rbx, 0         ; 存储二进制表示的位数/当前计算到了第几位
    TRANS_BINARY_NEXT: ; 不断除以2取余，这样的话二进制顺序是反的（这反而更适合转换8进制/16进制）
        call trans_binary_temp_byte
        add rbx, 1
        call trans_binary_temp_check_fininished
        cmp byte [binaryFinished], 1
        jne TRANS_BINARY_NEXT
    mov byte [binaryLen], bl
    ret



trans_binary_temp_check_fininished: ; 辅助函数，检查转换是否完成
    mov cl, byte [inputLen]  ; 将输入长度先放入cl，再零扩展到rcx。
    movzx rcx, cl
    mov rdx, 0
    TEMP_CHECK_NEXT:
        mov r8b, byte [inputNumber+rdx]
        cmp r8b, 0
        jg TEMP_NOT_FINISHED
        add rdx, 1        
        cmp rdx, rcx
        jl TEMP_CHECK_NEXT
    mov byte [binaryFinished], 1
    TEMP_NOT_FINISHED:
    ret



trans_binary_temp_byte:  ; 辅助函数，进行一次除2取余
    mov cl, byte [inputLen]  
    movzx rcx, cl             
    mov rdx, 0 ; rdx存储当前inputNumber的index
    mov rax, 0 ; rax暂存当前位除以2的余数
    
    ; 除2取余，也就是从前到后每个字节都右移一位（商）、每个字节与1做and（余数）退到下一位
    DIV_NEXT_BYTE:
        cmp rax, 1         ; 前一位有余数则当前位+10
        jne DIV_NO_MOD
        mov r8b, byte [inputNumber+rdx]
        add r8b, 10
        mov byte [inputNumber+rdx], r8b
        DIV_NO_MOD:
        mov al, byte [inputNumber+rdx]   ; 当前位除以2取余数     
        movzx rax, al
        and rax, 1
        mov r8b, byte [inputNumber+rdx]  ; 当前位除以2取商并写回，除以2用右移1位(shr)实现
        shr r8b, 1
        mov byte [inputNumber+rdx], r8b  
        add rdx, 1        ; rdx自增，并判断是否到达inputNumber的末尾
        cmp rdx, rcx
        jl DIV_NEXT_BYTE

    mov byte [binaryTemp+rbx], al   ; al即rax的低8位，就是余数，保存
    ret



trans_output:     
    mov r8b, byte [radix]  ; rdx存储目标进制
    cmp r8b, 'b'
    je TO_TRANS_B
    cmp r8b, 'o'
    je TO_TRANS_O
    cmp r8b, 'h'
    je TO_TRANS_H

    TO_TRANS_B:
    call trans_output_b
    jmp BACK_FROM_TRANS_X

    TO_TRANS_O:
    call trans_output_o
    jmp BACK_FROM_TRANS_X

    TO_TRANS_H:
    call trans_output_h
    jmp BACK_FROM_TRANS_X

    BACK_FROM_TRANS_X:
    ret



trans_output_b:
    mov cl, byte [binaryLen]
    movzx rcx, cl
    mov rbx, 0
    TRANS_OUTPUT_B_NEXT:
        mov r8b, byte [binaryTemp+rbx]
        add r8b, '0'      
        mov byte [outputNumber+rbx], r8b
        add rbx, 1
        cmp rbx, rcx
        jl TRANS_OUTPUT_B_NEXT
    mov byte [outputNumber+rbx], 'b'
    add rbx, 1
    mov byte [outputNumber+rbx], '0'
    add rbx, 1
    mov byte [outputLen], bl
    ret


trans_output_o:
    mov cl, byte [binaryLen]
    movzx rcx, cl        ; 二进制长度
    mov rbx, 0           ; 当前的二进制下标
    mov rdx, 0           ; 当前的答案下标
    TRANS_OUTPUT_O_NEXT:
        mov r8b, 0       ; r8b必须初始化！不然下面会出现还没Mov直接add！
        mov r9b, bl      ; 从高到低依次处理连续3位
        movzx r9, r9b    ; binaryTemp+r9而不能加r9b
        add r9, 2
        cmp rcx, r9
        jle O_NEXT_2
        mov r8b, byte [binaryTemp+r9]
        shl r8b, 1       ; 乘2，加上下一位
        O_NEXT_2:
        sub r9, 1
        cmp rcx, r9
        jle O_NEXT_1
        add r8b, byte [binaryTemp+r9]
        shl r8b, 1
        O_NEXT_1:
        sub r9, 1
        add r8b, byte [binaryTemp+r9]
        add r8b, '0'      ; 处理完后转化成正常数字并存回
        mov byte [outputNumber+rdx], r8b
        add rbx, 3        ; 下一组
        add rdx, 1
        cmp rbx, rcx
        jl TRANS_OUTPUT_O_NEXT
    mov byte [outputNumber+rdx], 'o'
    add rdx, 1
    mov byte [outputNumber+rdx], '0'
    add rdx, 1
    mov byte [outputLen], dl
    ret


trans_output_h:
    mov cl, byte [binaryLen]
    movzx rcx, cl        ; 二进制长度
    mov rbx, 0           ; 当前的二进制下标
    mov rdx, 0           ; 当前的答案下标
    TRANS_OUTPUT_H_NEXT:
        mov r8b, 0 
        mov r9b, bl      ; 从高到低依次处理连续4位
        movzx r9, r9b
        add r9, 3
        cmp rcx, r9
        jle H_NEXT_3
        mov r8b, byte [binaryTemp+r9]
        shl r8b, 1       ; 乘2，加上下一位
        H_NEXT_3:
        sub r9, 1
        cmp rcx, r9
        jle H_NEXT_2
        add r8b, byte [binaryTemp+r9]
        shl r8b, 1
        H_NEXT_2:
        sub r9, 1
        cmp rcx, r9
        jle H_NEXT_1
        add r8b, byte [binaryTemp+r9]
        shl r8b, 1
        H_NEXT_1:
        sub r9, 1
        add r8b, byte [binaryTemp+r9]
        cmp r8b, 9        ; 处理完后转化成正常数字并存回（要分大于9和小于等于9两种）
        jg H_BYTE_BIGGER
        add r8b, '0' 
        jmp H_BYTE_FINAL
        H_BYTE_BIGGER:
        add r8b, 87
        H_BYTE_FINAL:
        mov byte [outputNumber+rdx], r8b
        add rbx, 4        ; 下一组
        add rdx, 1
        cmp rbx, rcx
        jl TRANS_OUTPUT_H_NEXT
    mov byte [outputNumber+rdx], 'x'
    add rdx, 1
    mov byte [outputNumber+rdx], '0'
    add rdx, 1
    mov byte [outputLen], dl
    ret
    


put_answer:
    mov bl, byte [outputLen]
    movzx rbx, bl
    PUT_NEXT:
        sub rbx, 1
        mov r8b, byte [outputNumber+rbx]
        mov byte [ioTemp], r8b
        call putchar
        cmp rbx, 0
        jg PUT_NEXT
    mov byte [ioTemp], 0Ah
    call putchar
    ret



exit:
    mov rax, 60     ; 调用号60和231都可以退出，60只退出当前线程，231可以退出所有线程（本次作业不涉及多线程，无所谓用哪个）
    mov rdi, 0      ; 状态码0，正常退出
    syscall