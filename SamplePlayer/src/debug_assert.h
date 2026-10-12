#pragma once

/// TODO: clean this up, potentially attempt a serial
/// print to any/all targets as well

#ifdef DEBUG
#define DAISY_DEBUG_ASSERT_ENABLED 1
#else
#define DAISY_DEBUG_ASSERT_ENABLED 0
#endif

#if DAISY_DEBUG_ASSERT_ENABLED

// Halts the core if a debugger is attached, otherwise stops here
// instead of taking a HardFault (bkpt with no debugger attached faults).
static inline void debug_assert_trap(void)
{
#ifdef CoreDebug_DHCSR_C_DEBUGEN_Msk
    if(CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk)
    {
        __asm volatile("bkpt 255");
    }
#else
    __asm volatile("bkpt 255");
#endif
    while(1) {
    } // TODO: blink an LED or similar here if you want a visible signal
}

#define debug_assert(expr)       \
    do                           \
    {                            \
        if(!(expr))              \
        {                        \
            debug_assert_trap(); \
        }                        \
    } while(0)

#else

// sizeof doesn't evaluate expr, so there are no side effects and
// no unused-variable warnings when the assert is compiled out.
#define debug_assert(expr) ((void)sizeof(expr))

#endif