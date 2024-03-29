/******************************************************************************
 *
 * Name: hwtimer.c - ACPI Power Management Timer Interface
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2013, Intel Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 */

#include "acpi.h"
#include "accommon.h"

#define _COMPONENT          ACPI_HARDWARE
        ACPI_MODULE_NAME    ("hwtimer")


#if (!ACPI_REDUCED_HARDWARE) /* Entire module */
/******************************************************************************
 *
 * FUNCTION:    AcpiGetTimerResolution
 *
 * PARAMETERS:  Resolution          - Where the resolution is returned
 *
 * RETURN:      Status and timer resolution
 *
 * DESCRIPTION: Obtains resolution of the ACPI PM Timer (24 or 32 bits).
 *
 ******************************************************************************/

ACPI_STATUS
AcpiGetTimerResolution (
    UINT32                  *Resolution)
{
    ACPI_FUNCTION_TRACE (AcpiGetTimerResolution);


    if (!Resolution)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    if ((AcpiGbl_FADT.Flags & ACPI_FADT_32BIT_TIMER) == 0)
    {
        *Resolution = 24;
    }
    else
    {
        *Resolution = 32;
    }

    return_ACPI_STATUS (AE_OK);
}

ACPI_EXPORT_SYMBOL (AcpiGetTimerResolution)


/******************************************************************************
 *
 * FUNCTION:    AcpiGetTimer
 *
 * PARAMETERS:  Ticks               - Where the timer value is returned
 *
 * RETURN:      Status and current timer value (ticks)
 *
 * DESCRIPTION: Obtains current value of ACPI PM Timer (in ticks).
 *
 ******************************************************************************/

ACPI_STATUS
AcpiGetTimer (
    UINT32                  *Ticks)
{
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE (AcpiGetTimer);


    if (!Ticks)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    Status = AcpiHwRead (Ticks, &AcpiGbl_FADT.XPmTimerBlock);

    return_ACPI_STATUS (Status);
}

ACPI_EXPORT_SYMBOL (AcpiGetTimer)


/******************************************************************************
 *
 * FUNCTION:    AcpiGetTimerDuration
 *
 * PARAMETERS:  StartTicks          - Starting timestamp
 *              EndTicks            - End timestamp
 *              TimeElapsed         - Where the elapsed time is returned
 *
 * RETURN:      Status and TimeElapsed
 *
 * DESCRIPTION: Computes the time elapsed (in microseconds) between two
 *              PM Timer time stamps, taking into account the possibility of
 *              rollovers, the timer resolution, and timer frequency.
 *
 *              The PM Timer's clock ticks at roughly 3.6 times per
 *              _microsecond_, and its clock continues through Cx state
 *              transitions (unlike many CPU timestamp counters) -- making it
 *              a versatile and accurate timer.
 *
 *              Note that this function accommodates only a single timer
 *              rollover. Thus for 24-bit timers, this function should only
 *              be used for calculating durations less than ~4.6 seconds
 *              (~20 minutes for 32-bit timers) -- calculations below:
 *
 *              2**24 Ticks / 3,600,000 Ticks/Sec = 4.66 sec
 *              2**32 Ticks / 3,600,000 Ticks/Sec = 1193 sec or 19.88 minutes
 *
 ******************************************************************************/

ACPI_STATUS
AcpiGetTimerDuration (
    UINT32                  StartTicks,
    UINT32                  EndTicks,
    UINT32                  *TimeElapsed)
{
    ACPI_STATUS             Status;
    UINT32                  DeltaTicks;
    UINT64                  Quotient;


    ACPI_FUNCTION_TRACE (AcpiGetTimerDuration);


    if (!TimeElapsed)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    /*
     * Compute Tick Delta:
     * Handle (max one) timer rollovers on 24-bit versus 32-bit timers.
     */
    if (StartTicks < EndTicks)
    {
        DeltaTicks = EndTicks - StartTicks;
    }
    else if (StartTicks > EndTicks)
    {
        if ((AcpiGbl_FADT.Flags & ACPI_FADT_32BIT_TIMER) == 0)
        {
            /* 24-bit Timer */

            DeltaTicks = (((0x00FFFFFF - StartTicks) + EndTicks) & 0x00FFFFFF);
        }
        else
        {
            /* 32-bit Timer */

            DeltaTicks = (0xFFFFFFFF - StartTicks) + EndTicks;
        }
    }
    else /* StartTicks == EndTicks */
    {
        *TimeElapsed = 0;
        return_ACPI_STATUS (AE_OK);
    }

    /*
     * Compute Duration (Requires a 64-bit multiply and divide):
     *
     * TimeElapsed (microseconds) =
     *  (DeltaTicks * ACPI_USEC_PER_SEC) / ACPI_PM_TIMER_FREQUENCY;
     */
    Status = AcpiUtShortDivide (((UINT64) DeltaTicks) * ACPI_USEC_PER_SEC,
                ACPI_PM_TIMER_FREQUENCY, &Quotient, NULL);

    *TimeElapsed = (UINT32) Quotient;
    return_ACPI_STATUS (Status);
}

ACPI_EXPORT_SYMBOL (AcpiGetTimerDuration)

#endif /* !ACPI_REDUCED_HARDWARE */
