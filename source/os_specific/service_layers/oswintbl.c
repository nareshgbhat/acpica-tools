/******************************************************************************
 *
 * Module Name: oswintbl - Windows OSL for obtaining ACPI tables
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
#include <stdio.h>

#ifdef WIN32
#pragma warning(disable:4115)   /* warning C4115: (caused by rpcasync.h) */
#include <windows.h>

#elif WIN64
#include <windowsx.h>
#endif

#define _COMPONENT          ACPI_OS_SERVICES
        ACPI_MODULE_NAME    ("oswintbl")

/* Local prototypes */

static char *
WindowsFormatException (
    LONG                WinStatus);

/* Globals */

#define LOCAL_BUFFER_SIZE           64

static char             KeyBuffer[LOCAL_BUFFER_SIZE];
static char             ErrorBuffer[LOCAL_BUFFER_SIZE];

/*
 * Tables supported in the Windows registry. SSDTs are not placed into
 * the registry, a limitation.
 */
static char             *SupportedTables[] =
{
    "DSDT",
    "RSDT",
    "FACS",
    "FACP"
};

/* Max index for table above */

#define ACPI_OS_MAX_TABLE_INDEX     3


/******************************************************************************
 *
 * FUNCTION:    WindowsFormatException
 *
 * PARAMETERS:  WinStatus       - Status from a Windows system call
 *
 * RETURN:      Formatted (ascii) exception code. Front-end to Windows
 *              FormatMessage interface.
 *
 * DESCRIPTION: Decode a windows exception
 *
 *****************************************************************************/

static char *
WindowsFormatException (
    LONG                WinStatus)
{

    ErrorBuffer[0] = 0;
    FormatMessage (FORMAT_MESSAGE_FROM_SYSTEM, NULL, WinStatus, 0,
        ErrorBuffer, LOCAL_BUFFER_SIZE, NULL);

    return (ErrorBuffer);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsGetTableByAddress
 *
 * PARAMETERS:  Address         - Physical address of the ACPI table
 *              Table           - Where a pointer to the table is returned
 *
 * RETURN:      Status; Table buffer is returned if AE_OK.
 *              AE_NOT_FOUND: A valid table was not found at the address
 *
 * DESCRIPTION: Get an ACPI table via a physical memory address.
 *
 * NOTE:        Cannot be implemented without a Windows device driver.
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsGetTableByAddress (
    ACPI_PHYSICAL_ADDRESS   Address,
    ACPI_TABLE_HEADER       **Table)
{

    fprintf (stderr, "Get table by address is not supported on Windows\n");
    return (AE_SUPPORT);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsGetTableByIndex
 *
 * PARAMETERS:  Index           - Which table to get
 *              Table           - Where a pointer to the table is returned
 *              Address         - Where the table physical address is returned
 *
 * RETURN:      Status; Table buffer and physical address returned if AE_OK.
 *              AE_LIMIT: Index is beyond valid limit
 *
 * DESCRIPTION: Get an ACPI table via an index value (0 through n). Returns
 *              AE_LIMIT when an invalid index is reached. Index is not
 *              necessarily an index into the RSDT/XSDT.
 *              Table is obtained from the Windows registry.
 *
 * NOTE:        Cannot get the physical address from the windows registry;
 *              zero is returned instead.
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsGetTableByIndex (
    UINT32                  Index,
    ACPI_TABLE_HEADER       **Table,
    ACPI_PHYSICAL_ADDRESS   *Address)
{
    ACPI_STATUS             Status;


    if (Index > ACPI_OS_MAX_TABLE_INDEX)
    {
        return (AE_LIMIT);
    }

    Status = AcpiOsGetTableByName (SupportedTables[Index], 0, Table, Address);
    return (Status);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsGetTableByName
 *
 * PARAMETERS:  Signature       - ACPI Signature for desired table. Must be
 *                                a null terminated 4-character string.
 *              Instance        - For SSDTs (0...n). Use 0 otherwise.
 *              Table           - Where a pointer to the table is returned
 *              Address         - Where the table physical address is returned
 *
 * RETURN:      Status; Table buffer and physical address returned if AE_OK.
 *              AE_LIMIT: Instance is beyond valid limit
 *              AE_NOT_FOUND: A table with the signature was not found
 *
 * DESCRIPTION: Get an ACPI table via a table signature (4 ASCII characters).
 *              Returns AE_LIMIT when an invalid instance is reached.
 *              Table is obtained from the Windows registry.
 *
 * NOTE:        Assumes the input signature is uppercase.
 *              Cannot get the physical address from the windows registry;
 *              zero is returned instead.
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsGetTableByName (
    char                    *Signature,
    UINT32                  Instance,
    ACPI_TABLE_HEADER       **Table,
    ACPI_PHYSICAL_ADDRESS   *Address)
{
    HKEY                    Handle = NULL;
    LONG                    WinStatus;
    ULONG                   Type;
    ULONG                   NameSize;
    ULONG                   DataSize;
    HKEY                    SubKey;
    ULONG                   i;
    ACPI_TABLE_HEADER       *ReturnTable;


    /*
     * Windows has no SSDTs in the registry, so multiple instances are
     * not supported.
     */
    if (Instance > 0)
    {
        return (AE_LIMIT);
    }

    /* Get a handle to the table key */

    while (1)
    {
        ACPI_STRCPY (KeyBuffer, "HARDWARE\\ACPI\\");
        ACPI_STRCAT (KeyBuffer, Signature);

        WinStatus = RegOpenKeyEx (HKEY_LOCAL_MACHINE, KeyBuffer,
            0L, KEY_READ, &Handle);

        if (WinStatus != ERROR_SUCCESS)
        {
            /*
             * Somewhere along the way, MS changed the registry entry for
             * the FADT from
             * HARDWARE/ACPI/FACP  to
             * HARDWARE/ACPI/FADT.
             *
             * This code allows for both.
             */
            if (ACPI_COMPARE_NAME (Signature, "FACP"))
            {
                Signature = "FADT";
            }
            else if (ACPI_COMPARE_NAME (Signature, "XSDT"))
            {
                Signature = "RSDT";
            }
            else
            {
                fprintf (stderr,
                    "Could not find %s in registry at %s: %s (WinStatus=0x%X)\n",
                    Signature, KeyBuffer, WindowsFormatException (WinStatus), WinStatus);
                return (AE_NOT_FOUND);
            }
        }
        else
        {
            break;
        }
    }

    /* Actual data for the table is down a couple levels */

    for (i = 0; ;)
    {
        WinStatus = RegEnumKey (Handle, i, KeyBuffer, sizeof (KeyBuffer));
        i++;
        if (WinStatus == ERROR_NO_MORE_ITEMS)
        {
            break;
        }

        WinStatus = RegOpenKey (Handle, KeyBuffer, &SubKey);
        if (WinStatus != ERROR_SUCCESS)
        {
            fprintf (stderr, "Could not open %s entry: %s\n",
                Signature, WindowsFormatException (WinStatus));
            return (AE_ERROR);
        }

        RegCloseKey (Handle);
        Handle = SubKey;
        i = 0;
    }

    /* Find the (binary) table entry */

    for (i = 0; ; i++)
    {
        NameSize = sizeof (KeyBuffer);
        WinStatus = RegEnumValue (Handle, i, KeyBuffer, &NameSize, NULL,
            &Type, NULL, 0);
        if (WinStatus != ERROR_SUCCESS)
        {
            fprintf (stderr, "Could not get %s registry entry: %s\n",
                Signature, WindowsFormatException (WinStatus));
            return (AE_ERROR);
        }

        if (Type == REG_BINARY)
        {
            break;
        }
    }

    /* Get the size of the table */

    WinStatus = RegQueryValueEx (Handle, KeyBuffer, NULL, NULL,
        NULL, &DataSize);
    if (WinStatus != ERROR_SUCCESS)
    {
        fprintf (stderr, "Could not read the %s table size: %s\n",
            Signature, WindowsFormatException (WinStatus));
        return (AE_ERROR);
    }

    /* Allocate a new buffer for the table */

    ReturnTable = malloc (DataSize);
    if (!ReturnTable)
    {
        goto Cleanup;
    }

    /* Get the actual table from the registry */

    WinStatus = RegQueryValueEx (Handle, KeyBuffer, NULL, NULL,
        (UCHAR *) ReturnTable, &DataSize);
    if (WinStatus != ERROR_SUCCESS)
    {
        fprintf (stderr, "Could not read %s data: %s\n",
            Signature, WindowsFormatException (WinStatus));
        free (ReturnTable);
        return (AE_ERROR);
    }

Cleanup:
    RegCloseKey (Handle);

    *Table = ReturnTable;
    *Address = 0;
    return (AE_OK);
}
