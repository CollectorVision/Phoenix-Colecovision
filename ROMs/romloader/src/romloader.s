;
; Not used, switched to C for speed and convenience.
; Notes only.
;



; SD-cards power up in SD mode.  The card will enter SPI mode if the CS_n is
; asserted low during the reception of CMD0.  If the card requires SD mode,
; it will not respond to the command and remain in SD mode.

; CMD8 is used to send the currently used voltage in the VHS field.
; An error response means V1.x card.
; Otherwise, V2.x card, if the card can use the specified voltage, is echoes back the arguments.

; Reading the OCR (CMD58) is useful for V1.x cards to make sure they
; support the Phoenix voltage (3.3V), since V1.x cards do not have CMD8.

;
; SD-card initialization
;
; 1. Send at least 74 clocks with CS_n high.
;
;

; Between commands, send 8 clocks with CS_n high.

; Spec says:
;
; 1. CMD0 + CS_n
; 2. CMD8, send voltage, card will like it or indicate not valid
; 3. CMD58, read OCR for supported voltage ranges the card supports
; 4. ACMD41, until not busy
; 5. CMD58, read CCS (card capacity info)


; 1   CMD0 arg: 0x0, CRC: 0x95 (response: 0x01)
;
; 2   CMD8 arg: 0x000001AA, CRC: 0x87 (response: 0x01)
;
; 3   CMD55 arg: 0x0, CRC: any (CMD55 being the prefix to every ACMD)
;
; 4   ACMD41 , arg: 0x40000000, CRC: any
;
; 5   if response: 0x0, you're OK; if it's 0x1, goto 3.



; Used SD-card commands
CMD0    = 0         ; R1  go idle state
CMD8    = 8         ; R7  send interface condition (IC)
CMD9    = 9         ; R1  send card specific data (CSD)
CMD10   = 10        ; R1  send card identification (CID)
CMD12   = 12        ; R1b stop transmission
CMD13   = 13        ; R2  read status register (SR)
CMD17   = 17        ; R1  read single block
CMD18   = 18        ; R1  read multiple blocks
CMD24   = 24        ; R1  write block
CMD25   = 25        ; R1  write multiple blocks
CMD27   = 27        ; R1  program CSD
CMD55   = 55        ; R1  app cmd (next command is and application specific command)
CMD58   = 58        ; R3  read OCR
ACMD41  = 41        ; R1  send op cond, send host capacity and activate card initialization



; Commands are always "01" followed by the 6-bit command, four bytes of
; argument values, a 7-bit CRC, and a stop bit of "1".


; Registers
;
; IC (Interface Condition)
;   31..12 reserved
;   11..8  supply voltage VHS
;    7..0  check pattern
;
;
; OCR
;   31     card power up status bit (busy=0)
;   30     card capacity CCS (valid only if busy=1)
;   29     UHS-II card status
;   28..25 reserved
;   24     switching to 1.8V accepted
;   23     3.5V - 3.6V
;   22     3.4V - 3.5V
;   21     3.3V - 3.4V
;   20     3.2V - 3.3V
;   19     3.1V - 3.2V
;   18     3.0V - 3.1V
;   17     2.9V - 3.0V
;   16     2.8V - 2.9V
;   15     2.7V - 2.8V
;   14..0  reserved
;
;
; CID (Card Identification)
;
;   offset    size    description
;  -------------------------------------
;  127..120  1 byte   Manufacturer ID
;  119..104  2 byte   OEM / Application ID
;  103..64   5 byte   Product name
;   63..56   1 byte   Product revision
;   55..24   4 byte   Product serial number
;   23..20   4 bits   reserved
;   19..8   12 bits   Manufacturing date
;    7..1    7 bits   CRC
;    0..0    1 bit    Always "1"


; Responses
;
; R1 Format
;
;   bit  mask  description
;    7   >80   0
;    6   >40   parameter error
;    5   >20   address error
;    4   >10   erase sequence error
;    3   >08   com crc error
;    2   >04   illegal command
;    1   >02   erase reset
;    0   >01   in idle state
;
;
; R1b Format
;   An R1 response followed by a busy signal token that can be any number
;   of bytes.  A zero value byte indicates the card is busy.  A non-zero
;   value byte indicates the card is ready for the next command.
;
;
; R2 Format
;   Two bytes sent in response to the Send Status command.
;   The first byte is an R1 response, the second byte is:
;
;   bit  mask  description
;    7   >80   out of range or csd overwrite
;    6   >40   erase param
;    5   >20   wp violation
;    4   >10   card ecc failed
;    3   >08   CC error
;    2   >04   error
;    1   >02   wp erase skip or lock/unlock cmd failed
;    0   >01   card is locked
;
; R3 Format
;   An R1 response followed by four bytes of the OCR.
;
;
; R7 Format
;   An R1 response followed by four bytes:
;
;   39..32 R1 Format byte
;   31..28 command version
;   27..12 reserved
;   11..8  voltage accepted (3.3V = 0001)
;    7..0  check pattern


; ========
; Equates
STACK           EQU 63FFh       ; Stack location, bottom of 1K RAM
VDP_DATA_PORT   EQU 0BEh        ; VDP data port
VDP_CTRL_PORT   EQU 0BFh        ; VDP control port
SD_CTRL_PORT    EQU 56h         ; SC-card control port


; ========
; RAM Variables
;

nmi_cnt         EQU 6000h       ; NMI counter, 8-bit
main_cnt        EQU 6001h       ; main loop counter, 8-bit
sd_cnt          EQU 6002h       ; counter for SD-card detect, 8-bit
sd_dly          EQU 6003h       ; detect delay, 8-bit
sd_flag         EQU 6004h       ; the SD-card detect bit
print_loc       EQU 6005h       ; current print location, 16-bit


; ========
; Z80 power-on address 0000h
;
    ORG 0000h

    ld sp, STACK
    ; Skip the interrupt vector table areas
    jp main
; ========


; ========
; NMI vector
;
    ORG 0066h
    push af

    ; Update NMI counter.
    ld a, (nmi_cnt)
    inc a
    ld (nmi_cnt), a

    pop af
    retn
; ========


; ========
; Main
;
main:

    ; Initializing variables
    ld a, 0
    ld de, 0
    ld (nmi_cnt), a
    ld (main_cnt), a
    ld (sd_cnt), a
    ld (sd_dly), a
    ld (sd_flag), a
    ld (print_loc), de

    ; Enable the VDP interrupt.
    ld de, 01E0h ; VR1 = E0h
    call vdp_wtr

    ; Clear screen
    ld de, 0    ; dst
    ld a, 32    ; value
    ld bc, 768  ; count
    call vdp_memset

    ld hl, STR_HELLO
    call print_str_sp

    ld hl, STR_WORLD
    call print_str_nl

    forever:
        ; If the NMI has run, read VDP status to clear frame flag
        ld hl, main_cnt
        ld a, (nmi_cnt)
        cp (hl)
        jr z, forever_1
            ld (hl), a ; main_cnt = nmi_cnt
            in a, (VDP_CTRL_PORT)

        forever_1:

        ; Load the SD-card status
        ld hl, 32
        call print_pos
        call sd_card_detect

        bit 7, a
        jr z, sd_none
            ld hl, STR_SD_YES
            call print_str_nl
            jp sd_check_end
        sd_none:
            ld hl, STR_SD_NO
            call print_str_nl
        sd_check_end:

    jp forever
; ========


; ========
; Data area
;

STR_SD_YES: defb STR_YES_END-$-1, "SD YES"
STR_YES_END EQU $

STR_SD_NO: defb STR_NO_END-$-1, "SD NO "
STR_NO_END EQU $

STR_HELLO: defb STR_HELLO_END-$-1, "HELLO"
STR_HELLO_END EQU $

STR_WORLD: defb STR_WORLD_END-$-1, "WORLD"
STR_WORLD_END EQU $


; ========
; SD-card detect
;
; Sets A = 0 if no card, A != 0 if card detected
;
sd_card_detect:
    ; if sd_dly == 0 then sample and return
    ld a, (sd_dly)
    cp 0
    jr nz, sd_card_detect_1
        inc a
        ld (sd_dly), a  ; sd_dly += 1

        in a, (SD_CTRL_PORT)
        and 80h
        ld (sd_flag), a ; sd_flag = SD-card detect bit

        ld a, 0
        jp sd_card_done

    sd_card_detect_1:

    ; if sd_dly < 4 then count this attempt if enough time has passed,
    ; i.e. the NMI has run since the last time this function was called.
    cp 4
    jr z, sd_card_detect_3
        ; if nmi_cnt - sd_cnt == 0 then return
        ld hl, sd_cnt
        ld a, (nmi_cnt)
        cp (hl)
        jr z, sd_card_detect_2
            ; NMI has run, so count this call and return
            ld (hl), a  ; sd_cnt = nmi_cnt
            ld hl, sd_dly
            inc (hl)    ; sd_dly += 1

        sd_card_detect_2:
        ld a, 0
        jp sd_card_done

    sd_card_detect_3:

    ; Enough time has passed, so compare the current SD-card detect
    ; status to the saved value.  If they are the same, return the
    ; status, otherwise the value is bouncing.
    ld a, 0
    ld (sd_dly), a  ; sd_dly = 0

    in a, (SD_CTRL_PORT)
    and 80h
    ld hl, sd_flag
    cp (hl)
    jr z, sd_card_done
        ; Not the same, sample again next time.
        ld a, 0
        jp sd_card_done

    ; Register A contains the status either 0 or 80h.
    sd_card_done:
    ret
; ========


; ========
; Prints a string to the screen at the current location.
;
; HL = pointer to the string to print
;
print_str:
    ; Set the length in BC
    ld b, 0
    ld c, (hl)

    ; Adjust HL to point to the start of the string
    inc hl

    ; Load the screen location into DE and IX, adjust IX by the length and
    ; store it back in the location variable.
    ld de, (print_loc)
    ld ix, (print_loc)
    add ix, bc
    ld (print_loc), ix

    call vdp_memcpy

    ret

print_str_sp:
    call print_str
    inc ix
    ld (print_loc), ix
    ret

print_str_nl:
    call print_str
print_nl:
    ld hl, (print_loc)
    ld bc, 32
    add hl, bc
    ld a, l
    and 0E0h
    ld l, a
print_pos:
    ld (print_loc), hl
    ret
; ========


; ========
; VDP Write to Register
; Writes a value to the specified VDP register.
;
; D = VDP register to write
; E = value to write to the VDP register
;
vdp_wtr:
    ; Writing a VDP register, ensure bits >C0 = "10"
    set 7, d
    res 6, d
    ld a, e
    out (VDP_CTRL_PORT), a
    ld a, d
    out (VDP_CTRL_PORT), a

    ret
; ========


; ========
; Set up a VDP read or write address
;
; DE = VRAM address to set
; Modifies DE and A
;
vdp_wr_addr:
    ; Setting a "write" address, ensure bit >40 = 1
    set 6, d
vdp_rd_addr:
    res 7, d
    ld a, e
    out (VDP_CTRL_PORT), a
    ld a, d
    out (VDP_CTRL_PORT), a

    ret
; ========


; ========
; Sets a block of VRAM to a value
;
; DE = VRAM destination address
;  A = value to set
; BC = number of bytes to be set to value
;
vdp_memset:
    push af
    ; Set the VRAM destination address in DE
    call vdp_wr_addr

    ; BC contains the loop count.  B and C need to be swapped, if B != 0
    ; then C needs to be incremented by 1 to make the loop count correct.
    ld a, c
    cp 0
    jr z, vdp_memset_1
        inc b
    vdp_memset_1:
    ld c, b
    ld b, a
    pop af

    ; Now A=data, B=count LSB, C=adjusted count MSB
    vdp_memset_lp:
        out (VDP_DATA_PORT), a
        djnz vdp_memset_lp ; B--, jnz

        dec c
    jr nz, vdp_memset_lp

    ret
; ========


; ========
; Copies data from CPU memory to VRAM
;
; DE = VRAM destination address
; HL = CPU RAM source address
; BC = data length
;
vdp_memcpy:
    ; Set the VRAM destination address in DE
    call vdp_wr_addr

    ; BC contains the loop count.  B and C need to be swapped, if B != 0
    ; then C needs to be incremented by 1 to make the loop count correct.
    ld d, b
    ld a, c
    cp 0
    jr z, vdp_memcpy_1
        inc d
    vdp_memcpy_1:
    ld b, c
    ld c, VDP_DATA_PORT

    ; Now B=count LSB, D=adjusted count MSB, C=port, HL=source
    vdp_memcpy_lp:
        otir ; (C)=(HL), B--, HL++, until B=0
        dec d
    jr nz, vdp_memcpy_lp

    ret
; ========



; ========
; Quick and dirty loader to swap in the CV BIOS
; Not part of the above blob of code.
;
LD HL, swapcode
LD DE, 0x6000
LD BC, end_swapcode - code
LDIR
JP 0x6000

swapcode:
IN A, (0x55)
JP 0x0000
end_swapcode:
DEFB 0
