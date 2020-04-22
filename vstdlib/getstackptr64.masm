; call cpuid with args in eax, ecx
; store eax, ebx, ecx, edx to p
PUBLIC GetStackPtr64
.CODE

GetStackPtr64	PROC FRAME
; unsigned char* GetStackPtr64(void);
        .endprolog      
        
        mov		rax, rsp	; get stack ptr
        add		rax, 8h		; account for 8-byte return value of this function

        ret

GetStackPtr64 ENDP
_TEXT ENDS
END
