;Aisosa Okunbor
   .def getmsp
   .def getpsp
   .def setmsp
   .def setpsp
   .def usepsp
   .def hardFaultHandler
   .def usageFaultHandler
   .def busFaultHandler
   .def mpuFaultHandler
   ;.def pendsvHandler
   .def setunpriv
   .def clearunpriv
   .def start
   .def startTask
   .def swpush
   .def swpop
   .def svcPidof
   .def svcKill
   .def svcPkill
   ;.def svcRevive
   .def svcPs
   .def svcIpcs
   ;.def svcRun

    .ref hardFaultC
    .ref usageFaultC
    .ref busFaultC
    .ref mpuFaultC
  ;  .ref pendsvC
  	;.ref swpush
  	;.ref swpop
   .text

getmsp:
 	MRS R0, MSP
 	BX LR
getpsp:
	MRS R0, PSP
 	BX LR
setmsp:
 	MSR MSP, R0
 	ISB
 	BX LR
setpsp:
	MSR PSP, R0
	ISB             
	BX LR
usepsp:
	MRS R0, CONTROL
	ORR R0, R0, #2
	MSR CONTROL, R0
	ISB
	BX LR
setunpriv:
	MRS R0, CONTROL
	ORR R0, R0, #1
	MSR CONTROL, R0
	ISB
	BX LR
clearunpriv:
    MRS R0, CONTROL
    BIC R0, R0, #1      ; clear bit 0 = 0 meaning privileged
    MSR CONTROL, R0
    ISB
    BX LR

hardFaultHandler:
    TST     LR, #4          
    ITE     EQ
    MRSEQ   R0, MSP         
    MRSNE   R0, PSP
    MRS     R1, MSP
    MRS     R2, PSP
    MOV		R3, LR
    B       hardFaultC

usageFaultHandler:
    TST     LR, #4
    ITE     EQ
    MRSEQ   R0, MSP
    MRSNE   R0, PSP
    MRS     R1, MSP
    MRS     R2, PSP
    MOV		R3, LR
    B       usageFaultC

busFaultHandler:
    TST     LR, #4  ;  updates the Z flag msp -> z =1, psp -> z =0
    ITE     EQ ; if((LR & 0x4) == 0) R0 = MSP else R0 = PSP. determines where the exception came from
    MRSEQ   R0, MSP
    MRSNE   R0, PSP
    MRS     R1, MSP
    MRS     R2, PSP
    MOV		R3, LR
    B       busFaultC

mpuFaultHandler:
    TST     LR, #4
    ITE     EQ
    MRSEQ   R0, MSP
    MRSNE   R0, PSP
    MRS     R1, MSP
    MRS     R2, PSP
    MOV		R3, LR
    B       mpuFaultC
start:
	; R0 = sp, R1 = fn
	BX LR
; Start the first task
; R0 contains the function address to jump to
startTask:
    ; Switch to unprivileged mode
    MRS R1, CONTROL
    ORR R1, R1, #1      ; Set bit 0 (nPRIV = 1 = unprivileged)
    MSR CONTROL, R1
    ISB

    ; Jump to the task function (address in R0)
    BX R0
swpush:
    MRS R0, PSP         ; Copy PSP to R0
    STR R4, [R0, #-4]!  ; Store R4
    STR R5, [R0, #-8]!  ; Store R5
    STR R6, [R0, #-12]!  ; Store R6
    STR R7, [R0, #-16]!  ; Store R7
    STR R8, [R0, #-20]!  ; Store R8
    STR R9, [R0, #-24]!  ; Store R9
    STR R10, [R0, #-28]! ; Store R10
    STR R11, [R0, #-32]! ; Store R11
    BX LR
swpop:
    MRS R0, PSP         ; Copy PSP to R0
    ;LDR LR, [R0], #4    ; Restore LR
    ADD R0, R0, #4
    LDR R11, [R0], #4   ; Restore R11
    LDR R10, [R0], #4   ; Restore R10
    LDR R9, [R0], #4    ; Restore R9
    LDR R8, [R0], #4    ; Restore R8
    LDR R7, [R0], #4    ; Restore R7
    LDR R6, [R0], #4    ; Restore R6
    LDR R5, [R0], #4    ; Restore R5
    LDR R4, [R0], #4    ; Restore R4
    BX LR
svcPidof:
    SVC #10
    BX LR
svcKill:
	SVC #11
	BX LR
svcPkill:
	SVC #12
	BX LR
;svcRevive:
    ;SVC #13
    ;BX LR
svcPs:
    SVC #14
    BX LR
svcIpcs:
    SVC #15
    BX LR
;svcRun:
    ;SVC #17
    ;BX  LR
    
;pendsvHandler:
 ;   MRS     R0, MSP
  ;  MRS     R1, PSP
   ; B       pendsvC
