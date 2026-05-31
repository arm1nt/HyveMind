#ifndef _HYVEMIND_X64_VMX_VMX_H
#define _HYVEMIND_X64_VMX_VMX_H

#include <stdbool.h>

bool enter_vmx_operation(void);

void disable_vmx_operation(void);

#endif /* _HYVEMIND_X64_VMX_VMX_H */

