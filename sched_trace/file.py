"""Convenience wrapper around mmap'ed sched_trace binary files.
"""

from .format import RECORD_LEN, unpack, get_event, EVENTS

import mmap

class SchedTraceFile(object):
    def __init__(self, fname):
        self.source = fname
        with open(fname, 'rw') as f:
            self.mem = mmap.mmap(f.fileno(), 0, mmap.MAP_PRIVATE)

    def __len__(self):
        return len(self.mem) / RECORD_LEN

    def events_of_type(self, event_ids):
        for i in xrange(len(self)):
            if get_event(self.mem, i) in event_ids:
                record = self.mem[i * RECORD_LEN:]
                yield unpack(record)

    def __iter__(self):
        for i in xrange(len(self)):
            record = self.mem[i * RECORD_LEN:]
            yield unpack(record)

    def __reversed__(self):
        for i in reversed(xrange(len(self))):
            record = self.mem[i * RECORD_LEN:]
            yield unpack(record)

    def raw_iter(self):
        for i in xrange(len(self)):
            record = self.mem[i * RECORD_LEN:(i + 1) * RECORD_LEN]
            yield record

    def scheduling_intervals(self):
        to_id = EVENTS['ST_SWITCH_TO']
        away_id = EVENTS['ST_SWITCH_AWAY']

        # assumption: we are looking at a per-CPU trace
        events = self.events_of_type(set((to_id, away_id)))

        rec = events.next()
        while True:
            # fast-forward to next SWITCH_TO
            while rec[0] != to_id:
                rec = events.next()

            to = rec
            # next one on this CPU should be a SWITCH_AWAY
            rec = events.next()
            away = rec
            # check for event ID and matching PID and CPU and monotonic time
            if away[0] == away_id and away[1] == to[1] and away[2] == to[2] \
               and to[4] <= away[4]:
                yield (to, away)
