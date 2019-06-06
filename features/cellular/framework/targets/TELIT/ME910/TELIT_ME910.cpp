/*
 * Copyright (c) 2017, Arm Limited and affiliates.
 * SPDX-License-Identifier: Apache-2.0
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

#include "TELIT_ME910.h"
#include "TELIT_ME910_CellularContext.h"
#include "AT_CellularNetwork.h"
#include "PinNames.h"
#include "rtos/ThisThread.h"

using namespace mbed;
using namespace rtos;
using namespace events;

#if !defined(MBED_CONF_TELIT_ME910_PWR)
#define MBED_CONF_TELIT_ME910_PWR    NC
#endif

#if !defined(MBED_CONF_TELIT_ME910_TX)
#define MBED_CONF_TELIT_ME910_TX    NC
#endif

#if !defined(MBED_CONF_TELIT_ME910_RX)
#define MBED_CONF_TELIT_ME910_RX    NC
#endif

#if !defined(MBED_CONF_TELIT_ME910_POLARITY)
#define MBED_CONF_TELIT_ME910_POLARITY    1 // active high
#endif

static const intptr_t cellular_properties[AT_CellularBase::PROPERTY_MAX] = {
    AT_CellularNetwork::RegistrationModeLAC,    // C_EREG
    AT_CellularNetwork::RegistrationModeLAC,    // C_GREG
    AT_CellularNetwork::RegistrationModeLAC,    // C_REG
    0,  // AT_CGSN_WITH_TYPE
    0,  // AT_CGDATA
    1,  // AT_CGAUTH
    1,  // AT_CNMI
    1,  // AT_CSMP
    1,  // AT_CMGF
    1,  // AT_CSDH
    1,  // PROPERTY_IPV4_STACK
    1,  // PROPERTY_IPV6_STACK
    1,  // PROPERTY_IPV4V6_STACK
    0,  // PROPERTY_NON_IP_PDP_TYPE
    1,  // PROPERTY_AT_CGEREP
};

TELIT_ME910::TELIT_ME910(FileHandle *fh, PinName pwr, bool active_high)
    : AT_CellularDevice(fh),
      _active_high(active_high),
      _pwr_key(pwr, !_active_high)
{
    AT_CellularBase::set_cellular_properties(cellular_properties);
}

AT_CellularContext *TELIT_ME910::create_context_impl(ATHandler &at, const char *apn, bool cp_req, bool nonip_req)
{
    return new TELIT_ME910_CellularContext(at, this, apn, cp_req, nonip_req);
}


uint16_t TELIT_ME910::get_send_delay() const
{
    return DEFAULT_DELAY_BETWEEN_AT_COMMANDS;
}

nsapi_error_t TELIT_ME910::init()
{
    nsapi_error_t err = AT_CellularDevice::init();
    if (err != NSAPI_ERROR_OK) {
        return err;
    }
    _at->lock();
#if defined (MBED_CONF_TELIT_ME910_RTS) && defined (MBED_CONF_TELIT_ME910_CTS)
    _at->cmd_start("AT&K3;&C1;&D0");
#else
    _at->cmd_start("AT&K0;&C1;&D0");
#endif
    _at->cmd_stop_read_resp();

    // AT#QSS=1
    // Enable the Query SIM Status unsolicited indication in the ME. The format of the
    // unsolicited indication is the following:
    // #QSS: <status>
    // The ME informs at
    // every SIM status change through the basic unsolicited indication where <status> range is 0...1
    // <status> values:
    // - 0: SIM not inserted
    // - 1: SIM inserted
    _at->cmd_start("AT#QSS=1");
    _at->cmd_stop_read_resp();

    // AT#PSNT=1
    // Set command enables unsolicited result code for packet service network type (PSNT)
    // having the following format:
    // #PSNT:<nt>
    // <nt> values:
    // - 0: GPRS network
    // - 4: LTE network
    // - 5: unknown or not registered
    _at->cmd_start("AT#PSNT=1");
    _at->cmd_stop_read_resp();

    // AT+CGEREP=2
    // Set command enables sending of unsolicited result codes in case of certain events
    // occurring in the module or in the network.
    // Current setting: buffer unsolicited result codes in the TA when TA-TE link is reserved (e.g.
    // in on-line data mode) and flush them to the TE when TA-TE link becomes
    // available; otherwise forward them directly to the TE.
    _at->cmd_start("AT+CGEREP=2");
    _at->cmd_stop_read_resp();

    // AT+CMER=2
    // Set command enables sending of unsolicited result codes from TA to TE in the case of
    // indicator state changes.
    // Current setting: buffer +CIEV Unsolicited Result Codes in the TA when TA-TE link is
    // reserved (e.g. on-line data mode) and flush them to the TE after
    // reservation; otherwise forward them directly to the TE
    _at->cmd_start("AT+CMER=2");
    _at->cmd_stop_read_resp();

    // AT+CREG=1
    // Set command enables the network registration unsolicited result code and selects one of
    // the two available formats:
    // short format: +CREG: <stat>
    // long format: +CREG: <stat>[,<lac>,<ci>[,<AcT>]]
    // Current setting: enable the network registration unsolicited result code, and selects the
    // short format
    _at->cmd_start("AT+CREG=1");
    _at->cmd_stop_read_resp();

    // AT+CGREG=1
    // Set command enables/disables the +CGREG: unsolicited result code, and selects one of the
    // available formats:
    // short format:
    // +CGREG:<stat>
    // long format:
    // +CGREG:<stat>[,<lac>,<ci>[,<AcT>,<rac>]]
    // extra long format:
    // +CGREG:<stat>[,[<lac>],[<ci>],[<AcT>],[<rac>][,,[,[<ActiveTime>],[<PeriodicRAU>],[<GPRSREADYtimer>]]]]
    // Current setting: enable the network registration unsolicited result code, and selects the
    // short format
    _at->cmd_start("AT+CGREG=1");
    _at->cmd_stop_read_resp();

    // AT+CEREG=1
    // Set command enables the EPS network registration unsolicited result code (URC) in LTE,
    // and selects one of the available formats:
    // short format: +CEREG: <stat>
    // long format: +CEREG: <stat>[,[<tac>],[<ci>],[<AcT>]]
    // <tac>, <ci>, and <AcT> are reported by the command only if available.
    // Current setting: enable the network registration unsolicited result code, and select the short
    // format
    _at->cmd_start("AT+CEREG=1");
    _at->cmd_stop_read_resp();

    // AT+CMEE=2
    // Set command disables the use of result code +CME ERROR: <err> as an indication of an
    // error relating to the +Cxxx command issued. When enabled, device related errors cause the +CME
    // ERROR: <err> final result code instead of the default ERROR final result code. ERROR is returned
    // normally when the error message is related to syntax, invalid parameters or DTE functionality.
    // Current setting: enable and use verbose <err> values
    _at->cmd_start("AT+CMEE=2");
    _at->cmd_stop_read_resp();

    // AT&W&P
    // - AT&W: Execution command stores on profile <n> the complete configuration of the device. If
    //         parameter is omitted, the command has the same behavior of AT&W0.
    // - AT&P: Execution command defines which full profile will be loaded at startup. If parameter
    //         is omitted, the command has the same behavior as AT&P0
    _at->cmd_start("AT&W&P");
    _at->cmd_stop_read_resp();

    return _at->unlock_return_error();
}

#if MBED_CONF_TELIT_ME910_PROVIDE_DEFAULT
#include "UARTSerial.h"
CellularDevice *CellularDevice::get_default_instance()
{
    static UARTSerial serial(MBED_CONF_TELIT_ME910_TX, MBED_CONF_TELIT_ME910_RX, MBED_CONF_TELIT_ME910_BAUDRATE);
#if defined (MBED_CONF_TELIT_ME910_RTS) && defined (MBED_CONF_TELIT_ME910_CTS)
    serial.set_flow_control(SerialBase::RTSCTS, MBED_CONF_TELIT_ME910_RTS, MBED_CONF_TELIT_ME910_CTS);
#endif
    static TELIT_ME910 device(&serial,
                              MBED_CONF_TELIT_ME910_PWR,
                              MBED_CONF_TELIT_ME910_POLARITY);
    return &device;
}
#endif

nsapi_error_t TELIT_ME910::hard_power_on()
{
    soft_power_on();

    return NSAPI_ERROR_OK;
}

nsapi_error_t TELIT_ME910::soft_power_on()
{
    _pwr_key = _active_high;
    ThisThread::sleep_for(500);
    _pwr_key = !_active_high;
    ThisThread::sleep_for(5000);
    _pwr_key = _active_high;
    ThisThread::sleep_for(5000);

    return NSAPI_ERROR_OK;
}

nsapi_error_t TELIT_ME910::hard_power_off()
{
    _pwr_key = !_active_high;
    ThisThread::sleep_for(10000);

    return NSAPI_ERROR_OK;
}

nsapi_error_t TELIT_ME910::soft_power_off()
{
    return AT_CellularDevice::soft_power_off();
}
