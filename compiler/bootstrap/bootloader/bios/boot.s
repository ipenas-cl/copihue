; ╔═════╦═════╦═════╗
; ║ 🛡️  ║ ⚖️  ║ ⚡  ║
; ║  C  ║  E  ║  G  ║
; ╚═════╩═════╩═════╝
; ╔═════════════════╗
; ║ wcet [T∞] bound ║
; ╚═════════════════╝
;
; Author: Ignacio Peña Sepúlveda
; Date: June 25, 2025
;
; AtomicOS BIOS Stage 1 Bootloader
; This bootloader is loaded by BIOS at 0x7C00
; It loads stage2 from disk and transfers control to it

[BITS 16]
[ORG 0x7C00]

; Boot sector entry point
start:
    ; Set up segments
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00      ; Stack grows downward from boot sector
    
    ; Save boot drive
    mov [boot_drive], dl
    
    ; Clear screen
    mov ax, 0x0003      ; Text mode 80x25, 16 colors
    int 0x10
    
    ; Print boot message
    mov si, boot_msg
    call print_string
    
    ; Enable A20 line
    call enable_a20
    
    ; Load stage2 from disk
    call load_stage2
    
    ; Jump to stage2
    jmp 0x0000:0x8000

; Print string routine
; SI = pointer to null-terminated string
print_string:
    pusha
.loop:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0E        ; BIOS teletype output
    mov bh, 0x00        ; Page number
    mov bl, 0x07        ; Light gray color
    int 0x10
    jmp .loop
.done:
    popa
    ret

; Enable A20 line using multiple methods
enable_a20:
    pusha
    
    ; Method 1: BIOS interrupt
    mov ax, 0x2401
    int 0x15
    jc .keyboard_method  ; If failed, try keyboard method
    
    ; Check if A20 is enabled
    call check_a20
    test ax, ax
    jnz .done
    
.keyboard_method:
    ; Method 2: Keyboard controller
    call wait_kbd_in
    mov al, 0xAD        ; Disable keyboard
    out 0x64, al
    
    call wait_kbd_in
    mov al, 0xD0        ; Read output port
    out 0x64, al
    
    call wait_kbd_out
    in al, 0x60         ; Read data
    push ax
    
    call wait_kbd_in
    mov al, 0xD1        ; Write output port
    out 0x64, al
    
    call wait_kbd_in
    pop ax
    or al, 2            ; Set A20 bit
    out 0x60, al
    
    call wait_kbd_in
    mov al, 0xAE        ; Enable keyboard
    out 0x64, al
    
    call wait_kbd_in
    
.done:
    popa
    ret

; Wait for keyboard controller input buffer
wait_kbd_in:
    in al, 0x64
    test al, 2
    jnz wait_kbd_in
    ret

; Wait for keyboard controller output buffer
wait_kbd_out:
    in al, 0x64
    test al, 1
    jz wait_kbd_out
    ret

; Check if A20 is enabled
check_a20:
    pushf
    push ds
    push es
    push di
    push si
    
    xor ax, ax
    mov es, ax
    mov di, 0x0500      ; 0000:0500
    
    mov ax, 0xFFFF
    mov ds, ax
    mov si, 0x0510      ; FFFF:0510 = 0x100500
    
    mov al, [es:di]
    push ax
    
    mov al, [ds:si]
    push ax
    
    mov byte [es:di], 0x00
    mov byte [ds:si], 0xFF
    
    cmp byte [es:di], 0xFF
    
    pop ax
    mov [ds:si], al
    
    pop ax
    mov [es:di], al
    
    mov ax, 0
    je .disabled
    mov ax, 1
    
.disabled:
    pop si
    pop di
    pop es
    pop ds
    popf
    ret

; Load stage2 from disk
load_stage2:
    pusha
    
    ; Reset disk system
    mov ah, 0x00
    mov dl, [boot_drive]
    int 0x13
    jc .disk_error
    
    ; Read sectors using LBA
    mov si, dap
    mov ah, 0x42        ; Extended read
    mov dl, [boot_drive]
    int 0x13
    jc .disk_error
    
    mov si, stage2_loaded_msg
    call print_string
    
    popa
    ret
    
.disk_error:
    mov si, disk_error_msg
    call print_string
    jmp $               ; Hang

; Disk Address Packet for loading stage2
dap:
    db 0x10             ; Size of DAP
    db 0                ; Reserved
    dw 64               ; Number of sectors to read (32KB)
    dw 0x8000           ; Offset
    dw 0x0000           ; Segment
    dq 1                ; LBA of first sector (sector 2)

; Data
boot_drive:         db 0
boot_msg:           db 'AtomicOS Stage 1 Bootloader', 13, 10, 0
stage2_loaded_msg:  db 'Stage 2 loaded successfully', 13, 10, 0
disk_error_msg:     db 'Disk error!', 13, 10, 0

; Pad to 510 bytes and add boot signature
times 510-($-$$) db 0
dw 0xAA55