from __future__ import division

import sys
import cairo

from math import ceil

from .format import EVENTS

from . import event_time, event_pid, event_cpu, event_job_id

ARROW_LINE_WIDTH = 2 # (pts)
ARROW_WIDTH      = 12 # (pts)
ARROW_HEIGHT     = 0.8 # (relative)
ALLOC_HEIGHT     = 0.5 # (relative)

COLORS = [
    (0.0000, 0.1667, 1.0000),
    (1.0000, 0.0833, 0.0000),
    (0.0417, 1.0000, 0.0000),
    (0.9583, 0.0000, 1.0000),
    (0.9792, 1.0000, 0.0000),
    (0.0000, 1.0000, 0.8958),
    (0.3958, 0.0000, 1.0000),
    (1.0000, 0.0000, 0.4792),
    (0.6042, 1.0000, 0.0000),
    (0.0000, 0.7292, 1.0000),
    (0.0000, 1.0000, 0.3333),
    (1.0000, 0.4583, 0.0000),
    (0.0208, 0.0000, 1.0000),
    (1.0000, 0.8333, 0.0000),
    (0.2083, 0.0000, 1.0000),
    (0.7917, 1.0000, 0.0000),
    (0.5833, 0.0000, 1.0000),
    (0.4167, 1.0000, 0.0000),
    (0.7708, 0.0000, 1.0000),
    (0.2292, 1.0000, 0.0000),
    (1.0000, 0.0000, 0.8542),
    (0.0000, 1.0000, 0.1458),
    (1.0000, 0.0000, 0.6667),
    (0.0000, 1.0000, 0.5208),
    (1.0000, 0.0000, 0.2917),
    (0.0000, 1.0000, 0.7083),
    (1.0000, 0.0000, 0.1042),
    (0.0000, 0.9167, 1.0000),
    (1.0000, 0.2708, 0.0000),
    (0.0000, 0.5417, 1.0000),
    (1.0000, 0.6458, 0.0000),
    (0.0000, 0.3542, 1.0000)
]

def cpu_color(cpu):
    return COLORS[cpu % len(COLORS)]

MAJOR_TICK_FONT_SIZE = 24

TASK_LABEL_FONT_SIZE = 18
TASK_LABEL_COLOR = (0, 0, 0)

TAG_FONT_SIZE = 12
TAG_COLOR = (0, 0, 0)

JOB_ID_FONT_SIZE = 10

GRID_WIDTH       = 2
MAJOR_TICK_WIDTH = 2
MINOR_TICK_WIDTH = 1

MAJOR_TICK_COLOR = (0.7, 0.7, 0.7)
MINOR_TICK_COLOR = (0.8, 0.8, 0.8)

GRID_COLOR = (0.7, 0.7, 0.7)

JOB_EVENT_COLOR = (0, 0, 0)

YRES = 100



XLABEL_MARGIN = 72
YLABEL_MARGIN = 200

def center_text(c, x, y, msg):
    x_bear, y_bear, width, height, x_adv, y_adv = c.text_extents(msg)
    c.move_to(x, y)
    c.rel_move_to(-width / 2, height)
    c.show_text(msg)

def center_text_above(c, x, y, msg):
    x_bear, y_bear, width, height, x_adv, y_adv = c.text_extents(msg)
    c.move_to(x, y)
    c.rel_move_to(-width / 2, 0)
    c.show_text(msg)

def text_left_align_below(c, x, y, msg):
    x_bear, y_bear, width, height, x_adv, y_adv = c.text_extents(msg)
    c.move_to(x, y)
    c.rel_move_to(0, height)
    c.show_text(msg)

def text_left_align_above(c, x, y, msg):
    c.move_to(x, y)
    c.show_text(msg)

def vcenter_right_align_text(c, x, y, msg):
    x_bear, y_bear, width, height, x_adv, y_adv = c.text_extents(msg)
    c.move_to(x, y)
    c.rel_move_to(-width, height/2)
    c.show_text(msg)
    return y_adv

def render(opts, trace):
    if opts.verbose:
        print '[II] Identifying relevant tasks and CPUs...',
        sys.stdout.flush()
    tasks, cores = trace.active_in_interval(opts.start, opts.end)
    if opts.verbose:
        print '%d tasks, %d cores.' % (len(tasks), len(cores))

    # scale is given in points/ms, we need points/ns
    xscale = opts.xscale/1E6
    yscale = opts.yscale/YRES
    width  = ceil(opts.margin * 2 + (opts.end - opts.start) * xscale + YLABEL_MARGIN)

    height = ceil(opts.margin * 2 + (len(tasks) * YRES)  * yscale + XLABEL_MARGIN)

    if opts.verbose:
        print '[II] Canvas size: %dpt x %dpt' % (width, height)

    pdf = cairo.PDFSurface(opts.output, width, height)

    task_idx = {}
    for i, t in enumerate(sorted(tasks)):
        task_idx[t] = i


    c = cairo.Context(pdf)
    c.translate(opts.margin + YLABEL_MARGIN, opts.margin)

    # draw box
#     c.rectangle(0, 0, width - opts.margin * 2, height - opts.margin * 2)
#     c.stroke()

    def xpos(x):
        return (x - opts.start) * xscale

    def ypos(y):
        return (y * YRES) * yscale

#    c.scale(xscale, yscale)

    if opts.verbose:
        print '[II] Drawing grid...',
        sys.stdout.flush()

    # draw minor tick lines
    if opts.minor_ticks:
        c.set_source_rgb(*MINOR_TICK_COLOR)
        c.set_line_width(MINOR_TICK_WIDTH)
        time = opts.start
        while time <= opts.end:
            x = xpos(time)
            y = ypos(len(tasks))
            c.new_path()
            c.move_to(x, y)
            c.line_to(x, 0)
            c.stroke()
            time += opts.minor_ticks * 1E6


    # draw major tick lines
    if opts.major_ticks:
        c.set_source_rgb(*MAJOR_TICK_COLOR)
        c.set_line_width(MAJOR_TICK_WIDTH)
        c.set_font_size(MAJOR_TICK_FONT_SIZE)
        time = opts.start
        while time <= opts.end:
            x = xpos(time)
            y = ypos(len(tasks) + 0.2)
            c.new_path()
            c.move_to(x, y)
            c.line_to(x, 0)
            c.stroke()
            y = ypos(len(tasks) + 0.3)
            center_text(c, x, y, "%dms" % ((time - opts.start) / 1E6))
            time += opts.major_ticks * 1E6

    # draw task labels
    c.set_font_size(TASK_LABEL_FONT_SIZE)
    c.set_source_rgb(*TASK_LABEL_COLOR)
    for pid in tasks:
        x = -24
        y = ypos(task_idx[pid] + 0.25)
        vcenter_right_align_text(c, x, y, "%s/%d" % (trace.task_names[pid], pid))
        y = ypos(task_idx[pid] + 0.75)
        vcenter_right_align_text(c, x, y,
            "(%.2fms, %.2fms)" % (trace.task_wcets[pid] / 1E6,
                                  trace.task_periods[pid] / 1E6))

    if opts.verbose:
        print 'done.'
        print '[II] Drawing CPU allocations...',
        sys.stdout.flush()


    # raw allocations
    box_height = ALLOC_HEIGHT * YRES * yscale
    c.set_font_size(TAG_FONT_SIZE)
    for (to, away) in trace.scheduling_intervals_in_range(opts.start, opts.end):
        delta = (event_time(away) - event_time(to)) * xscale
        pid = event_pid(to)
        y = ypos(task_idx[pid] + (1 - ALLOC_HEIGHT))
        x = xpos(event_time(to))
        c.new_path()
        c.rectangle(x, y, delta, box_height)
        c.set_source_rgb(*cpu_color(event_cpu(to)))
        c.fill()

        c.set_source_rgb(*TAG_COLOR)
        text_left_align_below(c, x + 2, y + 2, '%d' % event_cpu(to))

    # draw task base lines
    c.set_source_rgb(*GRID_COLOR)
    c.set_line_width(GRID_WIDTH)
    for t in tasks:
        c.new_path()
        y = ypos(task_idx[t] + 1)
        x = xpos(opts.start)
        c.move_to(x, y)
        x = xpos(opts.end)
        c.line_to(x, y)
        c.stroke()

    if opts.verbose:
        print 'done.'
        print '[II] Drawing releases and deadlines...',
        sys.stdout.flush()

    # draw releases and deadlines
    c.set_source_rgb(*JOB_EVENT_COLOR)
    c.set_line_width(ARROW_LINE_WIDTH)
#    c.set_line_cap(cairo.LINE_JOIN_ROUND)
    c.set_line_join(cairo.LINE_JOIN_ROUND)
    c.set_font_size(JOB_ID_FONT_SIZE)
    arrow_width  = ARROW_WIDTH / 2
    arrow_height = ARROW_HEIGHT * YRES * yscale
    for rec in trace.events_in_range_of_type(opts.start, opts.end, 'ST_RELEASE'):
        pid = event_pid(rec)
        y   = ypos(task_idx[pid] + 1)

        rel = xpos(event_time(rec))
        c.new_path()
        c.move_to(rel, y - (GRID_WIDTH - 1))
        c.rel_line_to(0, -arrow_height)
        c.rel_line_to(-arrow_width, arrow_width)
        c.rel_move_to(arrow_width, -arrow_width)
        c.rel_line_to(arrow_width, arrow_width)
        c.stroke()

        if opts.show_job_ids:
            text_left_align_above(c, rel + 4, y - arrow_height - 4,
                            "%d" % event_job_id(rec))

        dl  = xpos(rec[-1])
        c.new_path()
        c.move_to(dl, y - arrow_height - (GRID_WIDTH - 1))
        c.rel_line_to(0, arrow_height)
        c.rel_line_to(-arrow_width, -arrow_width)
        c.rel_move_to(arrow_width, arrow_width)
        c.rel_line_to(arrow_width, -arrow_width)
        c.stroke()


    if opts.verbose:
        print 'done.'
        print '[II] Drawing job completions...',
        sys.stdout.flush()

    # draw job completions
    for rec in trace.events_in_range_of_type(opts.start, opts.end, 'ST_COMPLETION'):
        pid = event_pid(rec)
        y   = ypos(task_idx[pid] + 1)
        x   = xpos(event_time(rec))
        c.new_path()
        c.move_to(x, y - (GRID_WIDTH - 1))
        c.rel_line_to(0, -arrow_height)
        c.rel_move_to(-arrow_width, 0)
        c.rel_line_to(2 * arrow_width, 0)
        c.stroke()

        if opts.show_job_ids:
            text_left_align_above(c, x + 4, y - arrow_height - 4,
                            "%d" % event_job_id(rec))

    if opts.verbose:
        print 'done.'
        print '[II] Drawing job suspensions...',
        sys.stdout.flush()

    # draw job suspensions
    c.set_line_width(1)
    for rec in trace.events_in_range_of_type(opts.start, opts.end, 'ST_BLOCK'):
        pid = event_pid(rec)
        y  = ypos(task_idx[pid] + (1 - ALLOC_HEIGHT) + 0.5 * ALLOC_HEIGHT)
        y1 = ypos(task_idx[pid] + (1 - ALLOC_HEIGHT) + 0.4 * ALLOC_HEIGHT)
        y2 = ypos(task_idx[pid] + (1 - ALLOC_HEIGHT) + 0.6 * ALLOC_HEIGHT)
        x   = xpos(event_time(rec))
        c.new_path()
        c.move_to(x, y1)
        c.line_to(x, y2)
        c.line_to(x - arrow_width, y)
        c.line_to(x, y1)
        c.close_path()
        c.stroke()


    if opts.verbose:
        print 'done.'
        print '[II] Drawing job wake-ups...',
        sys.stdout.flush()

    # draw job suspensions
    c.set_line_width(1)
    for rec in trace.events_in_range_of_type(opts.start, opts.end, 'ST_RESUME'):
        pid = event_pid(rec)
        y  = ypos(task_idx[pid] + (1 - ALLOC_HEIGHT) + 0.5 * ALLOC_HEIGHT)
        y1 = ypos(task_idx[pid] + (1 - ALLOC_HEIGHT) + 0.4 * ALLOC_HEIGHT)
        y2 = ypos(task_idx[pid] + (1 - ALLOC_HEIGHT) + 0.6 * ALLOC_HEIGHT)
        x   = xpos(event_time(rec))
        c.new_path()
        c.move_to(x, y1)
        c.line_to(x, y2)
        c.line_to(x + arrow_width, y)
        c.line_to(x, y1)
        c.close_path()
        c.stroke()

    if opts.verbose:
        print 'done.'
        print '[II] Finishing PDF...',
        sys.stdout.flush()

    pdf.finish()

    if opts.verbose:
        print 'done.'

    if opts.verbose:
        print '[II] Flushing PDF...',
        sys.stdout.flush()

    pdf.flush()

    if opts.verbose:
        print 'done.'

    del pdf
