#ifndef _HYVEMIND_X64_VM_VCPU_H
#define _HYVEMIND_X64_VM_VCPU_H

#include "asm/vcpu_guest_reg_state.h"

struct vcpu;

void reset_guest_reg_state_to_init_state(struct vcpu_guest_reg_state *state);
void init_guest_reg_state_mirroring_host(struct vcpu_guest_reg_state *state);
void init_guest_reg_state_for_linux_32bit(struct vcpu_guest_reg_state *state);

void vcpu_step_rip(struct vcpu *vcpu);
void vcpu_handle_cpuid(struct vcpu *vcpu);
void vcpu_handle_xsetbv(struct vcpu *vcpu);
void vcpu_handle_rdmsr(struct vcpu *vcpu);

#endif /* _HYVEMIND_X64_VM_VCPU_H */

