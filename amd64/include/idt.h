#ifndef _MACHINE_IDT_H_
#define _MACHINE_IDT_H_

#include <stdint.h>
#include <segments.h>
#include <trap.h>

#define IDT_ENTRIES 256

#define IDT_TYPE_INTERRUPT 0x0E /* 64-bit Interrupt Gate */
#define IDT_TYPE_TRAP 0x0F      /* 64-bit Trap Gate */

#define IDT_GATE_INTERRUPT 0x8E /* Interrupt gate, DPL=0, P=1 */
#define IDT_GATE_TRAP 0x8F      /* Trap gate, DPL=0, P=1 */
#define IDT_GATE_USER_INT 0xEE  /* Interrupt gate, DPL=3, P=1 */
#define IDT_GATE_USER_TRAP 0xEF /* Trap gate, DPL=3, P=1 */

#define IDT_IST_NONE 0
#define IDT_IST_DOUBLEFLT 1 /* Double fault uses dedicated stack */
#define IDT_IST_NMI 2       /* NMI uses dedicated stack */
#define IDT_IST_MCE 3       /* Machine check uses dedicated stack */

void idt_init(void);
void idt_set_gate(uint8_t vec,
                  uint64_t handler,
                  uint16_t selector,
                  uint8_t flags,
                  uint8_t ist);
void idt_load(uint64_t idt_ptr);

#endif