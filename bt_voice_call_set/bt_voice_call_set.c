/*
 *  Copyright (C) 2010  Texas Instruments, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/socket.h>

#define LOG_TAG "bt_voice_call_settings"
#define LOG_NDEBUG 0
#include <utils/Log.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/sco.h>

/** voiceCallSendHciCmd Function
 *  This function takes the hci commands for the BT chip configurations, creates
 *  a hci channel to send the commands through UART to configure BT chip
 *
 *  Parameters :
 *  @ commandLlength    : Number of arguments of the command
 *  @ command           : Pointer to command list
 *  Returns NO_ERROR upon success
 *        , different error messages depending upon the error.
 */
static int voiceCallSendHciCmd(int commandLength, const char **command)
{
    unsigned char messageBuffer[HCI_MAX_EVENT_SIZE];
    unsigned char *ptr = messageBuffer;
    struct hci_filter hciFilter;
    hci_event_hdr *hciEventHandler;
    uint16_t ocf, ogf;
    int deviceDescriptor, error, i, messageLength, deviceId;

    deviceId = hci_get_route(NULL);
    if (deviceId < 0) {
        LOGE("hci_get_route failed: %s(%d)",
             strerror(deviceId), deviceId);
        return deviceId;
    }
    LOGV("Bluetooth Device Id: %d", deviceId);

    ogf = (uint16_t)strtol(command[0], NULL, 16);
    ocf = (uint16_t)strtol(command[1], NULL, 16);

    for (i = 2, messageLength = 0;
            i < commandLength &&
            messageLength < (int)sizeof(messageBuffer);
            i++, messageLength++)
            *ptr++ = (uint8_t) strtol(command[i], NULL, 16);

    deviceDescriptor = hci_open_dev(deviceId);
    if (deviceDescriptor < 0) {
            LOGE("hci_open_dev failed: %s(%d)",
                 strerror(deviceDescriptor), deviceDescriptor);
            return deviceDescriptor;
    }
    LOGV("Bluetooth Device Descriptor: %d", deviceDescriptor);

    // Setup filter
    hci_filter_clear(&hciFilter);
    hci_filter_set_ptype(HCI_EVENT_PKT, &hciFilter);
    hci_filter_all_events(&hciFilter);
    error = setsockopt(deviceDescriptor, SOL_HCI, HCI_FILTER,
                        &hciFilter, sizeof(hciFilter));
    if (error < 0) {
        LOGE("HCI filter setup failed: %s(%d)", strerror(error), error);
        hci_close_dev(deviceDescriptor);
        return error;
    }

    // Send the BT chip configuration commands
    error = hci_send_cmd(deviceDescriptor, ogf, ocf,
                         messageLength, messageBuffer);
    if (error < 0) {
        LOGE("hci_send_cmd failed: %s(%d)", strerror(error), error);
        hci_close_dev(deviceDescriptor);
        return error;
    }

    // Wait for the command completion event
    messageLength = read(deviceDescriptor, messageBuffer,
                         sizeof(messageBuffer));
    if (messageLength < 0) {
        LOGE("HCI message completion failed: %s(%d)",
             strerror(messageLength), messageLength);
        hci_close_dev(deviceDescriptor);
        return error;
    }

    hci_close_dev(deviceDescriptor);

    return 0;
}

int main(int argc ,char *argv[])
{
    int error = 0;

    LOGV("Enable PCM port of Bluetooth SCO device for voice call");

    // BT PCM HCI configurations commands
    const char *hciCommand[] = {
        "0x3f", "0x106",                 /* OCF and OGF */
        "0x08", "0x02",                  /* Bit clock - 520KHz*/
        "0x01",                          /* BT chip as Slave*/
        "0x40", "0x1f", "0x00", "0x00",  /* Sampling rate - 8KHz*/
        "0x01", "0x00",                  /* 50% Duty cycle*/
        "0x00",                          /* Frame sync at rising edge*/
        "0x00",                          /* FS Active high polarity*/
        "0x00",                          /* FS direction - [Reserved]*/
        "0x10", "0x00",                  /* CH1 16 -bit OUT size*/
        "0x01", "0x00",                  /* One Clock delay */
        "0x00",                          /* Data driven at rising edge*/
        "0x10", "0x00",                  /* CH1 16 -bit IN size */
        "0x01", "0x00",                  /* CH1 DAta IN One Clock delay*/
        "0x01",                          /* Data driven at falling edge*/
        "0x00",                          /* Reserved bit*/
        "0x10", "0x00",                  /* CH2 16 -bit OUT size*/
        "0x11", "0x00",                  /* CH2 data OUT off set*/
        "0x00",                          /* Data driven at rising edge*/
        "0x10", "0x00",                  /* CH2 16 -bit IN size*/
        "0x11", "0x00",                  /* CH2 data IN off set*/
        "0x00",                          /* Data Sampled at rising edge*/
        "0x00"                           /* Reserved bit*/
    };

    /* send the BT configuration commands */
    error = voiceCallSendHciCmd(36, hciCommand);

    return error;
}