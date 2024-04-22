sha256sum lab1_test.py
nasm -f elf64 trans.asm -o trans.o
ld trans.o -o trans
python3 lab1_test.py ./trans
