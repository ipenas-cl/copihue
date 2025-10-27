; ===========================================================================
; TEMPO SYSCALLS - Definiciones multiplataforma
; ===========================================================================
; Syscalls comunes para AtomicOS y desarrollo del compilador
; Author: Ignacio Peña Sepúlveda
; Date: June 26, 2025
; ===========================================================================

%ifdef PLATFORM_LINUX
    ; Linux x86-64 syscalls
    %define SYS_read        0
    %define SYS_write       1
    %define SYS_open        2
    %define SYS_close       3
    %define SYS_stat        4
    %define SYS_fstat       5
    %define SYS_lstat       6
    %define SYS_poll        7
    %define SYS_lseek       8
    %define SYS_mmap        9
    %define SYS_mprotect    10
    %define SYS_munmap      11
    %define SYS_brk         12
    %define SYS_ioctl       16
    %define SYS_access      21
    %define SYS_pipe        22
    %define SYS_select      23
    %define SYS_sched_yield 24
    %define SYS_mremap      25
    %define SYS_msync       26
    %define SYS_mincore     27
    %define SYS_madvise     28
    %define SYS_shmget      29
    %define SYS_shmat       30
    %define SYS_shmctl      31
    %define SYS_dup         32
    %define SYS_dup2        33
    %define SYS_pause       34
    %define SYS_nanosleep   35
    %define SYS_getitimer   36
    %define SYS_alarm       37
    %define SYS_setitimer   38
    %define SYS_getpid      39
    %define SYS_socket      41
    %define SYS_connect     42
    %define SYS_accept      43
    %define SYS_sendto      44
    %define SYS_recvfrom    45
    %define SYS_sendmsg     46
    %define SYS_recvmsg     47
    %define SYS_shutdown    48
    %define SYS_bind        49
    %define SYS_listen      50
    %define SYS_fork        57
    %define SYS_vfork       58
    %define SYS_execve      59
    %define SYS_exit        60
    %define SYS_wait4       61
    %define SYS_kill        62
    %define SYS_fcntl       72
    %define SYS_flock       73
    %define SYS_fsync       74
    %define SYS_fdatasync   75
    %define SYS_truncate    76
    %define SYS_ftruncate   77
    %define SYS_getdents    78
    %define SYS_getcwd      79
    %define SYS_chdir       80
    %define SYS_fchdir      81
    %define SYS_rename      82
    %define SYS_mkdir       83
    %define SYS_rmdir       84
    %define SYS_creat       85
    %define SYS_link        86
    %define SYS_unlink      87
    %define SYS_symlink     88
    %define SYS_readlink    89
    %define SYS_chmod       90
    %define SYS_fchmod      91
    %define SYS_chown       92
    %define SYS_fchown      93
    %define SYS_lchown      94
    %define SYS_umask       95
    %define SYS_gettimeofday 96
    %define SYS_getrlimit   97
    %define SYS_getrusage   98
    %define SYS_sysinfo     99
    %define SYS_times       100
    %define SYS_getuid      102
    %define SYS_getgid      104
    %define SYS_setuid      105
    %define SYS_setgid      106
    %define SYS_geteuid     107
    %define SYS_getegid     108
    %define SYS_setpgid     109
    %define SYS_getppid     110
    %define SYS_getpgrp     111
    %define SYS_setsid      112
    %define SYS_setreuid    113
    %define SYS_setregid    114
    %define SYS_utime       132
    %define SYS_mknod       133
    %define SYS_personality 135
    %define SYS_statfs      137
    %define SYS_fstatfs     138
    %define SYS_getpriority 140
    %define SYS_setpriority 141
    %define SYS_sched_setparam 142
    %define SYS_sched_getparam 143
    %define SYS_sched_setscheduler 144
    %define SYS_sched_getscheduler 145
    %define SYS_sched_get_priority_max 146
    %define SYS_sched_get_priority_min 147
    %define SYS_mlock       149
    %define SYS_munlock     150
    %define SYS_mlockall    151
    %define SYS_munlockall  152
    %define SYS_prctl       157
    %define SYS_sync        162
    %define SYS_mount       165
    %define SYS_umount2     166
    %define SYS_gettid      186
    %define SYS_tkill       200
    %define SYS_time        201
    %define SYS_futex       202
    %define SYS_set_thread_area 205
    %define SYS_get_thread_area 211
    %define SYS_io_setup    206
    %define SYS_io_destroy  207
    %define SYS_io_getevents 208
    %define SYS_io_submit   209
    %define SYS_io_cancel   210
    %define SYS_lookup_dcookie 212
    %define SYS_epoll_create 213
    %define SYS_remap_file_pages 216
    %define SYS_set_tid_address 218
    %define SYS_timer_create 222
    %define SYS_timer_settime 223
    %define SYS_timer_gettime 224
    %define SYS_timer_getoverrun 225
    %define SYS_timer_delete 226
    %define SYS_clock_settime 227
    %define SYS_clock_gettime 228
    %define SYS_clock_getres 229
    %define SYS_clock_nanosleep 230
    %define SYS_exit_group  231
    %define SYS_tgkill      234
    %define SYS_utimes      235
    %define SYS_mbind       237
    %define SYS_set_mempolicy 238
    %define SYS_get_mempolicy 239
    %define SYS_mq_open     240
    %define SYS_mq_unlink   241
    %define SYS_mq_timedsend 242
    %define SYS_mq_timedreceive 243
    %define SYS_mq_notify   244
    %define SYS_mq_getsetattr 245
    %define SYS_kexec_load  246
    %define SYS_waitid      247
    %define SYS_add_key     248
    %define SYS_request_key 249
    %define SYS_keyctl      250
    %define SYS_ioprio_set  251
    %define SYS_ioprio_get  252
    %define SYS_inotify_init 253
    %define SYS_inotify_add_watch 254
    %define SYS_inotify_rm_watch 255
    %define SYS_openat      257
    %define SYS_mkdirat     258
    %define SYS_mknodat     259
    %define SYS_fchownat    260
    %define SYS_futimesat   261
    %define SYS_newfstatat  262
    %define SYS_unlinkat    263
    %define SYS_renameat    264
    %define SYS_linkat      265
    %define SYS_symlinkat   266
    %define SYS_readlinkat  267
    %define SYS_fchmodat    268
    %define SYS_faccessat   269
    %define SYS_pselect6    270
    %define SYS_ppoll       271
    %define SYS_unshare     272
    %define SYS_splice      275
    %define SYS_tee         276
    %define SYS_sync_file_range 277
    %define SYS_vmsplice    278
    %define SYS_utimensat   280
    %define SYS_epoll_pwait 281
    %define SYS_timerfd_create 283
    %define SYS_eventfd     284
    %define SYS_fallocate   285
    %define SYS_timerfd_settime 286
    %define SYS_timerfd_gettime 287
    %define SYS_accept4     288
    %define SYS_signalfd4   289
    %define SYS_eventfd2    290
    %define SYS_epoll_create1 291
    %define SYS_dup3        292
    %define SYS_pipe2       293
    %define SYS_inotify_init1 294
    %define SYS_preadv      295
    %define SYS_pwritev     296
    %define SYS_perf_event_open 298
    %define SYS_recvmmsg    299
    %define SYS_fanotify_init 300
    %define SYS_fanotify_mark 301
    %define SYS_prlimit64   302
    %define SYS_name_to_handle_at 303
    %define SYS_open_by_handle_at 304
    %define SYS_clock_adjtime 305
    %define SYS_syncfs      306
    %define SYS_sendmmsg    307
    %define SYS_setns       308
    %define SYS_getcpu      309
    %define SYS_process_vm_readv 310
    %define SYS_process_vm_writev 311
    %define SYS_finit_module 313
    %define SYS_sched_setattr 314
    %define SYS_sched_getattr 315
    %define SYS_renameat2   316
    %define SYS_memfd_create 319
    %define SYS_bpf         321
    %define SYS_execveat    322
    %define SYS_socket      41
    %define SYS_connect     42
    %define SYS_accept      43
    %define SYS_sendto      44
    %define SYS_recvfrom    45
    %define SYS_sendmsg     46
    %define SYS_recvmsg     47
    %define SYS_shutdown    48
    %define SYS_bind        49
    %define SYS_listen      50
    %define SYS_getsockname 51
    %define SYS_getpeername 52
    %define SYS_socketpair  53
    %define SYS_clone       56
    %define SYS_fork        57
    %define SYS_execve      59
    
    ; Flags para open()
    %define O_RDONLY    0x0000
    %define O_WRONLY    0x0001
    %define O_RDWR      0x0002
    %define O_CREAT     0x0040
    %define O_EXCL      0x0080
    %define O_NOCTTY    0x0100
    %define O_TRUNC     0x0200
    %define O_APPEND    0x0400
    %define O_NONBLOCK  0x0800
    
    ; Flags para mmap()
    %define PROT_READ   0x1
    %define PROT_WRITE  0x2
    %define PROT_EXEC   0x4
    %define PROT_NONE   0x0
    %define MAP_SHARED  0x01
    %define MAP_PRIVATE 0x02
    %define MAP_ANONYMOUS 0x20
    %define MAP_FIXED   0x10

%elifdef PLATFORM_MACOS
    ; macOS/Darwin syscalls (BSD + 0x2000000)
    %define SYS_exit        0x2000001
    %define SYS_fork        0x2000002
    %define SYS_read        0x2000003
    %define SYS_write       0x2000004
    %define SYS_open        0x2000005
    %define SYS_close       0x2000006
    %define SYS_wait4       0x2000007
    %define SYS_link        0x2000009
    %define SYS_unlink      0x200000A
    %define SYS_chdir       0x200000C
    %define SYS_fchdir      0x200000D
    %define SYS_mknod       0x200000E
    %define SYS_chmod       0x200000F
    %define SYS_chown       0x2000010
    %define SYS_getpid      0x2000014
    %define SYS_mount       0x2000015
    %define SYS_unmount     0x2000016
    %define SYS_setuid      0x2000017
    %define SYS_getuid      0x2000018
    %define SYS_geteuid     0x2000019
    %define SYS_recvmsg     0x200001B
    %define SYS_sendmsg     0x200001C
    %define SYS_recvfrom    0x200001D
    %define SYS_accept      0x200001E
    %define SYS_getpeername 0x200001F
    %define SYS_getsockname 0x2000020
    %define SYS_access      0x2000021
    %define SYS_chflags     0x2000022
    %define SYS_fchflags    0x2000023
    %define SYS_sync        0x2000024
    %define SYS_kill        0x2000025
    %define SYS_getppid     0x2000027
    %define SYS_dup         0x2000029
    %define SYS_pipe        0x200002A
    %define SYS_getegid     0x200002B
    %define SYS_sigaction   0x200002E
    %define SYS_getgid      0x200002F
    %define SYS_sigprocmask 0x2000030
    %define SYS_getlogin    0x2000031
    %define SYS_setlogin    0x2000032
    %define SYS_acct        0x2000033
    %define SYS_sigpending  0x2000034
    %define SYS_sigaltstack 0x2000035
    %define SYS_ioctl       0x2000036
    %define SYS_reboot      0x2000037
    %define SYS_revoke      0x2000038
    %define SYS_symlink     0x2000039
    %define SYS_readlink    0x200003A
    %define SYS_execve      0x200003B
    %define SYS_umask       0x200003C
    %define SYS_chroot      0x200003D
    %define SYS_msync       0x2000041
    %define SYS_munmap      0x2000049
    %define SYS_mprotect    0x200004A
    %define SYS_madvise     0x200004B
    %define SYS_mincore     0x200004E
    %define SYS_getgroups   0x200004F
    %define SYS_setgroups   0x2000050
    %define SYS_getpgrp     0x2000051
    %define SYS_setpgid     0x2000052
    %define SYS_setitimer   0x2000053
    %define SYS_swapon      0x2000055
    %define SYS_getitimer   0x2000056
    %define SYS_getdtablesize 0x2000059
    %define SYS_dup2        0x200005A
    %define SYS_fcntl       0x200005C
    %define SYS_select      0x200005D
    %define SYS_fsync       0x200005F
    %define SYS_setpriority 0x2000060
    %define SYS_socket      0x2000061
    %define SYS_connect     0x2000062
    %define SYS_getpriority 0x2000064
    %define SYS_bind        0x2000068
    %define SYS_setsockopt  0x2000069
    %define SYS_listen      0x200006A
    %define SYS_sigsuspend  0x200006F
    %define SYS_gettimeofday 0x2000074
    %define SYS_getrusage   0x2000075
    %define SYS_getsockopt  0x2000076
    %define SYS_readv       0x2000078
    %define SYS_writev      0x2000079
    %define SYS_settimeofday 0x200007A
    %define SYS_fchown      0x200007B
    %define SYS_fchmod      0x200007C
    %define SYS_setreuid    0x200007E
    %define SYS_setregid    0x200007F
    %define SYS_rename      0x2000080
    %define SYS_flock       0x2000083
    %define SYS_mkfifo      0x2000084
    %define SYS_sendto      0x2000085
    %define SYS_shutdown    0x2000086
    %define SYS_socketpair  0x2000087
    %define SYS_mkdir       0x2000088
    %define SYS_rmdir       0x2000089
    %define SYS_utimes      0x200008A
    %define SYS_adjtime     0x200008C
    %define SYS_gethostuuid 0x200008E
    %define SYS_setsid      0x2000093
    %define SYS_getpgid     0x2000097
    %define SYS_setprivexec 0x2000098
    %define SYS_pread       0x2000099
    %define SYS_pwrite      0x200009A
    %define SYS_statfs      0x200009D
    %define SYS_fstatfs     0x200009E
    %define SYS_unmount     0x200009F
    %define SYS_getfh       0x20000A1
    %define SYS_quotactl    0x20000A5
    %define SYS_mount       0x20000A7
    %define SYS_csops       0x20000A9
    %define SYS_waitid      0x20000AD
    %define SYS_kdebug_trace 0x20000B4
    %define SYS_setgid      0x20000B5
    %define SYS_setegid     0x20000B6
    %define SYS_seteuid     0x20000B7
    %define SYS_sigreturn   0x20000B8
    %define SYS_thread_selfcounts 0x20000BA
    %define SYS_fdatasync   0x20000BB
    %define SYS_stat        0x20000BC
    %define SYS_fstat       0x20000BD
    %define SYS_lstat       0x20000BE
    %define SYS_pathconf    0x20000BF
    %define SYS_fpathconf   0x20000C0
    %define SYS_getrlimit   0x20000C2
    %define SYS_setrlimit   0x20000C3
    %define SYS_getdirentries 0x20000C4
    %define SYS_mmap        0x20000C5
    %define SYS_lseek       0x20000C7
    %define SYS_truncate    0x20000C8
    %define SYS_ftruncate   0x20000C9
    %define SYS_sysctl      0x20000CA
    %define SYS_mlock       0x20000CB
    %define SYS_munlock     0x20000CC
    %define SYS_undelete    0x20000CD
    
    ; Flags para open()
    %define O_RDONLY    0x0000
    %define O_WRONLY    0x0001
    %define O_RDWR      0x0002
    %define O_NONBLOCK  0x0004
    %define O_APPEND    0x0008
    %define O_CREAT     0x0200
    %define O_TRUNC     0x0400
    %define O_EXCL      0x0800
    
    ; Flags para mmap()
    %define PROT_NONE   0x00
    %define PROT_READ   0x01
    %define PROT_WRITE  0x02
    %define PROT_EXEC   0x04
    %define MAP_SHARED  0x0001
    %define MAP_PRIVATE 0x0002
    %define MAP_FIXED   0x0010
    %define MAP_RENAME  0x0020
    %define MAP_NORESERVE 0x0040
    %define MAP_INHERIT 0x0080
    %define MAP_NOEXTEND 0x0100
    %define MAP_HASSEMAPHORE 0x0200
    %define MAP_NOCACHE 0x0400
    %define MAP_JIT     0x0800
    %define MAP_ANON    0x1000
    %define MAP_ANONYMOUS MAP_ANON

%elifdef PLATFORM_WINDOWS
    ; Windows no usa syscalls directas, usa API functions
    ; Aquí definimos las funciones más comunes
    
    ; Kernel32.dll functions
    extern GetStdHandle
    extern WriteConsoleA
    extern ReadFile
    extern WriteFile
    extern CreateFileA
    extern CloseHandle
    extern ExitProcess
    extern GetLastError
    extern SetLastError
    extern GetCurrentDirectoryA
    extern SetCurrentDirectoryA
    extern CreateDirectoryA
    extern RemoveDirectoryA
    extern DeleteFileA
    extern MoveFileA
    extern CopyFileA
    extern GetFileAttributesA
    extern SetFileAttributesA
    extern FindFirstFileA
    extern FindNextFileA
    extern FindClose
    extern GetSystemTime
    extern GetLocalTime
    extern GetTickCount
    extern Sleep
    extern WaitForSingleObject
    extern CreateThread
    extern TerminateThread
    extern GetCurrentThread
    extern GetCurrentThreadId
    extern GetCurrentProcess
    extern GetCurrentProcessId
    extern CreateProcessA
    extern TerminateProcess
    extern GetExitCodeProcess
    extern VirtualAlloc
    extern VirtualFree
    extern VirtualProtect
    extern FlushInstructionCache
    extern LoadLibraryA
    extern GetProcAddress
    extern FreeLibrary
    extern GetModuleHandleA
    extern GetCommandLineA
    extern GetEnvironmentVariableA
    extern SetEnvironmentVariableA
    
    ; Constants
    %define STD_INPUT_HANDLE  -10
    %define STD_OUTPUT_HANDLE -11
    %define STD_ERROR_HANDLE  -12
    
    %define GENERIC_READ    0x80000000
    %define GENERIC_WRITE   0x40000000
    %define FILE_SHARE_READ 0x00000001
    %define FILE_SHARE_WRITE 0x00000002
    %define CREATE_NEW      1
    %define CREATE_ALWAYS   2
    %define OPEN_EXISTING   3
    %define OPEN_ALWAYS     4
    %define TRUNCATE_EXISTING 5
    
    %define FILE_ATTRIBUTE_NORMAL 0x80
    
    %define PAGE_NOACCESS   0x01
    %define PAGE_READONLY   0x02
    %define PAGE_READWRITE  0x04
    %define PAGE_WRITECOPY  0x08
    %define PAGE_EXECUTE    0x10
    %define PAGE_EXECUTE_READ 0x20
    %define PAGE_EXECUTE_READWRITE 0x40
    
    %define MEM_COMMIT      0x1000
    %define MEM_RESERVE     0x2000
    %define MEM_RELEASE     0x8000

%else
    %error "Platform not defined! Use -DPLATFORM_LINUX, -DPLATFORM_MACOS, or -DPLATFORM_WINDOWS"
%endif

; ===========================================================================
; Macros comunes para todas las plataformas
; ===========================================================================

; Macro para hacer syscall de forma portable
%macro SYSCALL 0
    %ifdef PLATFORM_WINDOWS
        ; En Windows, las "syscalls" son calls a funciones
        call rax
    %else
        ; Linux y macOS usan la instrucción syscall
        syscall
    %endif
%endmacro

; Macro para salir del programa
%macro EXIT 1
    %ifdef PLATFORM_LINUX
        mov rax, SYS_exit
        mov rdi, %1
        syscall
    %elifdef PLATFORM_MACOS
        mov rax, SYS_exit
        mov rdi, %1
        syscall
    %elifdef PLATFORM_WINDOWS
        mov rcx, %1
        call ExitProcess
    %endif
%endmacro

; Macro para escribir a stdout
%macro WRITE_STDOUT 2
    %ifdef PLATFORM_LINUX
        mov rax, SYS_write
        mov rdi, 1          ; stdout
        mov rsi, %1         ; buffer
        mov rdx, %2         ; length
        syscall
    %elifdef PLATFORM_MACOS
        mov rax, SYS_write
        mov rdi, 1          ; stdout
        mov rsi, %1         ; buffer
        mov rdx, %2         ; length
        syscall
    %elifdef PLATFORM_WINDOWS
        sub rsp, 40         ; Shadow space + alignment
        mov rcx, STD_OUTPUT_HANDLE
        call GetStdHandle
        mov rcx, rax        ; handle
        mov rdx, %1         ; buffer
        mov r8, %2          ; length
        lea r9, [rsp+32]    ; bytes written
        call WriteConsoleA
        add rsp, 40
    %endif
%endmacro

; Macro para allocar memoria
%macro ALLOC_MEMORY 1
    %ifdef PLATFORM_LINUX
        mov rax, SYS_mmap
        xor rdi, rdi        ; addr = NULL
        mov rsi, %1         ; length
        mov rdx, PROT_READ | PROT_WRITE
        mov r10, MAP_PRIVATE | MAP_ANONYMOUS
        mov r8, -1          ; fd = -1
        xor r9, r9          ; offset = 0
        syscall
    %elifdef PLATFORM_MACOS
        mov rax, SYS_mmap
        xor rdi, rdi        ; addr = NULL
        mov rsi, %1         ; length
        mov rdx, PROT_READ | PROT_WRITE
        mov r10, MAP_PRIVATE | MAP_ANON
        mov r8, -1          ; fd = -1
        xor r9, r9          ; offset = 0
        syscall
    %elifdef PLATFORM_WINDOWS
        sub rsp, 32         ; Shadow space
        xor rcx, rcx        ; lpAddress = NULL
        mov rdx, %1         ; dwSize
        mov r8, MEM_COMMIT | MEM_RESERVE
        mov r9, PAGE_READWRITE
        call VirtualAlloc
        add rsp, 32
    %endif
%endmacro

; Macro para liberar memoria
%macro FREE_MEMORY 2
    %ifdef PLATFORM_LINUX
        mov rax, SYS_munmap
        mov rdi, %1         ; addr
        mov rsi, %2         ; length
        syscall
    %elifdef PLATFORM_MACOS
        mov rax, SYS_munmap
        mov rdi, %1         ; addr
        mov rsi, %2         ; length
        syscall
    %elifdef PLATFORM_WINDOWS
        sub rsp, 32         ; Shadow space
        mov rcx, %1         ; lpAddress
        xor rdx, rdx        ; dwSize = 0
        mov r8, MEM_RELEASE
        call VirtualFree
        add rsp, 32
    %endif
%endmacro