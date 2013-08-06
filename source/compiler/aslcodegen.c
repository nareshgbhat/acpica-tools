/******************************************************************************
 *
 * Module Name: aslcodegen - AML code generation
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


#include "aslcompiler.h"
#include "aslcompiler.y.h"
#include "amlcode.h"

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslcodegen")

/* Local prototypes */

static ACPI_STATUS
CgAmlWriteWalk (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);

static void
CgLocalWriteAmlData (
    ACPI_PARSE_OBJECT       *Op,
    void                    *Buffer,
    UINT32                  Length);

static void
CgWriteAmlOpcode (
    ACPI_PARSE_OBJECT       *Op);

static void
CgWriteTableHeader (
    ACPI_PARSE_OBJECT       *Op);

static void
CgCloseTable (
    void);

static void
CgWriteNode (
    ACPI_PARSE_OBJECT       *Op);


/*******************************************************************************
 *
 * FUNCTION:    CgGenerateAmlOutput
 *
 * PARAMETERS:  None.
 *
 * RETURN:      None
 *
 * DESCRIPTION: Generate AML code. Currently generates the listing file
 *              simultaneously.
 *
 ******************************************************************************/

void
CgGenerateAmlOutput (
    void)
{

    DbgPrint (ASL_DEBUG_OUTPUT, "\nWriting AML\n\n");

    /* Generate the AML output file */

    FlSeekFile (ASL_FILE_SOURCE_OUTPUT, 0);
    Gbl_SourceLine = 0;
    Gbl_NextError = Gbl_ErrorLog;

    TrWalkParseTree (RootNode, ASL_WALK_VISIT_DOWNWARD,
        CgAmlWriteWalk, NULL, NULL);
    CgCloseTable ();
}


/*******************************************************************************
 *
 * FUNCTION:    CgAmlWriteWalk
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Parse tree walk to generate the AML code.
 *
 ******************************************************************************/

static ACPI_STATUS
CgAmlWriteWalk (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{

    /*
     * Print header at level 0. Alignment assumes 32-bit pointers
     */
    if (!Level)
    {
        DbgPrint (ASL_TREE_OUTPUT,
            "Final parse tree used for AML output:\n");
        DbgPrint (ASL_TREE_OUTPUT,
            "%*s Value    P_Op A_Op OpLen PByts Len  SubLen PSubLen OpPtr    Child    Parent   Flags    AcTyp    Final Col L\n",
            76, " ");
    }

    /* Debug output */

    DbgPrint (ASL_TREE_OUTPUT,
        "%5.5d [%2d]", Op->Asl.LogicalLineNumber, Level);
    UtPrintFormattedName (Op->Asl.ParseOpcode, Level);

    if (Op->Asl.ParseOpcode == PARSEOP_NAMESEG    ||
        Op->Asl.ParseOpcode == PARSEOP_NAMESTRING ||
        Op->Asl.ParseOpcode == PARSEOP_METHODCALL)
    {
        DbgPrint (ASL_TREE_OUTPUT,
            "%10.32s      ", Op->Asl.ExternalName);
    }
    else
    {
        DbgPrint (ASL_TREE_OUTPUT, "                ");
    }

    DbgPrint (ASL_TREE_OUTPUT,
    "%08X %04X %04X %01X     %04X  %04X %04X   %04X    %08X %08X %08X %08X %08X %04X  %02d  %02d\n",
            /* 1  */ (UINT32) Op->Asl.Value.Integer,
            /* 2  */ Op->Asl.ParseOpcode,
            /* 3  */ Op->Asl.AmlOpcode,
            /* 4  */ Op->Asl.AmlOpcodeLength,
            /* 5  */ Op->Asl.AmlPkgLenBytes,
            /* 6  */ Op->Asl.AmlLength,
            /* 7  */ Op->Asl.AmlSubtreeLength,
            /* 8  */ Op->Asl.Parent ? Op->Asl.Parent->Asl.AmlSubtreeLength : 0,
            /* 9  */ Op,
            /* 10 */ Op->Asl.Child,
            /* 11 */ Op->Asl.Parent,
            /* 12 */ Op->Asl.CompileFlags,
            /* 13 */ Op->Asl.AcpiBtype,
            /* 14 */ Op->Asl.FinalAmlLength,
            /* 15 */ Op->Asl.Column,
            /* 16 */ Op->Asl.LineNumber);

    /* Generate the AML for this node */

    CgWriteNode (Op);
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    CgLocalWriteAmlData
 *
 * PARAMETERS:  Op              - Current parse op
 *              Buffer          - Buffer to write
 *              Length          - Size of data in buffer
 *
 * RETURN:      None
 *
 * DESCRIPTION: Write a buffer of AML data to the AML output file.
 *
 ******************************************************************************/

static void
CgLocalWriteAmlData (
    ACPI_PARSE_OBJECT       *Op,
    void                    *Buffer,
    UINT32                  Length)
{

    /* Write the raw data to the AML file */

    FlWriteFile (ASL_FILE_AML_OUTPUT, Buffer, Length);

    /* Update the final AML length for this node (used for listings) */

    if (Op)
    {
        Op->Asl.FinalAmlLength += Length;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    CgWriteAmlOpcode
 *
 * PARAMETERS:  Op            - Parse node with an AML opcode
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Write the AML opcode corresponding to a parse node.
 *
 ******************************************************************************/

static void
CgWriteAmlOpcode (
    ACPI_PARSE_OBJECT       *Op)
{
    UINT8                   PkgLenFirstByte;
    UINT8                   Byte;
    UINT16                  Word;
    UINT32                  DWord;
    UINT64                  QWord;
    UINT16                  AmlOpcode;
    UINT32                  PkgLen;

    /* We expect some DEFAULT_ARGs, just ignore them */

    if (Op->Asl.ParseOpcode == PARSEOP_DEFAULT_ARG)
    {
        return;
    }

    switch (Op->Asl.AmlOpcode)
    {
    case AML_UNASSIGNED_OPCODE:

        /* These opcodes should not get here */

        printf ("Found a node with an unassigned AML opcode\n");
        FlPrintFile (ASL_FILE_STDERR, "Found a node with an unassigned AML opcode\n");
        return;

    case AML_INT_RESERVEDFIELD_OP:

        /* Special opcodes for within a field definition */

        AmlOpcode = AML_FIELD_OFFSET_OP;
        break;

    case AML_INT_ACCESSFIELD_OP:

        AmlOpcode = AML_FIELD_ACCESS_OP;
        break;

    case AML_INT_CONNECTION_OP:

        AmlOpcode = AML_FIELD_CONNECTION_OP;
        break;

    default:

        AmlOpcode = Op->Asl.AmlOpcode;
        break;
    }


    switch (AmlOpcode)
    {
    case AML_PACKAGE_LENGTH:

        /* Value is the length to be encoded (Used in field definitions) */

        PkgLen = (UINT32) Op->Asl.Value.Integer;
        break;

    default:

        /* Check for two-byte opcode */

        if (AmlOpcode > 0x00FF)
        {
            /* Write the high byte first */
	    Byte = ACPI_HIBYTE(AmlOpcode);
	    CgLocalWriteAmlData (Op, &Byte, 1);
        }

	Byte = ACPI_LOBYTE(AmlOpcode);
        CgLocalWriteAmlData (Op, &Byte, 1);

        /* Subtreelength doesn't include length of package length bytes */

        PkgLen = Op->Asl.AmlSubtreeLength + Op->Asl.AmlPkgLenBytes;
        break;
    }

    /* Does this opcode have an associated "PackageLength" field? */

    if (Op->Asl.CompileFlags & NODE_AML_PACKAGE)
    {
        if (Op->Asl.AmlPkgLenBytes == 1)
        {
            /* Simplest case -- no bytes to follow, just write the count */
            Byte = ACPI_LOBYTE(PkgLen);
            CgLocalWriteAmlData (Op, &Byte, 1);
        }
        else if (Op->Asl.AmlPkgLenBytes != 0)
        {
            /*
             * Encode the "bytes to follow" in the first byte, top two bits.
             * The low-order nybble of the length is in the bottom 4 bits
             */
            PkgLenFirstByte = (UINT8)
                (((UINT32) (Op->Asl.AmlPkgLenBytes - 1) << 6) |
                (PkgLen & 0x0F));

            CgLocalWriteAmlData (Op, &PkgLenFirstByte, 1);

            /*
             * Shift the length over by the 4 bits we just stuffed
             * in the first byte
             */
            PkgLen >>= 4;

            /* Now we can write the remaining bytes - either 1, 2, or 3 bytes */
            Byte = ACPI_LOBYTE(PkgLen);
            CgLocalWriteAmlData (Op, &Byte, 1);
            if (Op->Asl.AmlPkgLenBytes >= 3)
            {
                Byte = ACPI_HIBYTE(PkgLen);
                CgLocalWriteAmlData (Op, &Byte, 1);
            }
            if (Op->Asl.AmlPkgLenBytes >= 4)
            {
                Byte = ACPI_LOBYTE(ACPI_HIWORD(PkgLen));
                CgLocalWriteAmlData (Op, &Byte, 1);
            }
        }
    }

    switch (AmlOpcode)
    {
    case AML_BYTE_OP:
        Byte = (UINT8) Op->Asl.Value.Integer;
        CgLocalWriteAmlData (Op, &Byte, 1);
        break;

    case AML_WORD_OP:
        ACPI_MOVE_64_TO_16(&Word, &Op->Asl.Value.Integer);
        CgLocalWriteAmlData (Op, &Word, 2);
       break;

    case AML_DWORD_OP:
        ACPI_MOVE_64_TO_32(&DWord, &Op->Asl.Value.Integer);
        CgLocalWriteAmlData (Op, &DWord, 4);
        break;

    case AML_QWORD_OP:
        ACPI_MOVE_64_TO_64(&QWord, &Op->Asl.Value.Integer);
        CgLocalWriteAmlData (Op, &QWord, 8);
        break;

    case AML_STRING_OP:

        CgLocalWriteAmlData (Op, Op->Asl.Value.String, Op->Asl.AmlLength);
        break;

    default:

        /* All data opcodes must appear above */

        break;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    CgWriteTableHeader
 *
 * PARAMETERS:  Op        - The DEFINITIONBLOCK node
 *
 * RETURN:      None
 *
 * DESCRIPTION: Write a table header corresponding to the DEFINITIONBLOCK
 *
 ******************************************************************************/

static void
CgWriteTableHeader (
    ACPI_PARSE_OBJECT       *Op)
{
    ACPI_PARSE_OBJECT       *Child;
    UINT32 DWord;


    /* AML filename */

    Child = Op->Asl.Child;

    /* Signature */

    Child = Child->Asl.Next;
    strncpy (TableHeader.Signature, Child->Asl.Value.String, 4);

    /* Revision */

    Child = Child->Asl.Next;
    TableHeader.Revision = (UINT8) Child->Asl.Value.Integer;

    /* Command-line Revision override */

    if (Gbl_RevisionOverride)
    {
        TableHeader.Revision = Gbl_RevisionOverride;
    }

    /* OEMID */

    Child = Child->Asl.Next;
    strncpy (TableHeader.OemId, Child->Asl.Value.String, 6);

    /* OEM TableID */

    Child = Child->Asl.Next;
    strncpy (TableHeader.OemTableId, Child->Asl.Value.String, 8);

    /* OEM Revision */

    Child = Child->Asl.Next;
    ACPI_MOVE_64_TO_32(&TableHeader.OemRevision, &Child->Asl.Value.Integer);

    /* Compiler ID */

    ACPI_MOVE_NAME (TableHeader.AslCompilerId, ASL_CREATOR_ID);

    /* Compiler version */

    DWord = ASL_REVISION;
    ACPI_MOVE_32_TO_32(&TableHeader.AslCompilerRevision, &DWord);

    /* Table length. Checksum zero for now, will rewrite later */

    ACPI_MOVE_32_TO_32(&TableHeader.Length, &Gbl_TableLength);
    TableHeader.Checksum = 0;

    CgLocalWriteAmlData (Op, &TableHeader, sizeof (ACPI_TABLE_HEADER));
}


/*******************************************************************************
 *
 * FUNCTION:    CgCloseTable
 *
 * PARAMETERS:  None.
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Complete the ACPI table by calculating the checksum and
 *              re-writing the header.
 *
 ******************************************************************************/

static void
CgCloseTable (
    void)
{
    signed char         Sum;
    UINT8               FileByte;


    FlSeekFile (ASL_FILE_AML_OUTPUT, 0);
    Sum = 0;

    /* Calculate the checksum over the entire file */

    while (FlReadFile (ASL_FILE_AML_OUTPUT, &FileByte, 1) == AE_OK)
    {
        Sum = (signed char) (Sum + FileByte);
    }

    /* Re-write the table header with the checksum */

    TableHeader.Checksum = (UINT8) (0 - Sum);

    FlSeekFile (ASL_FILE_AML_OUTPUT, 0);
    CgLocalWriteAmlData (NULL, &TableHeader, sizeof (ACPI_TABLE_HEADER));
}


/*******************************************************************************
 *
 * FUNCTION:    CgWriteNode
 *
 * PARAMETERS:  Op            - Parse node to write.
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Write the AML that corresponds to a parse node.
 *
 ******************************************************************************/

static void
CgWriteNode (
    ACPI_PARSE_OBJECT       *Op)
{
    ASL_RESOURCE_NODE       *Rnode;
    UINT8                   Byte;
    UINT16                  Word;
    UINT32                  DWord;
    UINT64                  QWord;

    /* Always check for DEFAULT_ARG and other "Noop" nodes */
    /* TBD: this may not be the best place for this check */

    if ((Op->Asl.ParseOpcode == PARSEOP_DEFAULT_ARG)  ||
        (Op->Asl.ParseOpcode == PARSEOP_EXTERNAL)     ||
        (Op->Asl.ParseOpcode == PARSEOP_INCLUDE)      ||
        (Op->Asl.ParseOpcode == PARSEOP_INCLUDE_END))
    {
        return;
    }

    Op->Asl.FinalAmlLength = 0;

    switch (Op->Asl.AmlOpcode)
    {
    case AML_RAW_DATA_BYTE:
        Byte = (UINT8) Op->Asl.Value.Integer;
        CgLocalWriteAmlData (Op, &Byte, 1);
        return;

    case AML_RAW_DATA_WORD:
        ACPI_MOVE_64_TO_16(&Word, &Op->Asl.Value.Integer);
        CgLocalWriteAmlData (Op, &Word, 2);
        return;

    case AML_RAW_DATA_DWORD:
        ACPI_MOVE_64_TO_32(&DWord, &Op->Asl.Value.Integer);
        CgLocalWriteAmlData (Op, &DWord, 4);
        return;

    case AML_RAW_DATA_QWORD:
        ACPI_MOVE_64_TO_64(&QWord, &Op->Asl.Value.Integer);
        CgLocalWriteAmlData (Op, &QWord, 8);
        return;

    case AML_RAW_DATA_BUFFER:

        CgLocalWriteAmlData (Op, Op->Asl.Value.Buffer, Op->Asl.AmlLength);
        return;


    case AML_RAW_DATA_CHAIN:

        Rnode = ACPI_CAST_PTR (ASL_RESOURCE_NODE, Op->Asl.Value.Buffer);
        while (Rnode)
        {
            CgLocalWriteAmlData (Op, Rnode->Buffer, Rnode->BufferLength);
            Rnode = Rnode->Next;
        }
        return;

    default:

        /* Internal data opcodes must all appear above */

        break;
    }

    switch (Op->Asl.ParseOpcode)
    {
    case PARSEOP_DEFAULT_ARG:

        break;

    case PARSEOP_DEFINITIONBLOCK:

        CgWriteTableHeader (Op);
        break;

    case PARSEOP_NAMESEG:
    case PARSEOP_NAMESTRING:
    case PARSEOP_METHODCALL:

        CgLocalWriteAmlData (Op, Op->Asl.Value.String, Op->Asl.AmlLength);
        break;

    default:

        CgWriteAmlOpcode (Op);
        break;
    }
}
