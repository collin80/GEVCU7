#pragma once

/*
A simple serial protocol handler that can send and receive files over a serial port. Kind of like X/YModem but not entirely
One big change from most such code is that this doesn't block for anything. Also, the serial interface has been given enough buffer
to easily buffer the entire 512 byte payload of this protocol. So, really, I do not want to ever block.
*/

#include <Arduino.h>
#include "SdFat.h"
#include <FastCRC.h>

#define START_TRANSFER  0xD0
#define ACK             0xAA
#define NAK             0xC6
#define ABORT           0xFA

enum FileSenderState
{
    FS_IDLE,
    WAITING_FOR_HEADER_ACK, //when sending
    WAITING_FOR_PACKET_ACK, //when sending
    RX_FILENAME,
    RX_FILESIZE,
    RX_PACKET, //when receiving
};

class SerialFileSender
{
public:
    SerialFileSender(HardwareSerial *stream);
    void processCharacter(char c);
    void sendFile(char *filename);
    void receiveFile();
    bool isActive();
    void loop();
private:
    void sendNextChunk();

    FsFile file;
    HardwareSerial *serialStream;
    FastCRC16 CRC16;
    uint32_t filePosition;
    uint32_t fileSize;
    uint8_t packet[512];
    int bytePos;
    uint16_t expectedCRC;
    char fname[130];
    uint32_t lastComm;
    FileSenderState state;
    bool active;
    bool lastchunk;
    int errCounter;
};