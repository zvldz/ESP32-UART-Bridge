#include "mavlink_parser.h"

uint8_t MavlinkParser::extractTargetSystem(mavlink_message_t* msg) {
    // Extract target_system for routable messages
    switch(msg->msgid) {
        case MAVLINK_MSG_ID_PARAM_REQUEST_READ:
            return mavlink_msg_param_request_read_get_target_system(msg);
        case MAVLINK_MSG_ID_PARAM_REQUEST_LIST:
            return mavlink_msg_param_request_list_get_target_system(msg);
        case MAVLINK_MSG_ID_PARAM_SET:
            return mavlink_msg_param_set_get_target_system(msg);
        case MAVLINK_MSG_ID_MISSION_REQUEST_LIST:
            return mavlink_msg_mission_request_list_get_target_system(msg);
        case MAVLINK_MSG_ID_MISSION_COUNT:
            return mavlink_msg_mission_count_get_target_system(msg);
        case MAVLINK_MSG_ID_MISSION_ITEM_INT:
            return mavlink_msg_mission_item_int_get_target_system(msg);
        case MAVLINK_MSG_ID_COMMAND_INT:
            return mavlink_msg_command_int_get_target_system(msg);
        case MAVLINK_MSG_ID_COMMAND_LONG:
            return mavlink_msg_command_long_get_target_system(msg);
        case MAVLINK_MSG_ID_FILE_TRANSFER_PROTOCOL:
            return mavlink_msg_file_transfer_protocol_get_target_system(msg);
        default:
            return 0;  // No target field = broadcast
    }
}

uint8_t MavlinkParser::extractTargetComponent(mavlink_message_t* msg) {
    // Extract target_component for routable messages  
    switch(msg->msgid) {
        case MAVLINK_MSG_ID_PARAM_REQUEST_READ:
            return mavlink_msg_param_request_read_get_target_component(msg);
        case MAVLINK_MSG_ID_PARAM_REQUEST_LIST:
            return mavlink_msg_param_request_list_get_target_component(msg);
        case MAVLINK_MSG_ID_PARAM_SET:
            return mavlink_msg_param_set_get_target_component(msg);
        case MAVLINK_MSG_ID_MISSION_REQUEST_LIST:
            return mavlink_msg_mission_request_list_get_target_component(msg);
        case MAVLINK_MSG_ID_MISSION_COUNT:
            return mavlink_msg_mission_count_get_target_component(msg);
        case MAVLINK_MSG_ID_MISSION_ITEM_INT:
            return mavlink_msg_mission_item_int_get_target_component(msg);
        case MAVLINK_MSG_ID_COMMAND_INT:
            return mavlink_msg_command_int_get_target_component(msg);
        case MAVLINK_MSG_ID_COMMAND_LONG:
            return mavlink_msg_command_long_get_target_component(msg);
        case MAVLINK_MSG_ID_FILE_TRANSFER_PROTOCOL:
            return mavlink_msg_file_transfer_protocol_get_target_component(msg);
        default:
            return 0;
    }
}