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
   .def pendsvHandler
   .def setunpriv
   .def clearunpriv

    .ref hardFaultC
    .ref usageFaultC
    .ref busFaultC
    .ref mpuFaultC
    .ref pendsvC
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

pendsvHandler:
    MRS     R0, MSP
    MRS     R1, PSP
    B       pendsvC
