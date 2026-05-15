	INCLUDE equates.s

	IMPORT NES_RAM
	IMPORT NES_SRAM	
	IMPORT CPU_reset
	IMPORT PRG_ROM_Cache    ; 【修改】引入 32KB 缓存，抛弃原 romfile
	IMPORT load_prg_bank_c  ; 【修改】引入 C 语言读取 SD 卡的助手
	IMPORT cpu_data	   
	IMPORT op_table
		
	EXPORT cpu6502_init
	EXPORT map67_
	EXPORT map89_
	EXPORT mapAB_
	EXPORT mapCD_
	EXPORT mapEF_
;----------------------------------------------------------------------------
 AREA rom_code, CODE, READONLY
   PRESERVE8   
;----------------------------------------------------------------------------
cpu6502_init PROC
;----------------------------------------------------------------------------
	stmfd sp!,{r4-r11,lr}
		
    ldr r10,=cpu_data	
	ldr r11,=NES_RAM	
	ldr r11,[r11]       

	str r11,memmap_tbl             
	str r11,memmap_tbl+4
	str r11,memmap_tbl+8

	ldr r0,=NES_SRAM              
	ldr r0,[r0]
	str r0,memmap_tbl+12
	
	ldr r0,=op_table   
	str r0,opz         

	mov r9,#0		
	str r9,lastbank		

    ; 载入开头的 16KB 和 最后的 16KB
	mov r0,#0			
	bl map89AB_			
	mov r0,#-1
	bl mapCDEF_			
    
	ldr r0,=Mapper_W
	str r0,writemem_tbl+16
	str r0,writemem_tbl+20
	str r0,writemem_tbl+24
	str r0,writemem_tbl+28

	bl CPU_reset		
	ldmfd sp!,{r4-r11,lr}
	bx lr
	ENDP

;----------------------------------------------------------------------------
map67_	; SRAM 映射 ($6000-$7FFF)
;----------------------------------------------------------------------------
	ldr r1,=NES_SRAM
	ldr r1,[r1]
	sub r1,r1,#0x6000
	str r1,memmap_tbl+12
	b flush
    
;----------------------------------------------------------------------------
map89_	; r0=8k page# ($8000-$9FFF)
;----------------------------------------------------------------------------
    stmfd sp!, {r0-r4, r9, r12, lr} ; 保存关键寄存器并保持8字节对齐
    mov r1, r0          ; arg1: bank_num
    mov r0, #0          ; arg0: slot 0
    bl load_prg_bank_c  ; 从SD卡读取数据
    ldmfd sp!, {r0-r4, r9, r12, lr}

	ldr r1,=PRG_ROM_Cache
    ldr r1,[r1]
	sub r1,r1,#0x8000
	str r1,memmap_tbl+16
	b flush
    
;----------------------------------------------------------------------------
mapAB_  ; r0=8k page# ($A000-$BFFF)
;----------------------------------------------------------------------------
    stmfd sp!, {r0-r4, r9, r12, lr}
    mov r1, r0
    mov r0, #1          ; arg0: slot 1
    bl load_prg_bank_c
    ldmfd sp!, {r0-r4, r9, r12, lr}

	ldr r1,=PRG_ROM_Cache
    ldr r1,[r1]
    add r1,r1,#0x2000
	sub r1,r1,#0xA000
	str r1,memmap_tbl+20
	b flush
    
;----------------------------------------------------------------------------
mapCD_  ; r0=8k page# ($C000-$DFFF)
;----------------------------------------------------------------------------
    stmfd sp!, {r0-r4, r9, r12, lr}
    mov r1, r0
    mov r0, #2          ; arg0: slot 2
    bl load_prg_bank_c
    ldmfd sp!, {r0-r4, r9, r12, lr}

	ldr r1,=PRG_ROM_Cache
    ldr r1,[r1]
    add r1,r1,#0x4000
	sub r1,r1,#0xC000
	str r1,memmap_tbl+24
	b flush
    
;----------------------------------------------------------------------------
mapEF_  ; r0=8k page# ($E000-$FFFF)
;----------------------------------------------------------------------------
    stmfd sp!, {r0-r4, r9, r12, lr}
    mov r1, r0
    mov r0, #3          ; arg0: slot 3
    bl load_prg_bank_c
    ldmfd sp!, {r0-r4, r9, r12, lr}

	ldr r1,=PRG_ROM_Cache
    ldr r1,[r1]
    add r1,r1,#0x6000
	sub r1,r1,#0xE000
	str r1,memmap_tbl+28
	b flush
    
;----------------------------------------------------------------------------
map89AB_ ; r0=16k page# 
;----------------------------------------------------------------------------
    stmfd sp!, {r0-r4, r9, r12, lr}
    mov r1, r0, lsl #1  ; bank_num = r0 * 2
    mov r0, #0
    bl load_prg_bank_c
    ldmfd sp!, {r0-r4, r9, r12, lr}

    stmfd sp!, {r0-r4, r9, r12, lr}
    mov r1, r0, lsl #1
    add r1, r1, #1      ; bank_num = r0 * 2 + 1
    mov r0, #1
    bl load_prg_bank_c
    ldmfd sp!, {r0-r4, r9, r12, lr}

	ldr r1,=PRG_ROM_Cache
    ldr r1,[r1]
	sub r2,r1,#0x8000
	str r2,memmap_tbl+16
    add r2,r1,#0x2000
    sub r2,r2,#0xA000
	str r2,memmap_tbl+20

flush		;update m6502_pc & lastbank
	ldr r1,lastbank
	sub r9,r9,r1
	and r1,r9,#0xE000	   
	adr r2,memmap_tbl		   
	lsr r1,r1,#11				
	ldr r0,[r2,r1]				

	str r0,lastbank				 
	add r9,r9,r0	
	orr lr,#0x01		
	bx lr

;----------------------------------------------------------------------------
mapCDEF_  ; r0=16k page#
;----------------------------------------------------------------------------
    stmfd sp!, {r0-r4, r9, r12, lr}
    mov r1, r0, lsl #1  
    mov r0, #2
    bl load_prg_bank_c
    ldmfd sp!, {r0-r4, r9, r12, lr}

    stmfd sp!, {r0-r4, r9, r12, lr}
    mov r1, r0, lsl #1
    add r1, r1, #1
    mov r0, #3
    bl load_prg_bank_c
    ldmfd sp!, {r0-r4, r9, r12, lr}

	ldr r1,=PRG_ROM_Cache
    ldr r1,[r1]
    add r2,r1,#0x4000
	sub r2,r2,#0xC000
	str r2,memmap_tbl+24
    add r2,r1,#0x6000
    sub r2,r2,#0xE000
	str r2,memmap_tbl+28
	b flush
    
;----------------------------------------------------------------------------
map89ABCDEF_ ; r0=32k page#
;----------------------------------------------------------------------------
    stmfd sp!, {r0-r4, r9, r12, lr}
    mov r1, r0, lsl #2  ; bank_num = r0 * 4
    mov r0, #0
    bl load_prg_bank_c
    ldmfd sp!, {r0-r4, r9, r12, lr}

    stmfd sp!, {r0-r4, r9, r12, lr}
    mov r1, r0, lsl #2
    add r1, r1, #1
    mov r0, #1
    bl load_prg_bank_c
    ldmfd sp!, {r0-r4, r9, r12, lr}

    stmfd sp!, {r0-r4, r9, r12, lr}
    mov r1, r0, lsl #2
    add r1, r1, #2
    mov r0, #2
    bl load_prg_bank_c
    ldmfd sp!, {r0-r4, r9, r12, lr}

    stmfd sp!, {r0-r4, r9, r12, lr}
    mov r1, r0, lsl #2
    add r1, r1, #3
    mov r0, #3
    bl load_prg_bank_c
    ldmfd sp!, {r0-r4, r9, r12, lr}

	ldr r1,=PRG_ROM_Cache
    ldr r1,[r1]
	sub r2,r1,#0x8000
	str r2,memmap_tbl+16
    add r2,r1,#0x2000
    sub r2,r2,#0xA000
	str r2,memmap_tbl+20
    add r2,r1,#0x4000
    sub r2,r2,#0xC000
	str r2,memmap_tbl+24
    add r2,r1,#0x6000
    sub r2,r2,#0xE000
	str r2,memmap_tbl+28
	b flush

;*************************************************************************************
     IMPORT asm_Mapper_Write;
Mapper_W	
;-------------------------------------------
	stmfd sp!,{r3,lr}	
	mov r1,r12
	bl asm_Mapper_Write
	ldmfd sp!,{r3,lr}
	orr lr,#0x01		
	bx lr
;---------------------------------------------------------------------------------------
    END
