option casemap:none

.CODE

; import Coroutine_Finish with its mangled Microsoft Visual C++ name
?Coroutine_Finish@@YAXXZ PROTO

; extern "C" void SaveNonVolatileRegs( uintptr_t regs[8] );
; incoming parameter is rcs
SaveNonVolatileRegs PROC FRAME
    .endprolog
	mov qword ptr[rcx], rbx
	mov qword ptr[rcx+8], rbp
	mov qword ptr[rcx+16], rsi
	mov qword ptr[rcx+24], rdi
	mov qword ptr[rcx+32], r12
	mov qword ptr[rcx+40], r13
	mov qword ptr[rcx+48], r14
	mov qword ptr[rcx+56], r15
	ret
SaveNonVolatileRegs ENDP

; extern "C" void NORETURN Coroutine_Launch_ASM( byte **ppStackHigh, uintptr_t **ppLaunchParentFramePtr, void (*pfnExec)( void* ), void *pvParam )
; Per Win64 ABI, incoming params are rcx, rdx, r8, r9. initial stack pointer is half-aligned due to return address
Coroutine_Launch_ASM PROC FRAME
	; x64 prolog and prolog description macros:

	; save caller's nonvolatile registers (pushed in reverse order to match SaveNonVolatileRegs)
	; so that we can slam new values in later to trick the x64 callstack unwind procedure
	push r15
	.pushreg r15
	push r14
	.pushreg r14
	push r13
	.pushreg r13
	push r12
	.pushreg r12
	push rdi
	.pushreg rdi
	push rsi
	.pushreg rsi
	push rbp
	.pushreg rbp
	push rbx
	.pushreg rbx

	; stack-allocate Win64 function call shadow space for calls to pfnExec and Coroutine_Finish,
	; plus 8 additional bytes to align the stack frame properly (comes in off by 8)
	sub rsp, 28h
	.allocstack 28h

	.endprolog

	; compute top of stack for coroutine: 40 bytes for stack, 64 for saved regs, 8 for return address
	; (we do not bother including the additional unused 32 byte shadow space we own above that)
	lea rax, [rsp+70h]			
	mov qword ptr [rcx], rax

	; save off the address of our saved regs so that we can memcpy over them later and trick
	; the x64 stack unwind logic into walking up to a different Internal_Coroutine_Continue
	lea rax, [rsp+28h]
	mov qword ptr [rdx], rax

	; call pfnExec(pvParam)
	mov rcx, r9
	call r8

	; call Coroutine_Finish - does not return
	call ?Coroutine_Finish@@YAXXZ

Coroutine_Launch_ASM ENDP




; Needs to match definition found in setjmp.h
_JUMP_BUFFER STRUCT
	m_Frame QWORD ?
	m_Rbx QWORD ?
	m_Rsp QWORD ?
	m_Rbp QWORD ?
	m_Rsi QWORD ?
	m_Rdi QWORD ?
	m_R12 QWORD ?
	m_R13 QWORD ?
	m_R14 QWORD ?
	m_R15 QWORD ?
	m_Rip QWORD ?
	m_MxCsr DWORD ?
	m_FpCsr WORD ?
	m_Spare WORD ?
	m_Xmm6 XMMWORD ?
	m_Xmm7 XMMWORD ?
	m_Xmm8 XMMWORD ?
	m_Xmm9 XMMWORD ?
	m_Xmm10 XMMWORD ?
	m_Xmm11 XMMWORD ?
	m_Xmm12 XMMWORD ?
	m_Xmm13 XMMWORD ?
	m_Xmm14 XMMWORD ?
	m_Xmm15 XMMWORD ?
_JUMP_BUFFER ENDS


;This is the reference asm for __intrinsic_setjmp() in VS2015
;mov         qword ptr [rcx],rdx		; intrinsic call site does "mov rdx,rbp" followed by "add rdx,0FFFFFFFFFFFFFFC0h", looks like a nonstandard abi
;mov         qword ptr [rcx+8],rbx  
;mov         qword ptr [rcx+18h],rbp  
;mov         qword ptr [rcx+20h],rsi  
;mov         qword ptr [rcx+28h],rdi  
;mov         qword ptr [rcx+30h],r12  
;mov         qword ptr [rcx+38h],r13  
;mov         qword ptr [rcx+40h],r14  
;mov         qword ptr [rcx+48h],r15  
;lea         r8,[rsp+8]					; rsp set to post-return address
;mov         qword ptr [rcx+10h],r8  
;mov         r8,qword ptr [rsp]  
;mov         qword ptr [rcx+50h],r8  
;stmxcsr     dword ptr [rcx+58h]  
;fnstcw      word ptr [rcx+5Ch]  
;movdqa      xmmword ptr [rcx+60h],xmm6  
;ovdqa      xmmword ptr [rcx+70h],xmm7  
;movdqa      xmmword ptr [rcx+80h],xmm8  
;movdqa      xmmword ptr [rcx+90h],xmm9  
;movdqa      xmmword ptr [rcx+0A0h],xmm10  
;movdqa      xmmword ptr [rcx+0B0h],xmm11  
;movdqa      xmmword ptr [rcx+0C0h],xmm12  
;movdqa      xmmword ptr [rcx+0D0h],xmm13  
;movdqa      xmmword ptr [rcx+0E0h],xmm14  
;movdqa      xmmword ptr [rcx+0F0h],xmm15  
;xor         eax,eax  
;ret  


; extern "C" void NORETURN Coroutine_LongJmp_UnChecked( jmp_buf buf, int nResult )
; Per Win64 ABI, incoming params are rcx, rdx, r8, r9. initial stack pointer is half-aligned due to return address
Coroutine_LongJmp_Unchecked PROC
	;load nResult into result from initial setjmp()
	xor rax, rax
	mov eax, edx
	
	;restore to setjmp() caller state
	mov rdx, [rcx]._JUMP_BUFFER.m_Frame ; appears to be an error checking value of (_JUMP_BUFFER.m_Rbp + 0FFFFFFFFFFFFFFC0h) passed non-standardly through rdx to setjmp()
	mov rbx, [rcx]._JUMP_BUFFER.m_Rbx
	mov rsp, [rcx]._JUMP_BUFFER.m_Rsp
	mov rbp, [rcx]._JUMP_BUFFER.m_Rbp
	mov rsi, [rcx]._JUMP_BUFFER.m_Rsi
	mov rdi, [rcx]._JUMP_BUFFER.m_Rdi
	mov r12, [rcx]._JUMP_BUFFER.m_R12
	mov r13, [rcx]._JUMP_BUFFER.m_R13
	mov r14, [rcx]._JUMP_BUFFER.m_R14
	mov r15, [rcx]._JUMP_BUFFER.m_R15
	mov r10, [rcx]._JUMP_BUFFER.m_Rip ; store return address in r10 for return
	ldmxcsr [rcx]._JUMP_BUFFER.m_MxCsr
	fldcw [rcx]._JUMP_BUFFER.m_FpCsr
	;[rcx]._JUMP_BUFFER.m_Spare
	movaps xmm6, [rcx]._JUMP_BUFFER.m_Xmm6
	movaps xmm7, [rcx]._JUMP_BUFFER.m_Xmm7
	movaps xmm8, [rcx]._JUMP_BUFFER.m_Xmm8
	movaps xmm9, [rcx]._JUMP_BUFFER.m_Xmm9
	movaps xmm10, [rcx]._JUMP_BUFFER.m_Xmm10
	movaps xmm11, [rcx]._JUMP_BUFFER.m_Xmm11
	movaps xmm12, [rcx]._JUMP_BUFFER.m_Xmm12
	movaps xmm13, [rcx]._JUMP_BUFFER.m_Xmm13
	movaps xmm14, [rcx]._JUMP_BUFFER.m_Xmm14
	movaps xmm15, [rcx]._JUMP_BUFFER.m_Xmm15

	;jmp instead of ret to _JUMP_BUFFER.m_Rip because setjmp() already set the _JUMP_BUFFER.m_Rsp to the post-return state
	db 048h ; emit a REX prefix on the jmp to ensure it's a full qword
	jmp qword ptr r10
Coroutine_LongJmp_Unchecked ENDP


_TEXT ENDS
END
