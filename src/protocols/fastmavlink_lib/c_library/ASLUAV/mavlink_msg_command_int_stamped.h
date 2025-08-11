//------------------------------
// The fastMavlink library
// (c) OlliW, OlliW42, www.olliw.eu
//------------------------------

#pragma once
#ifndef FASTMAVLINK_MSG_COMMAND_INT_STAMPED_H
#define FASTMAVLINK_MSG_COMMAND_INT_STAMPED_H


//----------------------------------------
//-- Message COMMAND_INT_STAMPED
//----------------------------------------

// fields are ordered, as they appear on the wire
FASTMAVLINK_PACK(
typedef struct _fmav_command_int_stamped_t {
    uint64_t vehicle_timestamp;
    uint32_t utc_time;
    float param1;
    float param2;
    float param3;
    float param4;
    int32_t x;
    int32_t y;
    float z;
    uint16_t command;
    uint8_t target_system;
    uint8_t target_component;
    uint8_t frame;
    uint8_t current;
    uint8_t autocontinue;
}) fmav_command_int_stamped_t;


#define FASTMAVLINK_MSG_ID_COMMAND_INT_STAMPED  223

#define FASTMAVLINK_MSG_COMMAND_INT_STAMPED_PAYLOAD_LEN_MAX  47
#define FASTMAVLINK_MSG_COMMAND_INT_STAMPED_CRCEXTRA  119

#define FASTMAVLINK_MSG_COMMAND_INT_STAMPED_FLAGS  3
#define FASTMAVLINK_MSG_COMMAND_INT_STAMPED_TARGET_SYSTEM_OFS  42
#define FASTMAVLINK_MSG_COMMAND_INT_STAMPED_TARGET_COMPONENT_OFS  43

#define FASTMAVLINK_MSG_COMMAND_INT_STAMPED_FRAME_LEN_MAX  72



#define FASTMAVLINK_MSG_COMMAND_INT_STAMPED_FIELD_VEHICLE_TIMESTAMP_OFS  0
#define FASTMAVLINK_MSG_COMMAND_INT_STAMPED_FIELD_UTC_TIME_OFS  8
#define FASTMAVLINK_MSG_COMMAND_INT_STAMPED_FIELD_PARAM1_OFS  12
#define FASTMAVLINK_MSG_COMMAND_INT_STAMPED_FIELD_PARAM2_OFS  16
#define FASTMAVLINK_MSG_COMMAND_INT_STAMPED_FIELD_PARAM3_OFS  20
#define FASTMAVLINK_MSG_COMMAND_INT_STAMPED_FIELD_PARAM4_OFS  24
#define FASTMAVLINK_MSG_COMMAND_INT_STAMPED_FIELD_X_OFS  28
#define FASTMAVLINK_MSG_COMMAND_INT_STAMPED_FIELD_Y_OFS  32
#define FASTMAVLINK_MSG_COMMAND_INT_STAMPED_FIELD_Z_OFS  36
#define FASTMAVLINK_MSG_COMMAND_INT_STAMPED_FIELD_COMMAND_OFS  40
#define FASTMAVLINK_MSG_COMMAND_INT_STAMPED_FIELD_TARGET_SYSTEM_OFS  42
#define FASTMAVLINK_MSG_COMMAND_INT_STAMPED_FIELD_TARGET_COMPONENT_OFS  43
#define FASTMAVLINK_MSG_COMMAND_INT_STAMPED_FIELD_FRAME_OFS  44
#define FASTMAVLINK_MSG_COMMAND_INT_STAMPED_FIELD_CURRENT_OFS  45
#define FASTMAVLINK_MSG_COMMAND_INT_STAMPED_FIELD_AUTOCONTINUE_OFS  46


//----------------------------------------
//-- Message COMMAND_INT_STAMPED pack,encode routines, for sending
//----------------------------------------

FASTMAVLINK_FUNCTION_DECORATOR uint16_t fmav_msg_command_int_stamped_pack(
    fmav_message_t* _msg,
    uint8_t sysid,
    uint8_t compid,
    uint32_t utc_time, uint64_t vehicle_timestamp, uint8_t target_system, uint8_t target_component, uint8_t frame, uint16_t command, uint8_t current, uint8_t autocontinue, float param1, float param2, float param3, float param4, int32_t x, int32_t y, float z,
    fmav_status_t* _status)
{
    fmav_command_int_stamped_t* _payload = (fmav_command_int_stamped_t*)_msg->payload;

    _payload->vehicle_timestamp = vehicle_timestamp;
    _payload->utc_time = utc_time;
    _payload->param1 = param1;
    _payload->param2 = param2;
    _payload->param3 = param3;
    _payload->param4 = param4;
    _payload->x = x;
    _payload->y = y;
    _payload->z = z;
    _payload->command = command;
    _payload->target_system = target_system;
    _payload->target_component = target_component;
    _payload->frame = frame;
    _payload->current = current;
    _payload->autocontinue = autocontinue;


    _msg->sysid = sysid;
    _msg->compid = compid;
    _msg->msgid = FASTMAVLINK_MSG_ID_COMMAND_INT_STAMPED;
    _msg->target_sysid = target_system;
    _msg->target_compid = target_component;
    _msg->crc_extra = FASTMAVLINK_MSG_COMMAND_INT_STAMPED_CRCEXTRA;
    _msg->payload_max_len = FASTMAVLINK_MSG_COMMAND_INT_STAMPED_PAYLOAD_LEN_MAX;

    return fmav_finalize_msg(_msg, _status);
}


FASTMAVLINK_FUNCTION_DECORATOR uint16_t fmav_msg_command_int_stamped_encode(
    fmav_message_t* _msg,
    uint8_t sysid,
    uint8_t compid,
    const fmav_command_int_stamped_t* _payload,
    fmav_status_t* _status)
{
    return fmav_msg_command_int_stamped_pack(
        _msg, sysid, compid,
        _payload->utc_time, _payload->vehicle_timestamp, _payload->target_system, _payload->target_component, _payload->frame, _payload->command, _payload->current, _payload->autocontinue, _payload->param1, _payload->param2, _payload->param3, _payload->param4, _payload->x, _payload->y, _payload->z,
        _status);
}


FASTMAVLINK_FUNCTION_DECORATOR uint16_t fmav_msg_command_int_stamped_pack_to_frame_buf(
    uint8_t* _buf,
    uint8_t sysid,
    uint8_t compid,
    uint32_t utc_time, uint64_t vehicle_timestamp, uint8_t target_system, uint8_t target_component, uint8_t frame, uint16_t command, uint8_t current, uint8_t autocontinue, float param1, float param2, float param3, float param4, int32_t x, int32_t y, float z,
    fmav_status_t* _status)
{
    fmav_command_int_stamped_t* _payload = (fmav_command_int_stamped_t*)(&_buf[FASTMAVLINK_HEADER_V2_LEN]);

    _payload->vehicle_timestamp = vehicle_timestamp;
    _payload->utc_time = utc_time;
    _payload->param1 = param1;
    _payload->param2 = param2;
    _payload->param3 = param3;
    _payload->param4 = param4;
    _payload->x = x;
    _payload->y = y;
    _payload->z = z;
    _payload->command = command;
    _payload->target_system = target_system;
    _payload->target_component = target_component;
    _payload->frame = frame;
    _payload->current = current;
    _payload->autocontinue = autocontinue;


    _buf[5] = sysid;
    _buf[6] = compid;
    _buf[7] = (uint8_t)FASTMAVLINK_MSG_ID_COMMAND_INT_STAMPED;
    _buf[8] = ((uint32_t)FASTMAVLINK_MSG_ID_COMMAND_INT_STAMPED >> 8);
    _buf[9] = ((uint32_t)FASTMAVLINK_MSG_ID_COMMAND_INT_STAMPED >> 16);

    return fmav_finalize_frame_buf(
        _buf,
        FASTMAVLINK_MSG_COMMAND_INT_STAMPED_PAYLOAD_LEN_MAX,
        FASTMAVLINK_MSG_COMMAND_INT_STAMPED_CRCEXTRA,
        _status);
}


FASTMAVLINK_FUNCTION_DECORATOR uint16_t fmav_msg_command_int_stamped_encode_to_frame_buf(
    uint8_t* _buf,
    uint8_t sysid,
    uint8_t compid,
    const fmav_command_int_stamped_t* _payload,
    fmav_status_t* _status)
{
    return fmav_msg_command_int_stamped_pack_to_frame_buf(
        _buf, sysid, compid,
        _payload->utc_time, _payload->vehicle_timestamp, _payload->target_system, _payload->target_component, _payload->frame, _payload->command, _payload->current, _payload->autocontinue, _payload->param1, _payload->param2, _payload->param3, _payload->param4, _payload->x, _payload->y, _payload->z,
        _status);
}


#ifdef FASTMAVLINK_SERIAL_WRITE_CHAR

FASTMAVLINK_FUNCTION_DECORATOR uint16_t fmav_msg_command_int_stamped_pack_to_serial(
    uint8_t sysid,
    uint8_t compid,
    uint32_t utc_time, uint64_t vehicle_timestamp, uint8_t target_system, uint8_t target_component, uint8_t frame, uint16_t command, uint8_t current, uint8_t autocontinue, float param1, float param2, float param3, float param4, int32_t x, int32_t y, float z,
    fmav_status_t* _status)
{
    fmav_command_int_stamped_t _payload;

    _payload.vehicle_timestamp = vehicle_timestamp;
    _payload.utc_time = utc_time;
    _payload.param1 = param1;
    _payload.param2 = param2;
    _payload.param3 = param3;
    _payload.param4 = param4;
    _payload.x = x;
    _payload.y = y;
    _payload.z = z;
    _payload.command = command;
    _payload.target_system = target_system;
    _payload.target_component = target_component;
    _payload.frame = frame;
    _payload.current = current;
    _payload.autocontinue = autocontinue;


    return fmav_finalize_serial(
        sysid,
        compid,
        (uint8_t*)&_payload,
        FASTMAVLINK_MSG_ID_COMMAND_INT_STAMPED,
        FASTMAVLINK_MSG_COMMAND_INT_STAMPED_PAYLOAD_LEN_MAX,
        FASTMAVLINK_MSG_COMMAND_INT_STAMPED_CRCEXTRA,
        _status);
}


FASTMAVLINK_FUNCTION_DECORATOR uint16_t fmav_msg_command_int_stamped_encode_to_serial(
    uint8_t sysid,
    uint8_t compid,
    const fmav_command_int_stamped_t* _payload,
    fmav_status_t* _status)
{
    return fmav_finalize_serial(
        sysid,
        compid,
        (uint8_t*)_payload,
        FASTMAVLINK_MSG_ID_COMMAND_INT_STAMPED,
        FASTMAVLINK_MSG_COMMAND_INT_STAMPED_PAYLOAD_LEN_MAX,
        FASTMAVLINK_MSG_COMMAND_INT_STAMPED_CRCEXTRA,
        _status);
}
#endif


//----------------------------------------
//-- Message COMMAND_INT_STAMPED decode routines, for receiving
//----------------------------------------
// For these functions to work correctly, the msg payload must be zero-filled.
// Call the helper fmav_msg_zerofill() if needed, or set FASTMAVLINK_ALWAYS_ZEROFILL to 1
// Note that the parse functions do zero-fill the msg payload, but that message generator functions
// do not. This means that for the msg obtained from parsing the below functions can safely be used,
// but that this is not so for the msg obtained from pack/encode functions.

FASTMAVLINK_FUNCTION_DECORATOR void fmav_msg_command_int_stamped_decode(fmav_command_int_stamped_t* payload, const fmav_message_t* msg)
{
#if FASTMAVLINK_ALWAYS_ZEROFILL
    if (msg->len < FASTMAVLINK_MSG_COMMAND_INT_STAMPED_PAYLOAD_LEN_MAX) {
        memcpy(payload, msg->payload, msg->len);
        // ensure that returned payload is zero-filled
        memset(&(((uint8_t*)payload)[msg->len]), 0, FASTMAVLINK_MSG_COMMAND_INT_STAMPED_PAYLOAD_LEN_MAX - msg->len);
    } else {
        // note: msg->len can be larger than PAYLOAD_LEN_MAX if the message has unknown extensions
        memcpy(payload, msg->payload, FASTMAVLINK_MSG_COMMAND_INT_STAMPED_PAYLOAD_LEN_MAX);
    }
#else
    // this requires that msg payload had been zero-filled before
    memcpy(payload, msg->payload, FASTMAVLINK_MSG_COMMAND_INT_STAMPED_PAYLOAD_LEN_MAX);
#endif
}


FASTMAVLINK_FUNCTION_DECORATOR uint64_t fmav_msg_command_int_stamped_get_field_vehicle_timestamp(const fmav_message_t* msg)
{
    uint64_t r;
    memcpy(&r, &(msg->payload[0]), sizeof(uint64_t));
    return r;
}


FASTMAVLINK_FUNCTION_DECORATOR uint32_t fmav_msg_command_int_stamped_get_field_utc_time(const fmav_message_t* msg)
{
    uint32_t r;
    memcpy(&r, &(msg->payload[8]), sizeof(uint32_t));
    return r;
}


FASTMAVLINK_FUNCTION_DECORATOR float fmav_msg_command_int_stamped_get_field_param1(const fmav_message_t* msg)
{
    float r;
    memcpy(&r, &(msg->payload[12]), sizeof(float));
    return r;
}


FASTMAVLINK_FUNCTION_DECORATOR float fmav_msg_command_int_stamped_get_field_param2(const fmav_message_t* msg)
{
    float r;
    memcpy(&r, &(msg->payload[16]), sizeof(float));
    return r;
}


FASTMAVLINK_FUNCTION_DECORATOR float fmav_msg_command_int_stamped_get_field_param3(const fmav_message_t* msg)
{
    float r;
    memcpy(&r, &(msg->payload[20]), sizeof(float));
    return r;
}


FASTMAVLINK_FUNCTION_DECORATOR float fmav_msg_command_int_stamped_get_field_param4(const fmav_message_t* msg)
{
    float r;
    memcpy(&r, &(msg->payload[24]), sizeof(float));
    return r;
}


FASTMAVLINK_FUNCTION_DECORATOR int32_t fmav_msg_command_int_stamped_get_field_x(const fmav_message_t* msg)
{
    int32_t r;
    memcpy(&r, &(msg->payload[28]), sizeof(int32_t));
    return r;
}


FASTMAVLINK_FUNCTION_DECORATOR int32_t fmav_msg_command_int_stamped_get_field_y(const fmav_message_t* msg)
{
    int32_t r;
    memcpy(&r, &(msg->payload[32]), sizeof(int32_t));
    return r;
}


FASTMAVLINK_FUNCTION_DECORATOR float fmav_msg_command_int_stamped_get_field_z(const fmav_message_t* msg)
{
    float r;
    memcpy(&r, &(msg->payload[36]), sizeof(float));
    return r;
}


FASTMAVLINK_FUNCTION_DECORATOR uint16_t fmav_msg_command_int_stamped_get_field_command(const fmav_message_t* msg)
{
    uint16_t r;
    memcpy(&r, &(msg->payload[40]), sizeof(uint16_t));
    return r;
}


FASTMAVLINK_FUNCTION_DECORATOR uint8_t fmav_msg_command_int_stamped_get_field_target_system(const fmav_message_t* msg)
{
    uint8_t r;
    memcpy(&r, &(msg->payload[42]), sizeof(uint8_t));
    return r;
}


FASTMAVLINK_FUNCTION_DECORATOR uint8_t fmav_msg_command_int_stamped_get_field_target_component(const fmav_message_t* msg)
{
    uint8_t r;
    memcpy(&r, &(msg->payload[43]), sizeof(uint8_t));
    return r;
}


FASTMAVLINK_FUNCTION_DECORATOR uint8_t fmav_msg_command_int_stamped_get_field_frame(const fmav_message_t* msg)
{
    uint8_t r;
    memcpy(&r, &(msg->payload[44]), sizeof(uint8_t));
    return r;
}


FASTMAVLINK_FUNCTION_DECORATOR uint8_t fmav_msg_command_int_stamped_get_field_current(const fmav_message_t* msg)
{
    uint8_t r;
    memcpy(&r, &(msg->payload[45]), sizeof(uint8_t));
    return r;
}


FASTMAVLINK_FUNCTION_DECORATOR uint8_t fmav_msg_command_int_stamped_get_field_autocontinue(const fmav_message_t* msg)
{
    uint8_t r;
    memcpy(&r, &(msg->payload[46]), sizeof(uint8_t));
    return r;
}





//----------------------------------------
//-- Pymavlink wrappers
//----------------------------------------
#ifdef FASTMAVLINK_PYMAVLINK_ENABLED

#define MAVLINK_MSG_ID_COMMAND_INT_STAMPED  223

#define mavlink_command_int_stamped_t  fmav_command_int_stamped_t

#define MAVLINK_MSG_ID_COMMAND_INT_STAMPED_LEN  47
#define MAVLINK_MSG_ID_COMMAND_INT_STAMPED_MIN_LEN  47
#define MAVLINK_MSG_ID_223_LEN  47
#define MAVLINK_MSG_ID_223_MIN_LEN  47

#define MAVLINK_MSG_ID_COMMAND_INT_STAMPED_CRC  119
#define MAVLINK_MSG_ID_223_CRC  119




#if MAVLINK_COMM_NUM_BUFFERS > 0

FASTMAVLINK_FUNCTION_DECORATOR uint16_t mavlink_msg_command_int_stamped_pack(
    uint8_t sysid,
    uint8_t compid,
    mavlink_message_t* _msg,
    uint32_t utc_time, uint64_t vehicle_timestamp, uint8_t target_system, uint8_t target_component, uint8_t frame, uint16_t command, uint8_t current, uint8_t autocontinue, float param1, float param2, float param3, float param4, int32_t x, int32_t y, float z)
{
    fmav_status_t* _status = mavlink_get_channel_status(MAVLINK_COMM_0);
    return fmav_msg_command_int_stamped_pack(
        _msg, sysid, compid,
        utc_time, vehicle_timestamp, target_system, target_component, frame, command, current, autocontinue, param1, param2, param3, param4, x, y, z,
        _status);
}


FASTMAVLINK_FUNCTION_DECORATOR uint16_t mavlink_msg_command_int_stamped_encode(
    uint8_t sysid,
    uint8_t compid,
    mavlink_message_t* _msg,
    const mavlink_command_int_stamped_t* _payload)
{
    return mavlink_msg_command_int_stamped_pack(
        sysid,
        compid,
        _msg,
        _payload->utc_time, _payload->vehicle_timestamp, _payload->target_system, _payload->target_component, _payload->frame, _payload->command, _payload->current, _payload->autocontinue, _payload->param1, _payload->param2, _payload->param3, _payload->param4, _payload->x, _payload->y, _payload->z);
}

#endif


FASTMAVLINK_FUNCTION_DECORATOR uint16_t mavlink_msg_command_int_stamped_pack_txbuf(
    char* _buf,
    fmav_status_t* _status,
    uint8_t sysid,
    uint8_t compid,
    uint32_t utc_time, uint64_t vehicle_timestamp, uint8_t target_system, uint8_t target_component, uint8_t frame, uint16_t command, uint8_t current, uint8_t autocontinue, float param1, float param2, float param3, float param4, int32_t x, int32_t y, float z)
{
    return fmav_msg_command_int_stamped_pack_to_frame_buf(
        (uint8_t*)_buf,
        sysid,
        compid,
        utc_time, vehicle_timestamp, target_system, target_component, frame, command, current, autocontinue, param1, param2, param3, param4, x, y, z,
        _status);
}


FASTMAVLINK_FUNCTION_DECORATOR void mavlink_msg_command_int_stamped_decode(const mavlink_message_t* msg, mavlink_command_int_stamped_t* payload)
{
    fmav_msg_command_int_stamped_decode(payload, msg);
}

#endif // FASTMAVLINK_PYMAVLINK_ENABLED


#endif // FASTMAVLINK_MSG_COMMAND_INT_STAMPED_H
