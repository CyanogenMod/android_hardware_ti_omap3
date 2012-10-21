/*
 * OMX Core Plugin
 *
 * This component allows the PV OMX Mastercore to load the TI OMX Core
 *
 * Copyright (C) 2010 Texas Instruments Incorporated - http://www.ti.com/ 
 * 
 * 
 *  Redistribution and use in source and binary forms, with or without 
 *  modification, are permitted provided that the following conditions 
 *  are met:
 *
 *  o Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer.
 *
 *  o Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the   
 *    distribution.
 *
 *  o Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
*/
#define LOG_TAG "omx_interface"
#include <utils/Log.h>

#include "pvlogger.h"

#include "pv_omxcore.h"
#include "omx_interface.h"

// this is the name of the original SO that contains your OMX core
#define OMX_CORE_LIBRARY "libOMX_Core.so"

class TIOMXInterface : public OMXInterface
{
    public:
        // Handle to the OMX core library
        void* ipHandle;

        ~TIOMXInterface()
        {
            if ((NULL != ipHandle) && (0 != dlclose(ipHandle)))
            {
                // dlclose() returns non-zero value if close failed, check for errors
                const char* pErr = dlerror();
                if (NULL != pErr)
                {
                    ALOGE("TIOMXInterface: Error closing library: %s\n", pErr);
                }
                else
                {
                    ALOGE("OsclSharedLibrary::Close: Error closing library, no error reported");
                }
            }

            ipHandle = NULL;
        };

        OsclAny* SharedLibraryLookup(const OsclUuid& aInterfaceId)
        {
            // Make sure ipHandle is valid. If ipHandle is NULL, the dlopen
            // call failed.
            if (ipHandle && aInterfaceId == OMX_INTERFACE_ID)
            {
                ALOGD("TIOMXInterface: library lookup success\n");
                // the library lookup was successful
                return this;
            }
            // the ID doesn't match
            ALOGE("TIOMXInterface: library lookup failed\n");
            return NULL;
        };

        static TIOMXInterface* Instance()
        {
            ALOGD("TIOMXInterface: creating interface\n");
            return OSCL_NEW(TIOMXInterface, ());
        };

        bool UnloadWhenNotUsed(void)
        {
            // As of 9/22/08, the PV OMX core library can not be
            // safely unloaded and reloaded when the proxy interface
            // is enabled.
            return false;
        };

    private:

        TIOMXInterface()
        {
            ALOGD("Calling DLOPEN on OMX_CORE_LIBRARY (%s)\n", OMX_CORE_LIBRARY);
            ipHandle = dlopen(OMX_CORE_LIBRARY, RTLD_NOW);

            if (NULL == ipHandle)
            {
                pOMX_Init = NULL;
                pOMX_Deinit = NULL;
                pOMX_ComponentNameEnum = NULL;
                pOMX_GetHandle = NULL;
                pOMX_FreeHandle = NULL;
                pOMX_GetComponentsOfRole = NULL;
                pOMX_GetRolesOfComponent = NULL;
                pOMX_SetupTunnel = NULL;
                pOMX_GetContentPipe = NULL;
                // added extra method to enable config parsing without instantiating the component
                pOMXConfigParser = NULL;

                // check for errors
                const char* pErr = dlerror();
                if (NULL == pErr)
                {
                    // No error reported, but no handle to the library
                    ALOGE("OsclLib::LoadLibrary: Error opening "
                         "library (%s) but no error reported\n", OMX_CORE_LIBRARY);
                }
                else
                {
                    // Error reported
                    ALOGE("OsclLib::LoadLibrary: Error opening "
                         "library (%s): %s\n", OMX_CORE_LIBRARY, pErr);
                }
            }
            else
            {
                ALOGD("DLOPEN SUCCEEDED (%s)\n", OMX_CORE_LIBRARY);
                // Lookup all the symbols in the OMX core
                pOMX_Init = (tpOMX_Init)dlsym(ipHandle, "TIOMX_Init");
                pOMX_Deinit = (tpOMX_Deinit)dlsym(ipHandle, "TIOMX_Deinit");
                pOMX_ComponentNameEnum = (tpOMX_ComponentNameEnum)dlsym(ipHandle, "TIOMX_ComponentNameEnum");
                pOMX_GetHandle = (tpOMX_GetHandle)dlsym(ipHandle, "TIOMX_GetHandle");
                pOMX_FreeHandle = (tpOMX_FreeHandle)dlsym(ipHandle, "TIOMX_FreeHandle");
                pOMX_GetComponentsOfRole = (tpOMX_GetComponentsOfRole)dlsym(ipHandle, "TIOMX_GetComponentsOfRole");
                pOMX_GetRolesOfComponent = (tpOMX_GetRolesOfComponent)dlsym(ipHandle, "TIOMX_GetRolesOfComponent");
                pOMX_SetupTunnel = (tpOMX_SetupTunnel)dlsym(ipHandle, "TIOMX_SetupTunnel");
                pOMXConfigParser = (tpOMXConfigParser)dlsym(ipHandle, "TIOMXConfigParserRedirect");
#ifdef TARGET_OMAP4
                pOMX_GetContentPipe = (tpOMX_GetContentPipe)dlsym(ipHandle, "TIOMX_GetContentPipe");		
#else
                pOMX_GetContentPipe = NULL; // (tpOMX_GetContentPipe)dlsym(ipHandle, "OMX_GetContentPipe");
#endif
            }
        };

};

// function to obtain the interface object from the shared library
extern "C"
{
    OSCL_EXPORT_REF OsclAny* PVGetInterface()
    {
        return TIOMXInterface::Instance();
    }

    OSCL_EXPORT_REF void PVReleaseInterface(OsclSharedLibraryInterface* aInstance)
    {
       TIOMXInterface* instance = (TIOMXInterface*)aInstance;
       if (instance){
          OSCL_DELETE(instance);
       }
    }
}


