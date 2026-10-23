/*
 * Host stand-in for FlexCAN_T4: just the message and error structures that
 * firmware headers embed. No bus hardware is emulated.
 */
#pragma once

#include <Arduino.h>

typedef struct CAN_message_t {
    uint32_t id = 0;
    uint16_t timestamp = 0;
    uint8_t idhit = 0;
    struct {
        bool extended = 0;
        bool remote = 0;
        bool overrun = 0;
        bool reserved = 0;
    } flags;
    uint8_t len = 8;
    uint8_t buf[8] = { 0 };
    int8_t mb = 0;
    uint8_t bus = 0;
    bool seq = 0;
} CAN_message_t;

typedef struct CANFD_message_t {
    uint32_t id = 0;
    uint16_t timestamp = 0;
    uint8_t idhit = 0;
    bool brs = 1;
    bool esi = 0;
    bool edl = 1;
    struct {
        bool extended = 0;
        bool overrun = 0;
    } flags;
    uint8_t len = 8;
    uint8_t buf[64] = { 0 };
    int8_t mb = 0;
    uint8_t bus = 0;
    bool seq = 0;
} CANFD_message_t;

typedef struct CAN_error_t {
    char state[30] = "Idle";
    bool BIT1_ERR = 0;
    bool BIT0_ERR = 0;
    bool ACK_ERR = 0;
    bool CRC_ERR = 0;
    bool FRM_ERR = 0;
    bool STF_ERR = 0;
    bool RX_WRN = 0;
    bool TX_WRN = 0;
    char FLT_CONF[14] = { 0 };
    uint8_t RX_ERR_COUNTER = 0;
    uint8_t TX_ERR_COUNTER = 0;
    uint32_t ESR1 = 0;
    uint16_t ECR = 0;
} CAN_error_t;
