#define LOG_TAG "CameraHal"


#include "CameraHal.h"

namespace android {

/*--------------------FrameProvider Class STARTS here-----------------------------*/

int FrameProvider::enableFrameNotification(int32_t frameTypes)
{
    LOG_FUNCTION_NAME
    status_t ret = NO_ERROR;

    ///Enable the frame notification to CameraAdapter (which implements FrameNotifier interface)
    mFrameNotifier->enableMsgType(frameTypes<<MessageNotifier::FRAME_BIT_FIELD_POSITION
                                    , mFrameCallback
                                    , NULL
                                    , mCookie
                                    );

    LOG_FUNCTION_NAME_EXIT;
    return ret;
}

int FrameProvider::disableFrameNotification(int32_t frameTypes)
{
    LOG_FUNCTION_NAME
    status_t ret = NO_ERROR;

    mFrameNotifier->disableMsgType(frameTypes<<MessageNotifier::FRAME_BIT_FIELD_POSITION
                                    , mCookie
                                    );

    LOG_FUNCTION_NAME_EXIT;
    return ret;

}

int FrameProvider::returnFrame(void *frameBuf)
{
    LOG_FUNCTION_NAME
    status_t ret = NO_ERROR;

    mFrameNotifier->returnFrame(frameBuf);

    LOG_FUNCTION_NAME_EXIT
    return ret;
}


/*--------------------FrameProvider Class ENDS here-----------------------------*/

/*--------------------EventProvider Class STARTS here-----------------------------*/

int EventProvider::enableEventNotification(int32_t frameTypes)
{
    LOG_FUNCTION_NAME
    status_t ret = NO_ERROR;

    ///Enable the frame notification to CameraAdapter (which implements FrameNotifier interface)
    mEventNotifier->enableMsgType(frameTypes<<MessageNotifier::EVENT_BIT_FIELD_POSITION
                                    , NULL
                                    , mEventCallback
                                    , mCookie
                                    );

    LOG_FUNCTION_NAME_EXIT
    return ret;
}

int EventProvider::disableEventNotification(int32_t frameTypes)
{
    LOG_FUNCTION_NAME
    status_t ret = NO_ERROR;

    mEventNotifier->disableMsgType(frameTypes<<MessageNotifier::FRAME_BIT_FIELD_POSITION
                                    , mCookie
                                    );

    LOG_FUNCTION_NAME_EXIT
    return ret;

}

/*--------------------EventProvider Class ENDS here-----------------------------*/

};
