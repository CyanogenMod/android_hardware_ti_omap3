/* RingBuffer.cpp
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

#define LOG_TAG "RingBuffer"
#include <utils/Log.h>

#include <RingBuffer.h>

namespace android
{

RingBuffer::RingBuffer(size_t size) :
        mSize(size),
        mEmpty(1),
        mBegin(0),
        mEnd(0)
{
    mBuffer = new char[mSize];
    memset(mBuffer, 0, mSize);
}

RingBuffer::~RingBuffer()
{
    delete mBuffer;
}

void RingBuffer::reset()
{
    mEmpty = 1;
    mBegin = 0;
    mEnd = 0;
    memset(mBuffer, 0, mSize);
}

size_t RingBuffer::write(const void *buffer, size_t size)
{
    if (size == 0) return 0;

    char *cbuf = (char *)buffer;
    if (size > mSize) {
        LOGE("Single write is larger than ring size (%d > %d)."
                " Only writing last portion.", size, mSize);
        cbuf += (size - mSize);
        size = mSize;
    }

    mLock.lock();

    size_t avail = (mEmpty || mBegin <= mEnd) ? mSize + mBegin - mEnd : mBegin - mEnd;
    size_t len = mSize - mEnd;

    if (len > size) len = size;

    // Fill until we wrap.
    memcpy(mBuffer + mEnd, cbuf, len);
    cbuf += len;
    mEnd = (mEnd + len) % mSize;

    len = size - len;

    // Fill remaining data.
    if (len) {
        memcpy(mBuffer, cbuf, len);
        mEnd = len;
    }

    mEmpty = 0;
    if (size > avail) mBegin = mEnd;

    mCond.signal();
    mLock.unlock();

    return size;
}

size_t RingBuffer::read(void *buffer, size_t size)
{
    if (size > mSize) {
        LOGE("Single read is larger than ring size (%d > %d)."
                " Truncating read.", size, mSize);
        size = mSize;
    }

    char *cbuf = (char *)buffer;
    size_t len = size;

    mLock.lock();

    while (len) {
        if (mEmpty) mCond.wait(mLock);

        size_t s = (mBegin < mEnd) ? mEnd - mBegin : mSize - mBegin;
        if (len < s) s = len;

        memcpy(cbuf, mBuffer + mBegin, s);
        cbuf += s;
        len -= s;

        mBegin = (mBegin + s) % mSize;
        if (mBegin == mEnd) mEmpty = 1;
    }

    mLock.unlock();

    return size;
}

size_t RingBuffer::copy(void *buffer, size_t size)
{
    if (size > mSize) {
        LOGE("Single copy is larger than ring size (%d > %d)."
                " Truncating copy.", size, mSize);
        size = mSize;
    }

    char *cbuf = (char *)buffer;
    size_t len = size;

    mLock.lock();

    if (len && !mEmpty) {
        size_t s = (mBegin < mEnd) ? mEnd - mBegin : mSize - mBegin;
        if (len < s) s = len;

        memcpy(cbuf, mBuffer + mBegin, s);
        cbuf += s;
        len -= s;

        if (len) {
            s = mEnd;
            if (len < s) s = len;
            if (s) {
                memcpy(cbuf, mBuffer, s);
                len -= s;
            }
        }
    }

    mLock.unlock();

    return size - len;
}

}
