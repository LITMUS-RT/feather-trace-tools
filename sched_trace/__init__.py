from __future__ import division

from collections import defaultdict
from itertools import imap

import heapq

from .format import event, EVENTS_WITHOUT_TIMESTAMP, EVENTS
from .file import SchedTraceFile

def event_id(unpacked_record):
    "Given a tuple from format.unpack(), return the ID of the event"
    return unpacked_record[0]

def event_cpu(unpacked_record):
    "Given a tuple from format.unpack(), return the CPU that recorded the event"
    return unpacked_record[1]

def event_pid(unpacked_record):
    "Given a tuple from format.unpack(), return the process ID of the event"
    return unpacked_record[2]

def event_job_id(unpacked_record):
    "Given a tuple from format.unpack(), return the job ID of the event"
    return unpacked_record[3]

def event_name(unpacked_record):
    "Given a tuple from format.unpack(), return the name of the event"
    # first element of tuple from format.unpack() is event type
    return event(unpacked_record[0])

def event_time(unpacked_record):
    "Given a tuple from format.unpack(), return the time of the event"
    if unpacked_record[0] in EVENTS_WITHOUT_TIMESTAMP:
        return 0
    else:
        return unpacked_record[4]

def lookup_ids(events):
    wanted = set()
    for e in events:
        if type(e) == int:
            # directly add numeric IDs
            wanted.add(e)
        else:
            # translate strings
            wanted.add(EVENTS[e])
    return wanted


def cached(trace_attr):
    cached_val = []
    def check_cache(*args, **kargs):
        if not cached_val:
            cached_val.append(trace_attr(*args, **kargs))
        return cached_val[0]
    return check_cache

class SchedTrace(object):
    def __init__(self, trace_files):
        self.traces = [SchedTraceFile(f) for f in trace_files]

        self._names = None
        self._wcets = None
        self._periods = None
        self._phases = None
        self._partitions = None
        self._sys_rels = None

    def __len__(self):
        return sum((len(t) for t in self.traces))

    def __iter__(self):
        for t in self.traces:
            for rec in t:
                yield rec

    def events_of_type(self, *events):
        wanted = lookup_ids(events)
        for t in self.traces:
            for rec in t.events_of_type(wanted):
                yield rec

    def events_of_type_chrono(self, *events):
        wanted = lookup_ids(events)

        def by_event_time(rec):
            return (event_time(rec), rec)

        trace_iters = [imap(by_event_time, t.events_of_type(wanted))
                       for t in self.traces]

        for (when, rec) in heapq.merge(*trace_iters):
            yield rec

    def events_in_range_of_type(self, start=0, end=0, *events, **kargs):
        if 'sorted' in kargs and kargs['sorted']:
            all = self.events_of_type_chrono(*events)
        else:
            all = self.events_of_type(*events)

        for rec in all:
            if start <= event_time(rec) <= end:
                yield rec

    def active_in_interval(self, start=0, end=0):
        tasks = set()
        cores = set()
        for rec in self.events_of_type('ST_SWITCH_TO', 'ST_SWITCH_AWAY', 'ST_RELEASE'):
            if start <= event_time(rec) <= end:
                tasks.add(event_pid(rec))
                cores.add(event_cpu(rec))
        return tasks, cores

    def scheduling_intervals(self):
        for t in self.traces:
            for interval in t.scheduling_intervals():
                yield interval

    def scheduling_intervals_in_range(self, start=0, end=0):
        for t in self.traces:
            for (to, away) in t.scheduling_intervals():
                if not (end < event_time(to) or event_time(away) < start):
                    yield (to, away)

    def identify_tasks(self):
        self._names      = defaultdict(str)
        self._wcets      = defaultdict(int)
        self._periods    = defaultdict(int)
        self._phases     = defaultdict(int)
        self._partitions = defaultdict(int)
        param_id = EVENTS['ST_PARAM']
        name_id = EVENTS['ST_NAME']
        for rec in self.events_of_type(param_id, name_id):
            pid = rec[2]
            if rec[0] == param_id:
                wcet, period, phase, partition = rec[-4:]
                self._wcets[pid]   = wcet
                self._periods[pid] = period
                self._phases[pid]  = phase
                self._partitions[pid] = partition
            elif rec[0] == name_id:
                self._names[pid] = rec[-1]

    @property
    def task_wcets(self):
        if self._wcets is None:
            self.identify_tasks()
        return self._wcets

    @property
    def task_periods(self):
        if self._periods is None:
            self.identify_tasks()
        return self._periods

    @property
    def task_phases(self):
        if self._phases is None:
            self.identify_tasks()
        return self._phases

    @property
    def task_partitions(self):
        if self._partitions is None:
            self.identify_tasks()
        return self._partitions

    @property
    def task_names(self):
        if self._names is None:
            self.identify_tasks()
        return self._names

    @property
    @cached
    def system_releases(self):
        return [rec[-1] for rec in self.events_of_type('ST_SYS_RELEASE')]

    @property
    @cached
    def earliest_event_time(self):
        earliest = None
        for t in self.traces:
            for rec in t:
                if event_time(rec) > 0:
                    if earliest is None or event_time(rec) < earliest:
                        earliest = event_time(rec)
                    break
        return earliest

    @property
    @cached
    def latest_event_time(self):
        latest = None
        for t in self.traces:
            for rec in reversed(t):
                if event_time(rec) > 0:
                    if latest is None or event_time(rec) > latest:
                        latest = event_time(rec)
                    break
        return latest
