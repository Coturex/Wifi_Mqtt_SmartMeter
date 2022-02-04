/*
 * Copyright (C) 2018-2019 Pierre Hébert
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef PZEM004Tv10_H
#define PZEM004Tv10_H

#include <Arduino.h>
#include <SoftwareSerial.h>

// all messages have the same length
#define PZEM_MSG_LENGTH 7

// a clean and simple interface for the PZEM-004Tv10 module, using a fixed address
class PZEM004Tv10 {
    public:
        PZEM004Tv10();
        PZEM004Tv10(int receivePin, int transmitPin);

        bool init();
        float voltage();
        float current();
        uint16_t power();
        uint32_t energy();

        const char *getRawData();

    private:
        Stream *port;
        char read_buffer[PZEM_MSG_LENGTH];

        void flush();
        bool read_reply();

        static const char RequestSetAddress[7];
        static const char RequestReadVoltage[7];
        static const char RequestReadCurrent[7];
        static const char RequestReadPower[7];
        static const char RequestReadEnergy[7];
};

#endif