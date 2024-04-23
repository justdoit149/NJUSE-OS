section .text
    global myPrint

myPrint:
    mov eax, 4        ; 设置系统调用号为4（write）
    mov ebx, 1        ; 文件描述符为1（标准输出）
    mov ecx, [esp + 8]; 字符串地址
    mov edx, [esp + 4]; 字符串长度
    int 80h           ; 调用系统调用
    ret