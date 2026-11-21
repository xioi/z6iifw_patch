/* SPDX-License-Identifier: MIT */
.syntax unified
.thumb
.section .text.entry,"ax",%progbits
.global extension_init_hook
.thumb_func
extension_init_hook:
    push {r4, lr}
    ldr r0, =0xc1819b04
    ldr r1, =0x10450784
    ldr r3, =0x1035d32b
    blx r3
    bl extension_boot
    pop.w {r4, lr}
    ldr pc, =0x1045064b
.ltorg
.section .note.GNU-stack,"",%progbits
