#include <dirent.h>

#define MAX_ROLES 20
#define MAX_TABLE_SIZE 30

typedef struct _ComponentTable {
    OMX_STRING name;
    OMX_U16 nRoles;
    OMX_STRING pRoleArray[MAX_ROLES];
}ComponentTable;


OMX_API OMX_ERRORTYPE TIOMX_GetRolesOfComponent ( 
    OMX_IN      OMX_STRING compName, 
    OMX_INOUT   OMX_U32 *pNumRoles,
    OMX_OUT     OMX_U8 **roles);


OMX_ERRORTYPE TIOMX_BuildComponentTable();
OMX_ERRORTYPE ComponentTable_EventHandler(
        OMX_IN OMX_HANDLETYPE hComponent,
        OMX_IN OMX_PTR pAppData,
        OMX_IN OMX_EVENTTYPE eEvent,
        OMX_IN OMX_U32 nData1,
        OMX_IN OMX_U32 nData2,
        OMX_IN OMX_PTR pEventData);

OMX_ERRORTYPE ComponentTable_EmptyBufferDone(
        OMX_OUT OMX_HANDLETYPE hComponent,
        OMX_OUT OMX_PTR pAppData,
        OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer);

OMX_ERRORTYPE ComponentTable_FillBufferDone(
        OMX_OUT OMX_HANDLETYPE hComponent,
        OMX_OUT OMX_PTR pAppData,
        OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer);
