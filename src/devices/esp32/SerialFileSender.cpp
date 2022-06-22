#include "SerialFileSender.h"
#include "../../Logger.h"

SerialFileSender::SerialFileSender(HardwareSerial *stream)
{
    serialStream = stream;
    state = FS_IDLE;
    filePosition = 0;
    lastComm = 0;
    bytePos = 0;
    active = false;
}

bool SerialFileSender::isActive()
{
    return active;
}

void SerialFileSender::loop()
{
    //check to see if there is a loss of activity
    if (active && (millis() - lastComm) > 1000)
    {
        Logger::error("Lost comm. Aborting the transfer!");
        state = FS_IDLE;
        active = false;
        file.close();
    }
}

void SerialFileSender::processCharacter(char c)
{

    switch (state)
    {
    case FS_IDLE:
        if (c == START_TRANSFER) //other side wants a transfer
        {
            active = true;
            state = RX_FILENAME;
            memset(fname, 0, 128);
            fileSize = 0;
            lastComm = millis();
            Logger::debug("ESP32 requests to send us a file");
        }
        break;
    case RX_FILENAME:
        lastComm = millis();
        fname[bytePos++] = c;
        if (c == 0)
        {
            state = RX_FILESIZE;
            bytePos = 0;
        }
        break;
    case RX_FILESIZE:
        lastComm = millis();
        fileSize = (fileSize << 8) + c;
        bytePos++;
        if (bytePos == 4)
        {
            bytePos = 0;
            expectedCRC = 0;
            state = RX_PACKET;
            serialStream->write(ACK);
            if (!file.open(fname, O_WRITE))
            {                
                Logger::error("Error opening file %s for writing! Abort!");
                state = FS_IDLE;
                file.close();
                active = false;
                return;
            }
        }
        break;
    case RX_PACKET:
        lastComm = millis();
        if (bytePos < 512) packet[bytePos++] = c;
        else
        {
            expectedCRC = (expectedCRC << 8) + c;
            bytePos++;
        }
        if (bytePos == 514)
        {
            uint16_t crc = CRC16.xmodem(packet, 512);
            if (crc == expectedCRC)
            {
                int sz = 512;
                if (filePosition + 512 > fileSize) sz = fileSize - filePosition;
                file.write(packet, sz);
                filePosition += sz;
                serialStream->write(ACK);
                if (filePosition >= fileSize)
                {
                    state = FS_IDLE;
                    active = false;
                    file.flush();
                    file.close();
                    Logger::debug("Successfully transferred file: %s", fname);
                }
            }
            else
            {
                Logger::debug("CRC error in packet. Expected: %x calculated: %x", expectedCRC, crc);
                serialStream->write(NAK);
            }
        }
        break;
    case WAITING_FOR_HEADER_ACK:
        if (c == ACK) //got ack, send first chunk of file
        {
            errCounter = 0;
            filePosition = 0;
            lastComm = millis();
            sendNextChunk();
        }
        if (c == NAK) //resend the header
        {
            Logger::debug("ESP32 NAK while sending file at position %u", filePosition);
            lastComm = millis();
            errCounter++;
            if (errCounter > 4) 
            {   
                state = FS_IDLE;
                active = false;
                file.close();
                return;
            }
            serialStream->write(fname, sizeof(fname) + 1); //send filename including terminating null
            serialStream->write((uint8_t *)&fileSize, 4);
        }
        break;
    case WAITING_FOR_PACKET_ACK:
        if (c == ACK)
        {
            errCounter = 0;
            lastComm = millis();
            //first check whether that was the last chunk
            if (!lastchunk)
            {
                filePosition += 512;
                sendNextChunk();
            }
            else
            {
                state = FS_IDLE;
                active = false;
                file.close();
            }
        }
        if (c == NAK)
        {
            lastComm = millis();
            errCounter++;
            if (errCounter > 4) 
            {   
                state = FS_IDLE;
                active = false;
                file.close();
                return;
            }            
            sendNextChunk(); //send same chunk again
        }
        break;
    }
}

void SerialFileSender::sendNextChunk()
{
    int bytesToRead = 512;
    uint32_t endPosition = filePosition + 512;
    if (endPosition > fileSize)
    {
        bytesToRead = fileSize - filePosition;
        lastchunk = true;        
    }
    file.seek(filePosition);
    file.read(packet, bytesToRead);
    if (bytesToRead < 512) memset(packet + bytesToRead, 0, 512 - bytesToRead); //zero out unused space    
    serialStream->write(packet, 512);
    uint16_t crc = CRC16.xmodem(packet, 512);
    Logger::debug("CRC of packet is %x", crc);
    serialStream->write((uint8_t *)&crc, 2);
    lastComm = millis();
}

/*
File transfers are started from the sender. It sends 0xD0 as a start of transfer
signal. Then it sends a header with the filename and size. The other side ACKs
that it is ready with 0xAA. Then the sender sends 0xDA, followed by a sequence
number (0-255), followed by 512 bytes (padding if needed), followed by the CRC16
of the 512 bytes. The receiver then ACKs (0xAA, followed by sequence #) to show it
has received the chunk, confirmed the CRC, and is ready for another chunk. 0x26 is instead
used for NAK (was going to use 0x55 but that's just a bit shift away from AA which is not ideal)
If NAK then we need to resend that last packet. 0xFA signals the receiver wants to ABORT.
*/
void SerialFileSender::sendFile(char *filename)
{    
    if (file.open(filename, O_READ))
    {
        active = true;
        serialStream->write(START_TRANSFER);
        fileSize = file.fileSize();
        //write header
        strncpy(fname, filename, 128);
        serialStream->write(filename, sizeof(filename) + 1); //send filename including terminating null
        serialStream->write((uint8_t *)&fileSize, 4);
        state = WAITING_FOR_HEADER_ACK;
        lastComm = millis();
    }
}

void SerialFileSender::receiveFile()
{

}