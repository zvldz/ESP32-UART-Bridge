//------------------------------
// The fastMavlink library
// (c) OlliW, OlliW42, www.olliw.eu
//------------------------------

#pragma once
#ifndef FASTMAVLINK_MSG_ARRAY_TEST_4_H
#define FASTMAVLINK_MSG_ARRAY_TEST_4_H


//----------------------------------------
//-- Message ARRAY_TEST_4
//----------------------------------------

// fields are ordered, as they appear on the wire
FASTMAVLINK_PACK(
typedef struct _fmav_array_test_4_t {
    uint32_t ar_u32[4];
    uint8_t v;
}) fmav_array_test_4_t;


#define FASTMAVLINK_MSG_ID_ARRAY_TEST_4  17154

#define FASTMAVLINK_MSG_ARRAY_TEST_4_PAYLOAD_LEN_MAX  17
#define FASTMAVLINK_MSG_ARRAY_TEST_4_CRCEXTRA  89

#define FASTMAVLINK_MSG_ARRAY_TEST_4_FLAGS  0
#define FASTMAVLINK_MSG_ARRAY_TEST_4_TARGET_SYSTEM_OFS  0
#define FASTMAVLINK_MSG_ARRAY_TEST_4_TARGET_COMPONENT_OFS  0

#define FASTMAVLINK_MSG_ARRAY_TEST_4_FRAME_LEN_MAX  42

#define FASTMAVLINK_MSG_ARRAY_TEST_4_FIELD_AR_U32_NUM  4 // number of elements in array
#define FASTMAVLINK_MSG_ARRAY_TEST_4_FIELD_AR_U32_LEN  16 // length of array = number of bytes

#define FASTMAVLINK_MSG_ARRAY_TEST_4_FIELD_AR_U32_OFS  0
#define FASTMAVLINK_MSG_ARRAY_TEST_4_FIELD_V_OFS  16


//----------------------------------------
//-- Message ARRAY_TEST_4 pack,encode routines, for sending
//----------------------------------------

FASTMAVLINK_FUNCTION_DECORATOR uint16_t fmav_msg_array_test_4_pack(
    fmav_message_t* _msg,
    uint8_t sysid,
    uint8_t compid,
    const uint32_t* ar_u32, uint8_t v,
    fmav_status_t* _status)
{
    fmav_array_test_4_t* _payload = (fmav_array_test_4_t*)_msg->payload;

    _payload->v = v;
    memcpy(&(_payload->ar_u32), ar_u32, sizeof(uint32_t)*4);

    _msg->sysid = sysid;
    _msg->compid = compid;
    _msg->msgid = FASTMAVLINK_MSG_ID_ARRAY_TEST_4;
    _msg->target_sysid = 0;
    _msg->target_compid = 0;
    _msg->crc_extra = FASTMAVLINK_MSG_ARRAY_TEST_4_CRCEXTRA;
    _msg->payload_max_len = FASTMAVLINK_MSG_ARRAY_TEST_4_PAYLOAD_LEN_MAX;

    return fmav_finalize_msg(_msg, _status);
}


FASTMAVLINK_FUNCTION_DECORATOR uint16_t fmav_msg_array_test_4_encode(
    fmav_message_t* _msg,
    uint8_t sysid,
    uint8_t compid,
    const fmav_array_test_4_t* _payload,
    fmav_status_t* _status)
{
    return fmav_msg_array_test_4_pack(
        _msg, sysid, compid,
        _payload->ar_u32, _payload->v,
        _status);
}


FASTMAVLINK_FUNCTION_DECORATOR uint16_t fmav_msg_array_test_4_pack_to_frame_buf(
    uint8_t* _buf,
    uint8_t sysid,
    uint8_t compid,
    const uint32_t* ar_u32, uint8_t v,
    fmav_status_t* _status)
{
    fmav_array_test_4_t* _payload = (fmav_array_test_4_t*)(&_buf[FASTMAVLINK_HEADER_V2_LEN]);

    _payload->v = v;
    memcpy(&(_payload->ar_u32), ar_u32, sizeof(uint32_t)*4);

    _buf[5] = sysid;
    _buf[6] = compid;
    _buf[7] = (uint8_t)FASTMAVLINK_MSG_ID_ARRAY_TEST_4;
    _buf[8] = ((uint32_t)FASTMAVLINK_MSG_ID_ARRAY_TEST_4 >> 8);
    _buf[9] = ((uint32_t)FASTMAVLINK_MSG_ID_ARRAY_TEST_4 >> 16);

    return fmav_finalize_frame_buf(
        _buf,
        FASTMAVLINK_MSG_ARRAY_TEST_4_PAYLOAD_LEN_MAX,
        FASTMAVLINK_MSG_ARRAY_TEST_4_CRCEXTRA,
        _status);
}


FASTMAVLINK_FUNCTION_DECORATOR uint16_t fmav_msg_array_test_4_encode_to_frame_buf(
    uint8_t* _buf,
    uint8_t sysid,
    uint8_t compid,
    const fmav_array_test_4_t* _payload,
    fmav_status_t* _status)
{
    return fmav_msg_array_test_4_pack_to_frame_buf(
        _buf, sysid, compid,
        _payload->ar_u32, _payload->v,
        _status);
}


#ifdef FASTMAVLINK_SERIAL_WRITE_CHAR

FASTMAVLINK_FUNCTION_DECORATOR uint16_t fmav_msg_array_test_4_pack_to_serial(
    uint8_t sysid,
    uint8_t compid,
    const uint32_t* ar_u32, uint8_t v,
    fmav_status_t* _status)
{
    fmav_array_test_4_t _payload;

    _payload.v = v;
    memcpy(&(_payload.ar_u32), ar_u32, sizeof(uint32_t)*4);

    return fmav_finalize_serial(
        sysid,
        compid,
        (uint8_t*)&_payload,
        FASTMAVLINK_MSG_ID_ARRAY_TEST_4,
        FASTMAVLINK_MSG_ARRAY_TEST_4_PAYLOAD_LEN_MAX,
        FASTMAVLINK_MSG_ARRAY_TEST_4_CRCEXTRA,
        _status);
}


FASTMAVLINK_FUNCTION_DECORATOR uint16_t fmav_msg_array_test_4_encode_to_serial(
    uint8_t sysid,
    uint8_t compid,
    const fmav_array_test_4_t* _payload,
    fmav_status_t* _status)
{
    return fmav_finalize_serial(
        sysid,
        compid,
        (uint8_t*)_payload,
        FASTMAVLINK_MSG_ID_ARRAY_TEST_4,
        FASTMAVLINK_MSG_ARRAY_TEST_4_PAYLOAD_LEN_MAX,
        FASTMAVLINK_MSG_ARRAY_TEST_4_CRCEXTRA,
        _status);
}
#endif


//----------------------------------------
//-- Message ARRAY_TEST_4 decode routines, for receiving
//----------------------------------------
// For these functions to work correctly, the msg payload must be zero-filled.
// Call the helper fmav_msg_zerofill() if needed, or set FASTMAVLINK_ALWAYS_ZEROFILL to 1
// Note that the parse functions do zero-fill the msg payload, but that message generator functions
// do not. This means that for the msg obtained from parsing the below functions can safely be used,
// but that this is not so for the msg obtained from pack/encode functions.

FASTMAVLINK_FUNCTION_DECORATOR void fmav_msg_array_test_4_decode(fmav_array_test_4_t* payload, const fmav_message_t* msg)
{
#if FASTMAVLINK_ALWAYS_ZEROFILL
    if (msg->len < FASTMAVLINK_MSG_ARRAY_TEST_4_PAYLOAD_LEN_MAX) {
        memcpy(payload, msg->payload, msg->len);
        // ensure that returned payload is zero-filled
        memset(&(((uint8_t*)payload)[msg->len]), 0, FASTMAVLINK_MSG_ARRAY_TEST_4_PAYLOAD_LEN_MAX - msg->len);
    } else {
        // note: msg->len can be larger than PAYLOAD_LEN_MAX if the message has unknown extensions
        memcpy(payload, msg->payload, FASTMAVLINK_MSG_ARRAY_TEST_4_PAYLOAD_LEN_MAX);
    }
#else
    // this requires that msg payload had been zero-filled before
    memcpy(payload, msg->payload, FASTMAVLINK_MSG_ARRAY_TEST_4_PAYLOAD_LEN_MAX);
#endif
}


FASTMAVLINK_FUNCTION_DECORATOR uint8_t fmav_msg_array_test_4_get_field_v(const fmav_message_t* msg)
{
    uint8_t r;
    memcpy(&r, &(msg->payload[16]), sizeof(uint8_t));
    return r;
}


FASTMAVLINK_FUNCTION_DECORATOR uint32_t* fmav_msg_array_test_4_get_field_ar_u32_ptr(const fmav_message_t* msg)
{
    return (uint32_t*)&(msg->payload[0]);
}


FASTMAVLINK_FUNCTION_DECORATOR uint32_t fmav_msg_array_test_4_get_field_ar_u32(uint16_t index, const fmav_message_t* msg)
{
    if (index >= FASTMAVLINK_MSG_ARRAY_TEST_4_FIELD_AR_U32_NUM) return 0;
    return ((uint32_t*)&(msg->payload[0]))[index];
}


//----------------------------------------
//-- Pymavlink wrappers
//----------------------------------------
#ifdef FASTMAVLINK_PYMAVLINK_ENABLED

#define MAVLINK_MSG_ID_ARRAY_TEST_4  17154

#define mavlink_array_test_4_t  fmav_array_test_4_t

#define MAVLINK_MSG_ID_ARRAY_TEST_4_LEN  17
#define MAVLINK_MSG_ID_ARRAY_TEST_4_MIN_LEN  17
#define MAVLINK_MSG_ID_17154_LEN  17
#define MAVLINK_MSG_ID_17154_MIN_LEN  17

#define MAVLINK_MSG_ID_ARRAY_TEST_4_CRC  89
#define MAVLINK_MSG_ID_17154_CRC  89

#define MAVLINK_MSG_ARRAY_TEST_4_FIELD_AR_U32_LEN 4


#if MAVLINK_COMM_NUM_BUFFERS > 0

FASTMAVLINK_FUNCTION_DECORATOR uint16_t mavlink_msg_array_test_4_pack(
    uint8_t sysid,
    uint8_t compid,
    mavlink_message_t* _msg,
    const uint32_t* ar_u32, uint8_t v)
{
    fmav_status_t* _status = mavlink_get_channel_status(MAVLINK_COMM_0);
    return fmav_msg_array_test_4_pack(
        _msg, sysid, compid,
        ar_u32, v,
        _status);
}


FASTMAVLINK_FUNCTION_DECORATOR uint16_t mavlink_msg_array_test_4_encode(
    uint8_t sysid,
    uint8_t compid,
    mavlink_message_t* _msg,
    const mavlink_array_test_4_t* _payload)
{
    return mavlink_msg_array_test_4_pack(
        sysid,
        compid,
        _msg,
        _payload->ar_u32, _payload->v);
}

#endif


FASTMAVLINK_FUNCTION_DECORATOR uint16_t mavlink_msg_array_test_4_pack_txbuf(
    char* _buf,
    fmav_status_t* _status,
    uint8_t sysid,
    uint8_t compid,
    const uint32_t* ar_u32, uint8_t v)
{
    return fmav_msg_array_test_4_pack_to_frame_buf(
        (uint8_t*)_buf,
        sysid,
        compid,
        ar_u32, v,
        _status);
}


FASTMAVLINK_FUNCTION_DECORATOR void mavlink_msg_array_test_4_decode(const mavlink_message_t* msg, mavlink_array_test_4_t* payload)
{
    fmav_msg_array_test_4_decode(payload, msg);
}

#endif // FASTMAVLINK_PYMAVLINK_ENABLED


#endif // FASTMAVLINK_MSG_ARRAY_TEST_4_H
