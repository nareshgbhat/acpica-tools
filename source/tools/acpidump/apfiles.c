/******************************************************************************
 *
 * Module Name: apfiles - File-related functions for acpidump utility
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

#include "acpidump.h"
#include "acapps.h"


/******************************************************************************
 *
 * FUNCTION:    ApOpenOutputFile
 *
 * PARAMETERS:  Pathname            - Output filename
 *
 * RETURN:      Open file handle
 *
 * DESCRIPTION: Open a text output file for acpidump. Checks if file already
 *              exists.
 *
 ******************************************************************************/

int
ApOpenOutputFile (
    char                    *Pathname)
{
    struct stat             StatInfo;
    FILE                    *File;


    /* If file exists, prompt for overwrite */

    if (!stat (Pathname, &StatInfo))
    {
        fprintf (stderr, "Target path already exists, overwrite? [y|n] ");

        if (getchar () != 'y')
        {
            return (-1);
        }
    }

    /* Point stdout to the file */

    File = freopen (Pathname, "w", stdout);
    if (!File)
    {
        perror ("Could not open output file");
        return (-1);
    }

    /* Save the file and path */

    Gbl_OutputFile = File;
    Gbl_OutputFilename = Pathname;
    return (0);
}


/******************************************************************************
 *
 * FUNCTION:    ApWriteToBinaryFile
 *
 * PARAMETERS:  Table               - ACPI table to be written
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Write an ACPI table to a binary file. Builds the output
 *              filename from the table signature.
 *
 ******************************************************************************/

int
ApWriteToBinaryFile (
    ACPI_TABLE_HEADER       *Table)
{
    char                    Filename[ACPI_NAME_SIZE + 16];
    char                    SsdtInstance [16];
    FILE                    *File;
    size_t                  Actual;


    /* Construct lower-case filename from the table signature */

    Filename[0] = (char) ACPI_TOLOWER (Table->Signature[0]);
    Filename[1] = (char) ACPI_TOLOWER (Table->Signature[1]);
    Filename[2] = (char) ACPI_TOLOWER (Table->Signature[2]);
    Filename[3] = (char) ACPI_TOLOWER (Table->Signature[3]);
    Filename[ACPI_NAME_SIZE] = 0;

    /* Handle multiple SSDTs - create different filenames for each */

    if (ACPI_COMPARE_NAME (Table->Signature, ACPI_SIG_SSDT))
    {
        sprintf (SsdtInstance, "%u", Gbl_SsdtCount);
        strcat (Filename, SsdtInstance);
        Gbl_SsdtCount++;
    }

    strcat (Filename, ACPI_TABLE_FILE_SUFFIX);

    if (Gbl_VerboseMode)
    {
        fprintf (stderr,
            "Writing [%4.4s] to binary file: %s 0x%X (%u) bytes\n",
            Table->Signature, Filename, Table->Length, Table->Length);
    }

    /* Open the file and dump the entire table in binary mode */

    File = fopen (Filename, "wb");
    if (!File)
    {
        perror ("Could not open output file");
        return (-1);
    }

    Actual = fwrite (Table, 1, Table->Length, File);
    if (Actual != Table->Length)
    {
        perror ("Error writing binary output file");
        fclose (File);
        return (-1);
    }

    fclose (File);
    return (0);
}


/******************************************************************************
 *
 * FUNCTION:    ApGetTableFromFile
 *
 * PARAMETERS:  Pathname            - File containing the binary ACPI table
 *              OutFileSize         - Where the file size is returned
 *
 * RETURN:      Buffer containing the ACPI table. NULL on error.
 *
 * DESCRIPTION: Open a file and read it entirely into a new buffer
 *
 ******************************************************************************/

ACPI_TABLE_HEADER *
ApGetTableFromFile (
    char                    *Pathname,
    UINT32                  *OutFileSize)
{
    ACPI_TABLE_HEADER       *Buffer = NULL;
    FILE                    *File;
    UINT32                  FileSize;
    size_t                  Actual;


    /* Must use binary mode */

    File = fopen (Pathname, "rb");
    if (!File)
    {
        perror ("Could not open input file");
        return (NULL);
    }

    /* Need file size to allocate a buffer */

    FileSize = ApGetFileSize (File);
    if (!FileSize)
    {
        fprintf (stderr,
            "Could not get input file size: %s\n", Pathname);
        goto Cleanup;
    }

    /* Allocate a buffer for the entire file */

    Buffer = calloc (1, FileSize);
    if (!Buffer)
    {
        fprintf (stderr,
            "Could not allocate file buffer of size: %u\n", FileSize);
        goto Cleanup;
    }

    /* Read the entire file */

    Actual = fread (Buffer, 1, FileSize, File);
    if (Actual != FileSize)
    {
        fprintf (stderr,
            "Could not read input file: %s\n", Pathname);
        free (Buffer);
        Buffer = NULL;
        goto Cleanup;
    }

    *OutFileSize = FileSize;

Cleanup:
    fclose (File);
    return (Buffer);
}


/******************************************************************************
 *
 * FUNCTION:    ApGetFileSize
 *
 * PARAMETERS:  File                - Open file descriptor
 *
 * RETURN:      File size in bytes
 *
 * DESCRIPTION: Get the size of an open file
 *
 ******************************************************************************/

UINT32
ApGetFileSize (
    FILE                    *File)
{
    UINT32                  FileSize;
    long                    Offset;


    Offset = ftell (File);
    if (fseek (File, 0, SEEK_END))
    {
        return (0);
    }

    /* Get size and restore file pointer */

    FileSize = (UINT32) ftell (File);
    if (fseek (File, Offset, SEEK_SET))
    {
        return (0);
    }

    return (FileSize);
}
