//------------------------------
// The fastMavlink library
// (c) OlliW, OlliW42, www.olliw.eu
//------------------------------

#pragma once
#ifndef FASTMAVLINK_MSG_ENTRIES_H
#define FASTMAVLINK_MSG_ENTRIES_H


//------------------------------
//-- Message credentials
//-- The values of msg_entry_t for all messages in the dialect.
//-- msgid, extra crc, max length, flag, target sysid offset, target compid offset
//------------------------------

#define FASTMAVLINK_MSG_ENTRY_AIRLINK_AUTH  {52000, 13, 100, 0, 0, 0}
#define FASTMAVLINK_MSG_ENTRY_AIRLINK_AUTH_RESPONSE  {52001, 239, 1, 0, 0, 0}
#define FASTMAVLINK_MSG_ENTRY_AIRLINK_EYE_GS_HOLE_PUSH_REQUEST  {52002, 24, 1, 0, 0, 0}
#define FASTMAVLINK_MSG_ENTRY_AIRLINK_EYE_GS_HOLE_PUSH_RESPONSE  {52003, 166, 26, 0, 0, 0}
#define FASTMAVLINK_MSG_ENTRY_AIRLINK_EYE_HP  {52004, 39, 1, 0, 0, 0}
#define FASTMAVLINK_MSG_ENTRY_AIRLINK_EYE_TURN_INIT  {52005, 145, 1, 0, 0, 0}


/*------------------------------
 * If only relatively few MAVLink messages are used, efficiency can
 * be much improved, both memory and computational time wise, by
 * limiting the known message entries to only those which are used.
 *
 * This can be achieved by commenting out in the below define of
 * FASTMAVLINK_MSG_ENTRIES all those message entries which are not used.
 *
 * Alternatively, one can define one's own FASTMAVLINK_MESSAGE_CRCS
 * using the above defines for each message entry. It is then MOST
 * important to keep the sequence in order since otherwise the default
 * binary search will fail. For instance:
 *
 * #include "pathtofastmavlink/thedialect/fmav_msg_entries.h"
 * #define FASTMAVLINK_MESSAGE_CRCS {\
 *     FASTMAVLINK_MSG_ENTRY_PARAM_REQUEST_READ,\
 *     FASTMAVLINK_MSG_ENTRY_PARAM_REQUEST_LIST,\
 *     FASTMAVLINK_MSG_ENTRY_PARAM_SET,\
 *     FASTMAVLINK_MSG_ENTRY_COMMAND_LONG,\
 *     FASTMAVLINK_MSG_ENTRY_AUTOPILOT_VERSION_REQUEST }
 ------------------------------*/

#define FASTMAVLINK_MSG_ENTRIES {\
  FASTMAVLINK_MSG_ENTRY_AIRLINK_AUTH,\
  FASTMAVLINK_MSG_ENTRY_AIRLINK_AUTH_RESPONSE,\
  FASTMAVLINK_MSG_ENTRY_AIRLINK_EYE_GS_HOLE_PUSH_REQUEST,\
  FASTMAVLINK_MSG_ENTRY_AIRLINK_EYE_GS_HOLE_PUSH_RESPONSE,\
  FASTMAVLINK_MSG_ENTRY_AIRLINK_EYE_HP,\
  FASTMAVLINK_MSG_ENTRY_AIRLINK_EYE_TURN_INIT\
}


#endif // FASTMAVLINK_MSG_ENTRIES_H
