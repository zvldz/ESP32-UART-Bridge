//------------------------------
// The fastMavlink library
// (c) OlliW, OlliW42, www.olliw.eu
//------------------------------

#pragma once
#ifndef FASTMAVLINK_MSG_RALLY_FETCH_POINT_H
#define FASTMAVLINK_MSG_RALLY_FETCH_POINT_H


//----------------------------------------
//-- Message RALLY_FETCH_POINT
//----------------------------------------

// fields are ordered, as they appear on the wire
FASTMAVLINK_PACK(
typedef struct _fmav_rally_fetch_point_t {
    uint8_t target_system;
    uint8_t target_component;
    uint8_t idx;
}) fmav_rally_fetch_point_t;


#define FASTMAVLINK_MSG_ID_RALLY_FETCH_POINT  176

#define FASTMAVLINK_MSG_RALLY_FETCH_POINT_PAYLOAD_LEN_MAX  3
#define FASTMAVLINK_MSG_RALLY_FETCH_POINT_CRCEXTRA  234

#define FASTMAVLINK_MSG_RALLY_FETCH_POINT_FLAGS  3
#define FASTMAVLINK_MSG_RALLY_FETCH_POINT_TARGET_SYSTEM_OFS  0
#define FASTMAVLINK_MSG_RALLY_FETCH_POINT_TARGET_COMPONENT_OFS  1

#define FASTMAVLINK_MSG_RALLY_FETCH_POINT_FRAME_LEN_MAX  28



#define FASTMAVLINK_MSG_RALLY_FETCH_POINT_FIELD_TARGET_SYSTEM_OFS  0
#define FASTMAVLINK_MSG_RALLY_FETCH_POINT_FIELD_TARGET_COMPONENT_OFS  1
#define FASTMAVLINK_MSG_RALLY_FETCH_POINT_FIELD_IDX_OFS  2


//----------------------------------------
//-- Message RALLY_FETCH_POINT pack,encode routines, for sending
//----------------------------------------

FASTMAVLINK_FUNCTION_DECORATOR uint16_t fmav_msg_rally_fetch_point_pack(
    fmav_message_t* _msg,
    uint8_t sysid,
    uint8_t compid,
    uint8_t target_system, uint8_t target_component, uint8_t idx,
    fmav_status_t* _status)
{
    fmav_rally_fetch_point_t* _payload = (fmav_rally_fetch_point_t*)_msg->payload;

    _payload->target_system = target_system;
    _payload->target_component = target_component;
    _payload->idx = idx;


    _msg->sysid = sysid;
    _msg->compid = compid;
    _msg->msgid = FASTMAVLINK_MSG_ID_RALLY_FETCH_POINT;
    _msg->target_sysid = target_system;
    _msg->target_compid = target_component;
    _msg->crc_extra = FASTMAVLINK_MSG_RALLY_FETCH_POINT_CRCEXTRA;
    _msg->payload_max_len = FASTMAVLINK_MSG_RALLY_FETCH_POINT_PAYLOAD_LEN_MAX;

    return fmav_finalize_msg(_msg, _status);
}


FASTMAVLINK_FUNCTION_DECORATOR uint16_t fmav_msg_rally_fetch_point_encode(
    fmav_message_t* _msg,
    uint8_t sysid,
    uint8_t compid,
    const fmav_rally_fetch_point_t* _payload,
    fmav_status_t* _status)
{
    return fmav_msg_rally_fetch_point_pack(
        _msg, sysid, compid,
        _payload->target_system, _payload->target_component, _payload->idx,
        _status);
}


FASTMAVLINK_FUNCTION_DECORATOR uint16_t fmav_msg_rally_fetch_point_pack_to_frame_buf(
    uint8_t* _buf,
    uint8_t sysid,
    uint8_t compid,
    uint8_t target_system, uint8_t target_component, uint8_t idx,
    fmav_status_t* _status)
{
    fmav_rally_fetch_point_t* _payload = (fmav_rally_fetch_point_t*)(&_buf[FASTMAVLINK_HEADER_V2_LEN]);

    _payload->target_system = target_system;
    _payload->target_component = target_component;
    _payload->idx = idx;


    _buf[5] = sysid;
    _buf[6] = compid;
    _buf[7] = (uint8_t)FASTMAVLINK_MSG_ID_RALLY_FETCH_POINT;
    _buf[8] = ((uint32_t)FASTMAVLINK_MSG_ID_RALLY_FETCH_POINT >> 8);
    _buf[9] = ((uint32_t)FASTMAVLINK_MSG_ID_RALLY_FETCH_POINT >> 16);

    return fmav_finalize_frame_buf(
        _buf,
        FASTMAVLINK_MSG_RALLY_FETCH_POINT_PAYLOAD_LEN_MAX,
        FASTMAVLINK_MSG_RALLY_FETCH_POINT_CRCEXTRA,
        _status);
}


FASTMAVLINK_FUNCTION_DECORATOR uint16_t fmav_msg_rally_fetch_point_encode_to_frame_buf(
    uint8_t* _buf,
    uint8_t sysid,
    uint8_t compid,
    const fmav_rally_fetch_point_t* _payload,
    fmav_status_t* _status)
{
    return fmav_msg_rally_fetch_point_pack_to_frame_buf(
        _buf, sysid, compid,
        _payload->target_system, _payload->target_component, _payload->idx,
        _status);
}


#ifdef FASTMAVLINK_SERIAL_WRITE_CHAR

FASTMAVLINK_FUNCTION_DECORATOR uint16_t fmav_msg_rally_fetch_point_pack_to_serial(
    uint8_t sysid,
    uint8_t compid,
    uint8_t target_system, uint8_t target_component, uint8_t idx,
    fmav_status_t* _status)
{
    fmav_rally_fetch_point_t _payload;

    _payload.target_system = target_system;
    _payload.target_component = target_component;
    _payload.idx = idx;


    return fmav_finalize_serial(
        sysid,
        compid,
        (uint8_t*)&_payload,
        FASTMAVLINK_MSG_ID_RALLY_FETCH_POINT,
        FASTMAVLINK_MSG_RALLY_FETCH_POINT_PAYLOAD_LEN_MAX,
        FASTMAVLINK_MSG_RALLY_FETCH_POINT_CRCEXTRA,
        _status);
}


FASTMAVLINK_FUNCTION_DECORATOR uint16_t fmav_msg_rally_fetch_point_encode_to_serial(
    uint8_t sysid,
    uint8_t compid,
    const fmav_rally_fetch_point_t* _payload,
    fmav_status_t* _status)
{
    return fmav_finalize_serial(
        sysid,
        compid,
        (uint8_t*)_payload,
        FASTMAVLINK_MSG_ID_RALLY_FETCH_POINT,
        FASTMAVLINK_MSG_RALLY_FETCH_POINT_PAYLOAD_LEN_MAX,
        FASTMAVLINK_MSG_RALLY_FETCH_POINT_CRCEXTRA,
        _status);
}
#endif


//----------------------------------------
//-- Message RALLY_FETCH_POINT decode routines, for receiving
//----------------------------------------
// For these functions to work correctly, the msg payload must be zero-filled.
// Call the helper fmav_msg_zerofill() if needed, or set FASTMAVLINK_ALWAYS_ZEROFILL to 1
// Note that the parse functions do zero-fill the msg payload, but that message generator functions
// do not. This means that for the msg obtained from parsing the below functions can safely be used,
// but that this is not so for the msg obtained from pack/encode functions.

FASTMAVLINK_FUNCTION_DECORATOR void fmav_msg_rally_fetch_point_decode(fmav_rally_fetch_point_t* payload, const fmav_message_t* msg)
{
#if FASTMAVLINK_ALWAYS_ZEROFILL
    if (msg->len < FASTMAVLINK_MSG_RALLY_FETCH_POINT_PAYLOAD_LEN_MAX) {
        memcpy(payload, msg->payload, msg->len);
        // ensure that returned payload is zero-filled
        memset(&(((uint8_t*)payload)[msg->len]), 0, FASTMAVLINK_MSG_RALLY_FETCH_POINT_PAYLOAD_LEN_MAX - msg->len);
    } else {
        // note: msg->len can be larger than PAYLOAD_LEN_MAX if the message has unknown extensions
        memcpy(payload, msg->payload, FASTMAVLINK_MSG_RALLY_FETCH_POINT_PAYLOAD_LEN_MAX);
    }
#else
    // this requires that msg payload had been zero-filled before
    memcpy(payload, msg->payload, FASTMAVLINK_MSG_RALLY_FETCH_POINT_PAYLOAD_LEN_MAX);
#endif
}


FASTMAVLINK_FUNCTION_DECORATOR uint8_t fmav_msg_rally_fetch_point_get_field_target_system(const fmav_message_t* msg)
{
    uint8_t r;
    memcpy(&r, &(msg->payload[0]), sizeof(uint8_t));
    return r;
}


FASTMAVLINK_FUNCTION_DECORATOR uint8_t fmav_msg_rally_fetch_point_get_field_target_component(const fmav_message_t* msg)
{
    uint8_t r;
    memcpy(&r, &(msg->payload[1]), sizeof(uint8_t));
    return r;
}


FASTMAVLINK_FUNCTION_DECORATOR uint8_t fmav_msg_rally_fetch_point_get_field_idx(const fmav_message_t* msg)
{
    uint8_t r;
    memcpy(&r, &(msg->payload[2]), sizeof(uint8_t));
    return r;
}





//----------------------------------------
//-- Pymavlink wrappers
//----------------------------------------
#ifdef FASTMAVLINK_PYMAVLINK_ENABLED

#define MAVLINK_MSG_ID_RALLY_FETCH_POINT  176

#define mavlink_rally_fetch_point_t  fmav_rally_fetch_point_t

#define MAVLINK_MSG_ID_RALLY_FETCH_POINT_LEN  3
#define MAVLINK_MSG_ID_RALLY_FETCH_POINT_MIN_LEN  3
#define MAVLINK_MSG_ID_176_LEN  3
#define MAVLINK_MSG_ID_176_MIN_LEN  3

#define MAVLINK_MSG_ID_RALLY_FETCH_POINT_CRC  234
#define MAVLINK_MSG_ID_176_CRC  234




#if MAVLINK_COMM_NUM_BUFFERS > 0

FASTMAVLINK_FUNCTION_DECORATOR uint16_t mavlink_msg_rally_fetch_point_pack(
    uint8_t sysid,
    uint8_t compid,
    mavlink_message_t* _msg,
    uint8_t target_system, uint8_t target_component, uint8_t idx)
{
    fmav_status_t* _status = mavlink_get_channel_status(MAVLINK_COMM_0);
    return fmav_msg_rally_fetch_point_pack(
        _msg, sysid, compid,
        target_system, target_component, idx,
        _status);
}


FASTMAVLINK_FUNCTION_DECORATOR uint16_t mavlink_msg_rally_fetch_point_encode(
    uint8_t sysid,
    uint8_t compid,
    mavlink_message_t* _msg,
    const mavlink_rally_fetch_point_t* _payload)
{
    return mavlink_msg_rally_fetch_point_pack(
        sysid,
        compid,
        _msg,
        _payload->target_system, _payload->target_component, _payload->idx);
}

#endif


FASTMAVLINK_FUNCTION_DECORATOR uint16_t mavlink_msg_rally_fetch_point_pack_txbuf(
    char* _buf,
    fmav_status_t* _status,
    uint8_t sysid,
    uint8_t compid,
    uint8_t target_system, uint8_t target_component, uint8_t idx)
{
    return fmav_msg_rally_fetch_point_pack_to_frame_buf(
        (uint8_t*)_buf,
        sysid,
        compid,
        target_system, target_component, idx,
        _status);
}


FASTMAVLINK_FUNCTION_DECORATOR void mavlink_msg_rally_fetch_point_decode(const mavlink_message_t* msg, mavlink_rally_fetch_point_t* payload)
{
    fmav_msg_rally_fetch_point_decode(payload, msg);
}

#endif // FASTMAVLINK_PYMAVLINK_ENABLED


#endif // FASTMAVLINK_MSG_RALLY_FETCH_POINT_H
