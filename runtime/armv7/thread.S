.arch armv7ve
.arm
.section .text
.global __create_threads
.global __join_threads
/*
In our auto parallelization implementation, all threads use the same stack. So
these two functions need to manipulate the stack in a tricky way and use inline
system call.
For system call ABI, see https://man7.org/linux/man-pages/man2/syscall.2.html
*/

/*
Raw system call clone interface for arm:
long clone(unsigned long flags, void *stack,
                     int *parent_tid, unsigned long tls,
                     int *child_tid);
For detail of clone, see https://man7.org/linux/man-pages/man2/clone.2.html
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
SYS_clone = 120
CLONE_VM = 256
SIGCHLD = 17
__create_threads:
    push {r4, r5, r6, r7}
    sub r5, r0, #1
    cmp r5, #0
    movle r0, #0
    ble .L1
    mov r7, #SYS_clone
    mov r1, sp
    mov r2, #0
    mov r3, #0
    mov r4, #0
    mov r6, #0
.L0:
    mov r0, #(CLONE_VM | SIGCHLD)
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
Note that it depends on an inconsistent feature between linux and POSIX,
see section BUGS at https://man7.org/linux/man-pages/man2/wait.2.html
But since it already depends on so many features of linux, like the raw
syscall number, so don't mind it.
void __join_threads(int i, int n) {
    --n;
    if (i != n) {
        waitid(P_ALL, 0, NULL, WEXITED);
    }
    if (i != 0) {
        _exit(0);
    }
}
*/
SYS_waitid = 280
SYS_exit = 1
P_ALL = 0
WEXITED = 4
__join_threads:
    sub sp, sp, #16 // to avoid memory overlapping with __create_threads
    cmp r0, #0
    pusheq {r4, r7}
    mov r4, r0
    sub r1, r1, #1
    cmp r4, r1
    beq .L2
    mov r0, #P_ALL
    mov r1, #0
    mov r2, #0
    mov r3, #WEXITED
    mov r7, #SYS_waitid
    swi #0
.L2:
    cmp r4, #0
    bne .L3
    pop {r4, r7}
    add sp, sp, #16
    bx lr
.L3:
    mov r0, #0
    mov r7, #SYS_exit
    swi #0

