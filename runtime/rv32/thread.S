.text
.global __create_threads
.global __join_threads
/*
For system call ABI, see https://man7.org/linux/man-pages/man2/syscall.2.html
*/

/*
Raw system call interface varies on different architectures for clone,
but the manual page (https://man7.org/linux/man-pages/man2/clone.2.html) didn't
mention risc-v. By looking into the kernel source, I figure out that it is
long clone(unsigned long flags, void *stack,
                     int *parent_tid, unsigned long tls,
                     int *child_tid);

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
SYS_clone = 220
CLONE_VM = 256
SIGCHLD = 17
__create_threads:
    addi a0, a0, -1
    ble a0, zero, .ret_0
    mv a6, a0
    li a5, 0
    mv a1, sp
    li a2, 0
    li a3, 0
    li a4, 0
.L0:
    li a0, (CLONE_VM | SIGCHLD)
    li a7, SYS_clone
    ecall
    bne a0, zero, .ret_i
    addi a5, a5, 1
    blt a5, a6, .L0
.ret_n:
    mv a0, a6
    j .L1
.ret_0:
    mv a0, zero
    j .L1
.ret_i:
    mv a0, a5
.L1:
    jr ra

/*
Note that it depends on an inconsistent feature between linux and POSIX,
see section BUGS at https://man7.org/linux/man-pages/man2/wait.2.html
But since it already depends on so many features of linux, like the raw
syscall number, so never mind.
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
SYS_waitid = 95
SYS_exit = 93
P_ALL = 0
WEXITED = 4
__join_threads:
    mv a4, a0
    addi a5, a1, -1
    beq a4, a5, .L2
    li a0, P_ALL
    li a1, 0
    li a2, 0
    li a3, WEXITED
    li a7, SYS_waitid
    ecall
.L2:
    beq a4, zero, .L3
    li a0, 0
    li a7, SYS_exit
    ecall
.L3:
    jr ra
