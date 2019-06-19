#include<stdio.h>

void main(){
   
        asm ("mov $0x0000, %rax");
        asm ("mov $0x1111, %rcx");
        asm ("mov $0x2222, %rdx");
        asm ("mov $0x3333, %rbx");
        //asm ("mov $0x4444, %rsp");
        asm ("mov $0x5555, %rbp");
        asm ("mov $0x6666, %rsi");
        asm ("mov $0x7777, %rdi");
        asm ("mov $0x8888, %r8");
        asm ("mov $0x9999, %r9");
        asm ("mov $0x1122, %r10");
        asm ("mov $0x3344, %r11");
        asm ("mov $0x5566, %r12");
        asm ("mov $0x7788, %r13");
        asm ("mov $0x9900, %r14");
        asm ("mov $0x2211, %r15");
	
}

