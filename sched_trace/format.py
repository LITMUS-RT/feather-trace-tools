"""Functions for reading and interpreting binary sched_trace files.
"""

import struct

HEADER_LEN  =  8 # bytes
PAYLOAD_LEN = 16 # bytes
RECORD_LEN  = 24 # bytes

EVENT_NAMES = [
    "ST_NAME",
    "ST_PARAM",
    "ST_RELEASE",
    "ST_ASSIGNED",
    "ST_SWITCH_TO",
    "ST_SWITCH_AWAY",
    "ST_COMPLETION",
    "ST_BLOCK",
    "ST_RESUME",
    "ST_ACTION",
    "ST_SYS_RELEASE"
]

EVENTS = {}
for i, ev in enumerate(EVENT_NAMES):
    EVENTS[ev] = i + 1
    EVENTS[i + 1] = ev

def event(id):
    return EVENTS[id] if id in EVENTS else id

# struct st_trace_header {
# 	u8	type;		/* Of what type is this record?  */
# 	u8	cpu;		/* On which CPU was it recorded? */
# 	u16	pid;		/* PID of the task.              */
# 	u32	job;		/* The job sequence number.      */
# };

def get_event(mmaped_trace, index):
    offset = index * RECORD_LEN
    # first byte in the record is the event type ID
    return ord(mmaped_trace[offset])
#    return struct.unpack("B", raw_data[:1])[0]

def unpack_header(raw_data):
    """Returns (type, cpu, pid, job id)"""
    return struct.unpack("BBHI", raw_data[:HEADER_LEN])

def unpack_release(raw_data):
    """Returns (release, deadline)"""
    return struct.unpack("QQ", raw_data[HEADER_LEN:RECORD_LEN])

def unpack_switch_to(raw_data):
    """Returns (when, exec_time_so_far)"""
    return struct.unpack("QI", raw_data[HEADER_LEN:RECORD_LEN - 4])

def unpack_switch_away(raw_data):
    """Returns (when, exec_time_so_far)"""
    return struct.unpack("QQ", raw_data[HEADER_LEN:RECORD_LEN])

def unpack_completion(raw_data):
    """Returns (when, exec_time, forced)"""
    (when, exec_time) = struct.unpack("QQ", raw_data[HEADER_LEN:RECORD_LEN])
    return (when, exec_time >> 1, exec_time & 0x1)

def unpack_block(raw_data):
    """Returns (when,)"""
    return struct.unpack("Q", raw_data[HEADER_LEN:RECORD_LEN - 8])

def unpack_resume(raw_data):
    """Returns (when,)"""
    return struct.unpack("Q", raw_data[HEADER_LEN:RECORD_LEN - 8])

def unpack_sys_release(raw_data):
    """Returns (when, release)"""
    return struct.unpack("QQ", raw_data[HEADER_LEN:RECORD_LEN])

def unpack_name(raw_data):
    """Returns (name,)"""
    data = raw_data[HEADER_LEN:RECORD_LEN]
    null_byte_idx = data.find('\0')
    if null_byte_idx > -1:
        data = data[:null_byte_idx]
    return (data,)

def unpack_param(raw_data):
    """Returns (WCET, period, phase, partition)"""
    return struct.unpack("IIIB", raw_data[HEADER_LEN:RECORD_LEN - 3])

UNPACK = {
    EVENTS['ST_RELEASE']:       unpack_release,
    EVENTS['ST_SWITCH_TO']:     unpack_switch_to,
    EVENTS['ST_SWITCH_AWAY']:   unpack_switch_away,
    EVENTS['ST_COMPLETION']:    unpack_completion,
    EVENTS['ST_BLOCK']:         unpack_block,
    EVENTS['ST_RESUME']:        unpack_resume,
    EVENTS['ST_SYS_RELEASE']:   unpack_sys_release,
    EVENTS['ST_NAME']:          unpack_name,
    EVENTS['ST_PARAM']:         unpack_param,
}

EVENTS_WITHOUT_TIMESTAMP = frozenset([
    EVENTS['ST_NAME'],
    EVENTS['ST_PARAM'],
])

def when(raw_data):
    ev = get_event(raw_data)
    if ev in EVENTS_WITHOUT_TIMESTAMP:
        return 0
    else:
        # always first element for all others
        return UNPACK[ev](raw_data)[0]

def unpack(raw_data):
    hdr = unpack_header(raw_data)
    payload = UNPACK[hdr[0]](raw_data)
    return hdr + payload
