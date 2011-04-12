/* RingBuffer.h
 **
 ** Copyright 2009-2011, Texas Instruments
 **
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 **
 **     http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 */

#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <utils/threads.h>

namespace android
{

class RingBuffer
{
public:
    RingBuffer(size_t size);
    ~RingBuffer();

    void reset();
    size_t write(const void *buffer, size_t size);
    size_t read(void *buffer, size_t size);
    size_t copy(void *buffer, size_t size);

private:
    char *      mBuffer;
    size_t      mSize;
    int         mEmpty;
    size_t      mBegin;
    size_t      mEnd;
    Mutex       mLock;
    Condition   mCond;
};

}

#endif
