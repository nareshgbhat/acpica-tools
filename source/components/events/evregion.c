/******************************************************************************
 *
 * Module Name: evregion - Operation Region support
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


#define __EVREGION_C__

#include "acpi.h"
#include "accommon.h"
#include "acevents.h"
#include "acnamesp.h"
#include "acinterp.h"

#define _COMPONENT          ACPI_EVENTS
        ACPI_MODULE_NAME    ("evregion")


extern UINT8        AcpiGbl_DefaultAddressSpaces[];

/* Local prototypes */

static void
AcpiEvOrphanEcRegMethod (
    ACPI_NAMESPACE_NODE     *EcDeviceNode);

static ACPI_STATUS
AcpiEvRegRun (
    ACPI_HANDLE             ObjHandle,
    UINT32                  Level,
    void                    *Context,
    void                    **ReturnValue);


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvInitializeOpRegions
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Execute _REG methods for all Operation Regions that have
 *              an installed default region handler.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiEvInitializeOpRegions (
    void)
{
    ACPI_STATUS             Status;
    UINT32                  i;


    ACPI_FUNCTION_TRACE (EvInitializeOpRegions);


    Status = AcpiUtAcquireMutex (ACPI_MTX_NAMESPACE);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Run the _REG methods for OpRegions in each default address space */

    for (i = 0; i < ACPI_NUM_DEFAULT_SPACES; i++)
    {
        /*
         * Make sure the installed handler is the DEFAULT handler. If not the
         * default, the _REG methods will have already been run (when the
         * handler was installed)
         */
        if (AcpiEvHasDefaultHandler (AcpiGbl_RootNode,
               AcpiGbl_DefaultAddressSpaces[i]))
        {
            Status = AcpiEvExecuteRegMethods (AcpiGbl_RootNode,
                        AcpiGbl_DefaultAddressSpaces[i]);
        }
    }

    AcpiGbl_RegMethodsExecuted = TRUE;

    (void) AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvAddressSpaceDispatch
 *
 * PARAMETERS:  RegionObj           - Internal region object
 *              FieldObj            - Corresponding field. Can be NULL.
 *              Function            - Read or Write operation
 *              RegionOffset        - Where in the region to read or write
 *              BitWidth            - Field width in bits (8, 16, 32, or 64)
 *              Value               - Pointer to in or out value, must be
 *                                    a full 64-bit integer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Dispatch an address space or operation region access to
 *              a previously installed handler.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiEvAddressSpaceDispatch (
    ACPI_OPERAND_OBJECT     *RegionObj,
    ACPI_OPERAND_OBJECT     *FieldObj,
    UINT32                  Function,
    UINT32                  RegionOffset,
    UINT32                  BitWidth,
    UINT64                  *Value)
{
    ACPI_STATUS             Status;
    ACPI_ADR_SPACE_HANDLER  Handler;
    ACPI_ADR_SPACE_SETUP    RegionSetup;
    ACPI_OPERAND_OBJECT     *HandlerDesc;
    ACPI_OPERAND_OBJECT     *RegionObj2;
    void                    *RegionContext = NULL;
    ACPI_CONNECTION_INFO    *Context;


    ACPI_FUNCTION_TRACE (EvAddressSpaceDispatch);


    RegionObj2 = AcpiNsGetSecondaryObject (RegionObj);
    if (!RegionObj2)
    {
        return_ACPI_STATUS (AE_NOT_EXIST);
    }

    /* Ensure that there is a handler associated with this region */

    HandlerDesc = RegionObj->Region.Handler;
    if (!HandlerDesc)
    {
        ACPI_ERROR ((AE_INFO,
            "No handler for Region [%4.4s] (%p) [%s]",
            AcpiUtGetNodeName (RegionObj->Region.Node),
            RegionObj, AcpiUtGetRegionName (RegionObj->Region.SpaceId)));

        return_ACPI_STATUS (AE_NOT_EXIST);
    }

    Context = HandlerDesc->AddressSpace.Context;

    /*
     * It may be the case that the region has never been initialized.
     * Some types of regions require special init code
     */
    if (!(RegionObj->Region.Flags & AOPOBJ_SETUP_COMPLETE))
    {
        /* This region has not been initialized yet, do it */

        RegionSetup = HandlerDesc->AddressSpace.Setup;
        if (!RegionSetup)
        {
            /* No initialization routine, exit with error */

            ACPI_ERROR ((AE_INFO,
                "No init routine for region(%p) [%s]",
                RegionObj, AcpiUtGetRegionName (RegionObj->Region.SpaceId)));
            return_ACPI_STATUS (AE_NOT_EXIST);
        }

        /*
         * We must exit the interpreter because the region setup will
         * potentially execute control methods (for example, the _REG method
         * for this region)
         */
        AcpiExExitInterpreter ();

        Status = RegionSetup (RegionObj, ACPI_REGION_ACTIVATE,
                    Context, &RegionContext);

        /* Re-enter the interpreter */

        AcpiExEnterInterpreter ();

        /* Check for failure of the Region Setup */

        if (ACPI_FAILURE (Status))
        {
            ACPI_EXCEPTION ((AE_INFO, Status,
                "During region initialization: [%s]",
                AcpiUtGetRegionName (RegionObj->Region.SpaceId)));
            return_ACPI_STATUS (Status);
        }

        /* Region initialization may have been completed by RegionSetup */

        if (!(RegionObj->Region.Flags & AOPOBJ_SETUP_COMPLETE))
        {
            RegionObj->Region.Flags |= AOPOBJ_SETUP_COMPLETE;

            if (RegionObj2->Extra.RegionContext)
            {
                /* The handler for this region was already installed */

                ACPI_FREE (RegionContext);
            }
            else
            {
                /*
                 * Save the returned context for use in all accesses to
                 * this particular region
                 */
                RegionObj2->Extra.RegionContext = RegionContext;
            }
        }
    }

    /* We have everything we need, we can invoke the address space handler */

    Handler = HandlerDesc->AddressSpace.Handler;

    ACPI_DEBUG_PRINT ((ACPI_DB_OPREGION,
        "Handler %p (@%p) Address %8.8X%8.8X [%s]\n",
        &RegionObj->Region.Handler->AddressSpace, Handler,
        ACPI_FORMAT_NATIVE_UINT (RegionObj->Region.Address + RegionOffset),
        AcpiUtGetRegionName (RegionObj->Region.SpaceId)));


    /*
     * Special handling for GenericSerialBus and GeneralPurposeIo:
     * There are three extra parameters that must be passed to the
     * handler via the context:
     *   1) Connection buffer, a resource template from Connection() op.
     *   2) Length of the above buffer.
     *   3) Actual access length from the AccessAs() op.
     */
    if (((RegionObj->Region.SpaceId == ACPI_ADR_SPACE_GSBUS) ||
            (RegionObj->Region.SpaceId == ACPI_ADR_SPACE_GPIO)) &&
        Context &&
        FieldObj)
    {
        /* Get the Connection (ResourceTemplate) buffer */

        Context->Connection = FieldObj->Field.ResourceBuffer;
        Context->Length = FieldObj->Field.ResourceLength;
        Context->AccessLength = FieldObj->Field.AccessLength;
    }

    if (!(HandlerDesc->AddressSpace.HandlerFlags &
            ACPI_ADDR_HANDLER_DEFAULT_INSTALLED))
    {
        /*
         * For handlers other than the default (supplied) handlers, we must
         * exit the interpreter because the handler *might* block -- we don't
         * know what it will do, so we can't hold the lock on the intepreter.
         */
        AcpiExExitInterpreter();
    }

    /* Call the handler */

    Status = Handler (Function,
        (RegionObj->Region.Address + RegionOffset), BitWidth, Value,
        Context, RegionObj2->Extra.RegionContext);

    if (ACPI_FAILURE (Status))
    {
        ACPI_EXCEPTION ((AE_INFO, Status, "Returned by Handler for [%s]",
            AcpiUtGetRegionName (RegionObj->Region.SpaceId)));
    }

    if (!(HandlerDesc->AddressSpace.HandlerFlags &
            ACPI_ADDR_HANDLER_DEFAULT_INSTALLED))
    {
        /*
         * We just returned from a non-default handler, we must re-enter the
         * interpreter
         */
       AcpiExEnterInterpreter ();
    }

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvDetachRegion
 *
 * PARAMETERS:  RegionObj           - Region Object
 *              AcpiNsIsLocked      - Namespace Region Already Locked?
 *
 * RETURN:      None
 *
 * DESCRIPTION: Break the association between the handler and the region
 *              this is a two way association.
 *
 ******************************************************************************/

void
AcpiEvDetachRegion(
    ACPI_OPERAND_OBJECT     *RegionObj,
    BOOLEAN                 AcpiNsIsLocked)
{
    ACPI_OPERAND_OBJECT     *HandlerObj;
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_OPERAND_OBJECT     **LastObjPtr;
    ACPI_ADR_SPACE_SETUP    RegionSetup;
    void                    **RegionContext;
    ACPI_OPERAND_OBJECT     *RegionObj2;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE (EvDetachRegion);


    RegionObj2 = AcpiNsGetSecondaryObject (RegionObj);
    if (!RegionObj2)
    {
        return_VOID;
    }
    RegionContext = &RegionObj2->Extra.RegionContext;

    /* Get the address handler from the region object */

    HandlerObj = RegionObj->Region.Handler;
    if (!HandlerObj)
    {
        /* This region has no handler, all done */

        return_VOID;
    }

    /* Find this region in the handler's list */

    ObjDesc = HandlerObj->AddressSpace.RegionList;
    LastObjPtr = &HandlerObj->AddressSpace.RegionList;

    while (ObjDesc)
    {
        /* Is this the correct Region? */

        if (ObjDesc == RegionObj)
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_OPREGION,
                "Removing Region %p from address handler %p\n",
                RegionObj, HandlerObj));

            /* This is it, remove it from the handler's list */

            *LastObjPtr = ObjDesc->Region.Next;
            ObjDesc->Region.Next = NULL;        /* Must clear field */

            if (AcpiNsIsLocked)
            {
                Status = AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);
                if (ACPI_FAILURE (Status))
                {
                    return_VOID;
                }
            }

            /* Now stop region accesses by executing the _REG method */

            Status = AcpiEvExecuteRegMethod (RegionObj, ACPI_REG_DISCONNECT);
            if (ACPI_FAILURE (Status))
            {
                ACPI_EXCEPTION ((AE_INFO, Status, "from region _REG, [%s]",
                    AcpiUtGetRegionName (RegionObj->Region.SpaceId)));
            }

            if (AcpiNsIsLocked)
            {
                Status = AcpiUtAcquireMutex (ACPI_MTX_NAMESPACE);
                if (ACPI_FAILURE (Status))
                {
                    return_VOID;
                }
            }

            /*
             * If the region has been activated, call the setup handler with
             * the deactivate notification
             */
            if (RegionObj->Region.Flags & AOPOBJ_SETUP_COMPLETE)
            {
                RegionSetup = HandlerObj->AddressSpace.Setup;
                Status = RegionSetup (RegionObj, ACPI_REGION_DEACTIVATE,
                    HandlerObj->AddressSpace.Context, RegionContext);

                /* Init routine may fail, Just ignore errors */

                if (ACPI_FAILURE (Status))
                {
                    ACPI_EXCEPTION ((AE_INFO, Status,
                        "from region handler - deactivate, [%s]",
                        AcpiUtGetRegionName (RegionObj->Region.SpaceId)));
                }

                RegionObj->Region.Flags &= ~(AOPOBJ_SETUP_COMPLETE);
            }

            /*
             * Remove handler reference in the region
             *
             * NOTE: this doesn't mean that the region goes away, the region
             * is just inaccessible as indicated to the _REG method
             *
             * If the region is on the handler's list, this must be the
             * region's handler
             */
            RegionObj->Region.Handler = NULL;
            AcpiUtRemoveReference (HandlerObj);

            return_VOID;
        }

        /* Walk the linked list of handlers */

        LastObjPtr = &ObjDesc->Region.Next;
        ObjDesc = ObjDesc->Region.Next;
    }

    /* If we get here, the region was not in the handler's region list */

    ACPI_DEBUG_PRINT ((ACPI_DB_OPREGION,
        "Cannot remove region %p from address handler %p\n",
        RegionObj, HandlerObj));

    return_VOID;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvAttachRegion
 *
 * PARAMETERS:  HandlerObj          - Handler Object
 *              RegionObj           - Region Object
 *              AcpiNsIsLocked      - Namespace Region Already Locked?
 *
 * RETURN:      None
 *
 * DESCRIPTION: Create the association between the handler and the region
 *              this is a two way association.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiEvAttachRegion (
    ACPI_OPERAND_OBJECT     *HandlerObj,
    ACPI_OPERAND_OBJECT     *RegionObj,
    BOOLEAN                 AcpiNsIsLocked)
{

    ACPI_FUNCTION_TRACE (EvAttachRegion);


    ACPI_DEBUG_PRINT ((ACPI_DB_OPREGION,
        "Adding Region [%4.4s] %p to address handler %p [%s]\n",
        AcpiUtGetNodeName (RegionObj->Region.Node),
        RegionObj, HandlerObj,
        AcpiUtGetRegionName (RegionObj->Region.SpaceId)));

    /* Link this region to the front of the handler's list */

    RegionObj->Region.Next = HandlerObj->AddressSpace.RegionList;
    HandlerObj->AddressSpace.RegionList = RegionObj;

    /* Install the region's handler */

    if (RegionObj->Region.Handler)
    {
        return_ACPI_STATUS (AE_ALREADY_EXISTS);
    }

    RegionObj->Region.Handler = HandlerObj;
    AcpiUtAddReference (HandlerObj);

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvExecuteRegMethod
 *
 * PARAMETERS:  RegionObj           - Region object
 *              Function            - Passed to _REG: On (1) or Off (0)
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Execute _REG method for a region
 *
 ******************************************************************************/

ACPI_STATUS
AcpiEvExecuteRegMethod (
    ACPI_OPERAND_OBJECT     *RegionObj,
    UINT32                  Function)
{
    ACPI_EVALUATE_INFO      *Info;
    ACPI_OPERAND_OBJECT     *Args[3];
    ACPI_OPERAND_OBJECT     *RegionObj2;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE (EvExecuteRegMethod);


    RegionObj2 = AcpiNsGetSecondaryObject (RegionObj);
    if (!RegionObj2)
    {
        return_ACPI_STATUS (AE_NOT_EXIST);
    }

    if (RegionObj2->Extra.Method_REG == NULL)
    {
        return_ACPI_STATUS (AE_OK);
    }

    /* Allocate and initialize the evaluation information block */

    Info = ACPI_ALLOCATE_ZEROED (sizeof (ACPI_EVALUATE_INFO));
    if (!Info)
    {
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    Info->PrefixNode = RegionObj2->Extra.Method_REG;
    Info->RelativePathname = NULL;
    Info->Parameters = Args;
    Info->Flags = ACPI_IGNORE_RETURN_VALUE;

    /*
     * The _REG method has two arguments:
     *
     * Arg0 - Integer:
     *  Operation region space ID Same value as RegionObj->Region.SpaceId
     *
     * Arg1 - Integer:
     *  connection status 1 for connecting the handler, 0 for disconnecting
     *  the handler (Passed as a parameter)
     */
    Args[0] = AcpiUtCreateIntegerObject ((UINT64) RegionObj->Region.SpaceId);
    if (!Args[0])
    {
        Status = AE_NO_MEMORY;
        goto Cleanup1;
    }

    Args[1] = AcpiUtCreateIntegerObject ((UINT64) Function);
    if (!Args[1])
    {
        Status = AE_NO_MEMORY;
        goto Cleanup2;
    }

    Args[2] = NULL; /* Terminate list */

    /* Execute the method, no return value */

    ACPI_DEBUG_EXEC (
        AcpiUtDisplayInitPathname (ACPI_TYPE_METHOD, Info->PrefixNode, NULL));

    Status = AcpiNsEvaluate (Info);
    AcpiUtRemoveReference (Args[1]);

Cleanup2:
    AcpiUtRemoveReference (Args[0]);

Cleanup1:
    ACPI_FREE (Info);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvExecuteRegMethods
 *
 * PARAMETERS:  Node            - Namespace node for the device
 *              SpaceId         - The address space ID
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Run all _REG methods for the input Space ID;
 *              Note: assumes namespace is locked, or system init time.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiEvExecuteRegMethods (
    ACPI_NAMESPACE_NODE     *Node,
    ACPI_ADR_SPACE_TYPE     SpaceId)
{
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE (EvExecuteRegMethods);


    /*
     * Run all _REG methods for all Operation Regions for this space ID. This
     * is a separate walk in order to handle any interdependencies between
     * regions and _REG methods. (i.e. handlers must be installed for all
     * regions of this Space ID before we can run any _REG methods)
     */
    Status = AcpiNsWalkNamespace (ACPI_TYPE_ANY, Node, ACPI_UINT32_MAX,
                ACPI_NS_WALK_UNLOCK, AcpiEvRegRun, NULL,
                &SpaceId, NULL);

    /* Special case for EC: handle "orphan" _REG methods with no region */

    if (SpaceId == ACPI_ADR_SPACE_EC)
    {
        AcpiEvOrphanEcRegMethod (Node);
    }

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvRegRun
 *
 * PARAMETERS:  WalkNamespace callback
 *
 * DESCRIPTION: Run _REG method for region objects of the requested spaceID
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiEvRegRun (
    ACPI_HANDLE             ObjHandle,
    UINT32                  Level,
    void                    *Context,
    void                    **ReturnValue)
{
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_ADR_SPACE_TYPE     SpaceId;
    ACPI_STATUS             Status;


    SpaceId = *ACPI_CAST_PTR (ACPI_ADR_SPACE_TYPE, Context);

    /* Convert and validate the device handle */

    Node = AcpiNsValidateHandle (ObjHandle);
    if (!Node)
    {
        return (AE_BAD_PARAMETER);
    }

    /*
     * We only care about regions.and objects that are allowed to have address
     * space handlers
     */
    if ((Node->Type != ACPI_TYPE_REGION) &&
        (Node != AcpiGbl_RootNode))
    {
        return (AE_OK);
    }

    /* Check for an existing internal object */

    ObjDesc = AcpiNsGetAttachedObject (Node);
    if (!ObjDesc)
    {
        /* No object, just exit */

        return (AE_OK);
    }

    /* Object is a Region */

    if (ObjDesc->Region.SpaceId != SpaceId)
    {
        /* This region is for a different address space, just ignore it */

        return (AE_OK);
    }

    Status = AcpiEvExecuteRegMethod (ObjDesc, ACPI_REG_CONNECT);
    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvOrphanEcRegMethod
 *
 * PARAMETERS:  EcDeviceNode        - Namespace node for an EC device
 *
 * RETURN:      None
 *
 * DESCRIPTION: Execute an "orphan" _REG method that appears under the EC
 *              device. This is a _REG method that has no corresponding region
 *              within the EC device scope. The orphan _REG method appears to
 *              have been enabled by the description of the ECDT in the ACPI
 *              specification: "The availability of the region space can be
 *              detected by providing a _REG method object underneath the
 *              Embedded Controller device."
 *
 *              To quickly access the EC device, we use the EcDeviceNode used
 *              during EC handler installation. Otherwise, we would need to
 *              perform a time consuming namespace walk, executing _HID
 *              methods to find the EC device.
 *
 *  MUTEX:      Assumes the namespace is locked
 *
 ******************************************************************************/

static void
AcpiEvOrphanEcRegMethod (
    ACPI_NAMESPACE_NODE     *EcDeviceNode)
{
    ACPI_HANDLE             RegMethod;
    ACPI_NAMESPACE_NODE     *NextNode;
    ACPI_STATUS             Status;
    ACPI_OBJECT_LIST        Args;
    ACPI_OBJECT             Objects[2];


    ACPI_FUNCTION_TRACE (EvOrphanEcRegMethod);


    if (!EcDeviceNode)
    {
        return_VOID;
    }

    /* Namespace is currently locked, must release */

    (void) AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);

    /* Get a handle to a _REG method immediately under the EC device */

    Status = AcpiGetHandle (EcDeviceNode, METHOD_NAME__REG, &RegMethod);
    if (ACPI_FAILURE (Status))
    {
        goto Exit; /* There is no _REG method present */
    }

    /*
     * Execute the _REG method only if there is no Operation Region in
     * this scope with the Embedded Controller space ID. Otherwise, it
     * will already have been executed. Note, this allows for Regions
     * with other space IDs to be present; but the code below will then
     * execute the _REG method with the EmbeddedControl SpaceID argument.
     */
    NextNode = AcpiNsGetNextNode (EcDeviceNode, NULL);
    while (NextNode)
    {
        if ((NextNode->Type == ACPI_TYPE_REGION) &&
            (NextNode->Object) &&
            (NextNode->Object->Region.SpaceId == ACPI_ADR_SPACE_EC))
        {
            goto Exit; /* Do not execute the _REG */
        }

        NextNode = AcpiNsGetNextNode (EcDeviceNode, NextNode);
    }

    /* Evaluate the _REG(EmbeddedControl,Connect) method */

    Args.Count = 2;
    Args.Pointer = Objects;
    Objects[0].Type = ACPI_TYPE_INTEGER;
    Objects[0].Integer.Value = ACPI_ADR_SPACE_EC;
    Objects[1].Type = ACPI_TYPE_INTEGER;
    Objects[1].Integer.Value = ACPI_REG_CONNECT;

    Status = AcpiEvaluateObject (RegMethod, NULL, &Args, NULL);

Exit:
    /* We ignore all errors from above, don't care */

    Status = AcpiUtAcquireMutex (ACPI_MTX_NAMESPACE);
    return_VOID;
}
