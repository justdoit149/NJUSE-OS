%include "./help.asm"

section .text
    global _start

_start:
    NEXT_NUMBER:
        ; 初始化
        mov byte [hasError], 0  ; 是否有错误，初始化为无错误

        ; 输入
        call read_number        
        call read_radix         ; read_number读取非数字后才返回，即中间的空白符已被读取，直接读进制即可  
        cmp byte [hasError], 1  ; 若有错误，则跳过处理过程
        je TRANS_FINISHED
        
        ; 处理
        call trans_binary_temp  ; 先转成二进制，注意这里是逆序的
        call trans_output       ; 再转成结果，仍然是逆序

        ; 输出，注意是逆序输出数组
        call put_answer

        ; 检查是否到文件末尾（TODO遇见q在getchar里处理了）
        TRANS_FINISHED:
        call getchar
        cmp byte [ioTemp], -1
        jne NEXT_NUMBER
    call exit