.code

INCLUDE native_shared.inc

FiberStatus STRUCT
	callee QWORD	?
	termcb QWORD	?
	status DWORD	?
FiberStatus ENDS

Fiber STRUCT
    reg_eip QWORD       ?
    reg_esp QWORD       ?
    reg_r12 QWORD       ?
    reg_r13 QWORD       ?
    reg_r14 QWORD       ?
    reg_r15 QWORD       ?   
    reg_rdi QWORD       ?
    reg_rsi QWORD       ?
    reg_rbx QWORD       ?
    reg_rbp QWORD       ?

    reg_xmm6 XMMWORD    <>
    reg_xmm7 XMMWORD    <>
    reg_xmm8 XMMWORD    <>
    reg_xmm9 XMMWORD    <>
    reg_xmm10 XMMWORD   <>
    reg_xmm11 XMMWORD   <>
    reg_xmm12 XMMWORD   <>
    reg_xmm13 XMMWORD   <>
    reg_xmm14 XMMWORD   <>
    reg_xmm15 XMMWORD   <>

	stack_base QWORD PTR	?
	stack_limit QWORD PTR	?
    except_info QWORD PTR   ?

	stackSize QWORD		?
Fiber ENDS

; FIBER ROUTINES

_fiber64_procStart PROC
    ; Grab the proto handler.
    mov rax,[rsp+16]

    ; We first have to call the proto handler.
    ; For that we must establish a valid MSVC 64bit frame.
    mov rbx,rsp
    mov r10,rsp
    and r10,15
    sub rsp,r10

    call rax

    mov rsp,rbx

    ; After that we must do the termination handler.
	; Set thread status to terminated
	mov rax,[rsp+8]

	;ASSUME rax:PTR FiberStatus

    mov [rax].FiberStatus.status,FIBER_TERMINATED

    ; Cache the termination function
    mov rdx,[rax].FiberStatus.termcb

	mov rcx,rax ; this will be first argument to termination proto.

    ; Apply registers
    mov rax,[rax].FiberStatus.callee

	;ASSUME rax:PTR Fiber

    mov r12,[rax].Fiber.reg_r12
    mov r13,[rax].Fiber.reg_r13
    mov r14,[rax].Fiber.reg_r14
    mov r15,[rax].Fiber.reg_r15
    mov rdi,[rax].Fiber.reg_rdi
    mov rsi,[rax].Fiber.reg_rsi
    mov rbx,[rax].Fiber.reg_rbx
    mov rsp,[rax].Fiber.reg_esp
    mov rbp,[rax].Fiber.reg_rbp

    ; We also need XMM registers.
    movupd xmm6,[rax].Fiber.reg_xmm6
    movupd xmm7,[rax].Fiber.reg_xmm7
    movupd xmm8,[rax].Fiber.reg_xmm8
    movupd xmm9,[rax].Fiber.reg_xmm9
    movupd xmm10,[rax].Fiber.reg_xmm10
    movupd xmm11,[rax].Fiber.reg_xmm11
    movupd xmm12,[rax].Fiber.reg_xmm12
    movupd xmm13,[rax].Fiber.reg_xmm13
    movupd xmm14,[rax].Fiber.reg_xmm14
    movupd xmm15,[rax].Fiber.reg_xmm15

    push [rax].Fiber.reg_eip    ; The return address

    ; Apply exception and stack info.
    mov r10,[rax].Fiber.except_info
    mov r11,[rax].Fiber.stack_base
    mov rax,[rax].Fiber.stack_limit
    mov gs:[0],r10      ; except_info
    mov gs:[8],r11      ; stack_base
    mov gs:[16],rax     ; stack_limit

	;ASSUME rax:nothing

	; Terminate our thread
    call rdx

    ret
_fiber64_procStart ENDP

_fiber64_eswitch PROC
    ; RCX contains our from Fiber.
    ; RDX contains our to Fiber.

    ;ASSUME eax:PTR Fiber

    ; Save current environment
    mov [rcx].Fiber.reg_r12,r12
    mov [rcx].Fiber.reg_r13,r13
    mov [rcx].Fiber.reg_r14,r14
    mov [rcx].Fiber.reg_r15,r15
    mov [rcx].Fiber.reg_rsi,rsi
    mov [rcx].Fiber.reg_rdi,rdi
    mov [rcx].Fiber.reg_rbx,rbx
    mov [rcx].Fiber.reg_rbp,rbp
    pop rax
    mov [rcx].Fiber.reg_esp,rsp
    mov [rcx].Fiber.reg_eip,rax

    ; We have to apply SSE regs aswell.
    movupd [rcx].Fiber.reg_xmm6,xmm6
    movupd [rcx].Fiber.reg_xmm7,xmm7
    movupd [rcx].Fiber.reg_xmm8,xmm8
    movupd [rcx].Fiber.reg_xmm9,xmm9
    movupd [rcx].Fiber.reg_xmm10,xmm10
    movupd [rcx].Fiber.reg_xmm11,xmm11
    movupd [rcx].Fiber.reg_xmm12,xmm12
    movupd [rcx].Fiber.reg_xmm13,xmm13
    movupd [rcx].Fiber.reg_xmm14,xmm14
    movupd [rcx].Fiber.reg_xmm15,xmm15

    ; Save exception and stack info.
    mov r10,gs:[0]      ; except_info
    mov r11,gs:[8]      ; stack_base
    mov rax,gs:[16]     ; stack_limit
    mov [rcx].Fiber.stack_base,r11
    mov [rcx].Fiber.stack_limit,rax
    mov [rcx].Fiber.except_info,r10

    ; Apply registers
    mov r12,[rdx].Fiber.reg_r12
    mov r13,[rdx].Fiber.reg_r13
    mov r14,[rdx].Fiber.reg_r14
    mov r15,[rdx].Fiber.reg_r15
    mov rdi,[rdx].Fiber.reg_rdi
    mov rsi,[rdx].Fiber.reg_rsi
    mov rbx,[rdx].Fiber.reg_rbx
    mov rsp,[rdx].Fiber.reg_esp
    mov rbp,[rdx].Fiber.reg_rbp

    ; We also need XMM registers.
    movupd xmm6,[rdx].Fiber.reg_xmm6
    movupd xmm7,[rdx].Fiber.reg_xmm7
    movupd xmm8,[rdx].Fiber.reg_xmm8
    movupd xmm9,[rdx].Fiber.reg_xmm9
    movupd xmm10,[rdx].Fiber.reg_xmm10
    movupd xmm11,[rdx].Fiber.reg_xmm11
    movupd xmm12,[rdx].Fiber.reg_xmm12
    movupd xmm13,[rdx].Fiber.reg_xmm13
    movupd xmm14,[rdx].Fiber.reg_xmm14
    movupd xmm15,[rdx].Fiber.reg_xmm15

    ; Apply exception and stack info.
    mov r10,[rdx].Fiber.except_info
    mov r11,[rdx].Fiber.stack_base
    mov rax,[rdx].Fiber.stack_limit
    mov gs:[0],r10      ; except_info
    mov gs:[8],r11      ; stack_base
    mov gs:[16],rax     ; stack_limit

    jmp qword ptr[rdx].Fiber.reg_eip

    ;ASSUME eax:nothing
_fiber64_eswitch ENDP

_fiber64_qswitch PROC
    ; RCX contains our from Fiber.
    ; RDX contains our to Fiber.

    ;ASSUME eax:PTR Fiber

    ; Save current environment
    mov [rcx].Fiber.reg_r12,r12
    mov [rcx].Fiber.reg_r13,r13
    mov [rcx].Fiber.reg_r14,r14
    mov [rcx].Fiber.reg_r15,r15
    mov [rcx].Fiber.reg_rsi,rsi
    mov [rcx].Fiber.reg_rdi,rdi
    mov [rcx].Fiber.reg_rbx,rbx
    mov [rcx].Fiber.reg_rbp,rbp
    pop rax
    mov [rcx].Fiber.reg_esp,rsp
    mov [rcx].Fiber.reg_eip,rax

    ; We have to apply SSE regs aswell.
    movupd [rcx].Fiber.reg_xmm6,xmm6
    movupd [rcx].Fiber.reg_xmm7,xmm7
    movupd [rcx].Fiber.reg_xmm8,xmm8
    movupd [rcx].Fiber.reg_xmm9,xmm9
    movupd [rcx].Fiber.reg_xmm10,xmm10
    movupd [rcx].Fiber.reg_xmm11,xmm11
    movupd [rcx].Fiber.reg_xmm12,xmm12
    movupd [rcx].Fiber.reg_xmm13,xmm13
    movupd [rcx].Fiber.reg_xmm14,xmm14
    movupd [rcx].Fiber.reg_xmm15,xmm15

    ; Save exception info (due to quick switch)
    mov r10,gs:[0]      ; except_info
    mov [rcx].Fiber.except_info,r10

    ; Apply registers
    mov r12,[rdx].Fiber.reg_r12
    mov r13,[rdx].Fiber.reg_r13
    mov r14,[rdx].Fiber.reg_r14
    mov r15,[rdx].Fiber.reg_r15
    mov rdi,[rdx].Fiber.reg_rdi
    mov rsi,[rdx].Fiber.reg_rsi
    mov rbx,[rdx].Fiber.reg_rbx
    mov rsp,[rdx].Fiber.reg_esp
    mov rbp,[rdx].Fiber.reg_rbp

    ; We also need XMM registers.
    movupd xmm6,[rdx].Fiber.reg_xmm6
    movupd xmm7,[rdx].Fiber.reg_xmm7
    movupd xmm8,[rdx].Fiber.reg_xmm8
    movupd xmm9,[rdx].Fiber.reg_xmm9
    movupd xmm10,[rdx].Fiber.reg_xmm10
    movupd xmm11,[rdx].Fiber.reg_xmm11
    movupd xmm12,[rdx].Fiber.reg_xmm12
    movupd xmm13,[rdx].Fiber.reg_xmm13
    movupd xmm14,[rdx].Fiber.reg_xmm14
    movupd xmm15,[rdx].Fiber.reg_xmm15

    ; Apply exception and stack info.
    mov r10,[rdx].Fiber.except_info
    mov r11,[rdx].Fiber.stack_base
    mov rax,[rdx].Fiber.stack_limit
    mov gs:[0],r10      ; except_info
    mov gs:[8],r11      ; stack_base
    mov gs:[16],rax     ; stack_limit

    jmp qword ptr[rdx].Fiber.reg_eip

    ;ASSUME eax:nothing
_fiber64_qswitch ENDP

_fiber64_leave PROC
    ; We assume that eax contains the fiber ptr.
    ; This is an optimized function to give up a context 
    ; and immediatly switch to a Fiber.

    ;ASSUME eax:PTR Fiber

    ; Apply registers
    mov r12,[rax].Fiber.reg_r12
    mov r13,[rax].Fiber.reg_r13
    mov r14,[rax].Fiber.reg_r14
    mov r15,[rax].Fiber.reg_r15
    mov rdi,[rax].Fiber.reg_rdi
    mov rsi,[rax].Fiber.reg_rsi
    mov rbx,[rax].Fiber.reg_rbx
    mov rsp,[rax].Fiber.reg_esp
    mov rbp,[rax].Fiber.reg_rbp

    ; We also need XMM registers.
    movupd xmm6,[rax].Fiber.reg_xmm6
    movupd xmm7,[rax].Fiber.reg_xmm7
    movupd xmm8,[rax].Fiber.reg_xmm8
    movupd xmm9,[rax].Fiber.reg_xmm9
    movupd xmm10,[rax].Fiber.reg_xmm10
    movupd xmm11,[rax].Fiber.reg_xmm11
    movupd xmm12,[rax].Fiber.reg_xmm12
    movupd xmm13,[rax].Fiber.reg_xmm13
    movupd xmm14,[rax].Fiber.reg_xmm14
    movupd xmm15,[rax].Fiber.reg_xmm15

    ; Apply exception and stack info.
    mov r10,[rax].Fiber.except_info
    mov r11,[rax].Fiber.stack_base
    mov rax,[rax].Fiber.stack_limit
    mov gs:[0],r10      ; except_info
    mov gs:[8],r11      ; stack_base
    mov gs:[16],rax     ; stack_limit

    jmp qword ptr[rax].Fiber.reg_eip

    ;ASSUME eax:PTR nothing
_fiber64_leave ENDP

; THREAD ROUTINES

nativeThreadPlugin STRUCT
    terminationReturn QWORD     ?
nativeThreadPlugin ENDS

EXTERN nativeThreadPluginInterface_ThreadProcCPP:PROC
EXTERN nativeThreadPluginInterface_OnNativeThreadEnd:PROC

_thread64_procNative PROC
    ; Store the NT DISPATCHER stack.
    mov rdi,rsp

    ; REMEMBER that we are entering MSVC 2015 64 logic.
    ; This compiler assumed that the stack is always aligned to 16bytes,
    ; so that it can compile happily using SSE instructions.
    ; Make sure we have this property.
    mov rax,rsp
    and rax,15
    sub rsp,rax

    ; Call the C++ thread runtime with our argument.
    ; in rcx, we have our nativeThreadPlugin type
    mov rbx,rcx

    ;ASSUME rcx:PTR nativeThreadPlugin

	; We have to reserve some stack space for x64 calling-convention register spilling, at a minimum 32bytes.
    sub rsp,20h

    call nativeThreadPluginInterface_ThreadProcCPP

    ; Set the first argument for the next function call.
    mov rcx,rbx

    ; Check for a termination fiber.
    mov rbx,[rbx].nativeThreadPlugin.terminationReturn

    ; Give control of the native thread back to the manager.
    call nativeThreadPluginInterface_OnNativeThreadEnd

    ; Free the register spilling stack space.
    add rsp,20h

    ;ASSUME ebx:nothing

    test rbx,rbx
    jz NoTerminationReturn

    ; Since we have a termination return fiber, leave to it.
    mov rax,rbx
    jmp _fiber64_leave

NoTerminationReturn:
    ; RESTORE THE NT DISPATCHER STACK.
    mov rsp,rdi

    ; We return to the WinNT thread dispatcher.
    mov rax,0
    ret
_thread64_procNative ENDP

EXTERN _nativeThreadTerminationProto:PROC

_thread64_termProto PROC
    ; in r15 we expect userdata ptr.
    mov rcx,r15

    jmp _nativeThreadTerminationProto
_thread64_termProto ENDP

END