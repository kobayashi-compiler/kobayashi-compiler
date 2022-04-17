.arch armv7ve
.arm
.section .text
/*
In our auto parallelization, all threads use the same stack. So these
two functions need to manipulate the stack in a tricky way and use inline
system call.
For system call ABI, see https://man7.org/linux/man-pages/man2/syscall.2.html
*/

/*
Raw system call clone interface for arm:
long clone(unsigned long flags, void *stack,
                     int *parent_tid, unsigned long tls,
                     int *child_tid);
For detail of clone, see https://man7.org/linux/man-pages/man2/clone.2.html
SYS_clone = 120
int __create_threads(int n) {
    --n;
    if (n <= 0) {
        return 0;
    }
    for (int i = 0; i < n; ++i) {
        int pid = clone(CLONE_VM | SIGCHLD, sp, 0, 0, 0);
        if (pid != 0) {
            return i;
        }
    }
    return n;
}
*/
__create_threads:
	push {r4, r5, r6, r7}
	sub r5, r0, #1
	cmp r5, #0
	movle r0, #0
	ble .L1
	mov r7, #120
	mov r1, sp
	mov r2, #0
	mov r3, #0
	mov r4, #0
	mov r6, #0
.L0:
	mov r0, #273
	swi #0
	cmp r0, #0
	movne r0, r6
	bne .L1
	add r6, r6, #1
	cmp r6, r5
	bne .L0
	mov r0, r5
.L1:
	pop {r4, r5, r6, r7}
	bx lr

/*
SYS_waitid = 280
SYS_exit = 1
P_ALL = 0
WEXITED = 4
sizeof(siginfo_t) = 128
void __join_threads(int i, int n) {
    --n;
    siginfo_t tmp;
    if (i != n) {
        waitid(P_ALL, 0, &tmp, WEXITED);
    }
    if (i != 0) {
        _exit(0);
    }
}
*/
__join_threads:
	sub sp, sp, #(16+128) // in __create_threads, four save registers use 16 bytes.
	sub sp, sp, r0, LSL #7
    push {r4}
    mov r4, r0
	sub r1, r1, #1
	cmp r4, r1
	beq .L2
	mov r0, #0
    mov r1, #0
    add r2, sp, #4
    mov r3, #4
    mov r7, #280
    swi #0
.L2:
	cmp r4, #0
	bne .L3
	pop {r4}
	add sp, sp, #(16+128)
	bx lr
.L3:
	mov r0, #0
    mov r7, #1
	swi #0