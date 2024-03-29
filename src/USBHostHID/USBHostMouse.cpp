/* mbed USBHost Library
 * Copyright (c) 2006-2013 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "USBHostMouse.h"

#if USBHOST_MOUSE

USBHostMouse::USBHostMouse()
{
    init();
}

void USBHostMouse::init()
{
    dev = NULL;
    int_in = NULL;
    onUpdate = NULL;
    onButtonUpdate = NULL;
    onXUpdate = NULL;
    onYUpdate = NULL;
    onZUpdate = NULL;
    report_id = 0;
    dev_connected = false;
    mouse_device_found = false;
    mouse_intf = -1;

    buttons = 0;
    x = 0;
    y = 0;
    z = 0;
}

bool USBHostMouse::connected()
{
    return dev_connected;
}

bool USBHostMouse::connect()
{
    int len_listen;

    if (dev_connected) {
        return true;
    }
    host = USBHost::getHostInst();

    for (uint8_t i = 0; i < MAX_DEVICE_CONNECTED; i++) {
        if ((dev = host->getDevice(i)) != NULL) {

            if(host->enumerate(dev, this)) {
                break;
            }
            if (mouse_device_found) {
                {
                    /* As this is done in a specific thread
                     * this lock is taken to avoid to process the device
                     * disconnect in usb process during the device registering */
                    USBHost::Lock  Lock(host);
                    int_in = dev->getEndpoint(mouse_intf, INTERRUPT_ENDPOINT, IN);
                    if (!int_in) {
                        break;
                    }

                    USB_INFO("New Mouse device: VID:%04x PID:%04x [dev: %p - intf: %d]", dev->getVid(), dev->getPid(), dev, mouse_intf);
                    dev->setName("Mouse", mouse_intf);
                    host->registerDriver(dev, mouse_intf, this, &USBHostMouse::init);

                    int_in->attach(this, &USBHostMouse::rxHandler);
                    len_listen = int_in->getSize();
                    if (len_listen > sizeof(report)) {
                        len_listen = sizeof(report);
                    }
                }
                int ret=host->interruptRead(dev, int_in, report, len_listen, false);
                MBED_ASSERT((ret==USB_TYPE_OK) || (ret ==USB_TYPE_PROCESSING) || (ret == USB_TYPE_FREE));
                if ((ret==USB_TYPE_OK) || (ret ==USB_TYPE_PROCESSING)) {
                    dev_connected = true;
                }
                if (ret == USB_TYPE_FREE) {
                    dev_connected = false;
                }
                return true;
            }
        }
    }
    init();
    return false;
}

void USBHostMouse::rxHandler()
{
    int len_listen = int_in->getLengthTransferred();
    if (len_listen !=0) {

        if (onUpdate) {
            (*onUpdate)(report[0] & 0x07, report[1], report[2], report[3]);
        }

        if (onButtonUpdate && (buttons != (report[0] & 0x07))) {
            (*onButtonUpdate)(report[0] & 0x07);
        }

        if (onXUpdate && (x != report[1])) {
            (*onXUpdate)(report[1]);
        }

        if (onYUpdate && (y != report[2])) {
            (*onYUpdate)(report[2]);
        }

        if (onZUpdate && (z != report[3])) {
            (*onZUpdate)(report[3]);
        }

        // update mouse state
        buttons = report[0] & 0x07;
        x = report[1];
        y = report[2];
        z = report[3];
    }
    /*  set again the maximum value */
    len_listen = int_in->getSize();

    if (len_listen > sizeof(report)) {
        len_listen = sizeof(report);
    }

    if (dev) {
        host->interruptRead(dev, int_in, report, len_listen, false);
    }
}

/*virtual*/ void USBHostMouse::setVidPid(uint16_t vid, uint16_t pid)
{
    // we don't check VID/PID for mouse driver
}

/*virtual*/ bool USBHostMouse::parseInterface(uint8_t intf_nb, uint8_t intf_class, uint8_t intf_subclass, uint8_t intf_protocol) //Must return true if the interface should be parsed
{
    if ((mouse_intf == -1) &&
            (intf_class == HID_CLASS) &&
            (intf_subclass == 0x01) &&
            (intf_protocol == 0x02)) {
        mouse_intf = intf_nb;
        return true;
    }
    return false;
}

/*virtual*/ bool USBHostMouse::useEndpoint(uint8_t intf_nb, ENDPOINT_TYPE type, ENDPOINT_DIRECTION dir) //Must return true if the endpoint will be used
{
    if (intf_nb == mouse_intf) {
        if (type == INTERRUPT_ENDPOINT && dir == IN) {
            mouse_device_found = true;
            return true;
        }
    }
    return false;
}

#endif
