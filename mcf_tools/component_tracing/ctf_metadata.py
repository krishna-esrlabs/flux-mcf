"""
CTF metadata file contents

Copyright (c) 2024 Accenture
"""

CTF_METADATA = ('''/* CTF 1.8 */

/*
 * This file contains the metadata that will become part of an MCF trace
 * after conversion to Common Trace Format.
 *
 * It describes the storage format of the corresponding stream file(s).
 */
 
typealias integer { size = 32; align = 8; signed = true; } := int32_t;

typealias integer { size = 8; align = 8; signed = false; } := uint8_t;
typealias integer { size = 16; align = 8; signed = false; } := uint16_t;
typealias integer { size = 32; align = 8; signed = false; } := uint32_t;
typealias integer { size = 64; align = 8; signed = false; } := uint64_t;

typealias floating_point {
    exp_dig = 8;         /* sizeof(float) * CHAR_BIT - FLT_MANT_DIG */
    mant_dig = 24;       /* FLT_MANT_DIG */
    align = 8;
} := float;

clock {
    name = my_clock;
    freq = 1000000;
};

typealias integer {
    size = 64;
    map = clock.my_clock.value;
    align = 8;
} := tstamp_us_t;

struct port_desc {
    string name;
    string topic;
    uint8_t connected;
} align(8);

struct trigger_desc {
    string topic;
    tstamp_us_t trigger_time;
} align(8);


trace {
    major = 1;
    minor = 8;
    byte_order = le;
    packet.header := struct {
        uint32_t magic;
        uint32_t stream_id;
    };
};

stream {
    id = 0;
    packet.context := struct {
        tstamp_us_t timestamp_begin;
        tstamp_us_t timestamp_end;
    };
    event.header := struct {
        uint16_t id;
        tstamp_us_t timestamp;
    };
};

event {
    id = 10;
    name = "port_write";
    stream_id = 0;
    fields := struct {
        string trace_id;
        string component;
        struct port_desc port;
        uint64_t value_id;
        int32_t thread_id;
        int32_t cpu_id;
    };
};

event {
    id = 20;
    name = "port_read";
    stream_id = 0;
    fields := struct {
        string trace_id;
        string component;
        struct port_desc port;
        uint64_t value_id;
        int32_t thread_id;
        int32_t cpu_id;
    };
};

event {
    id = 25;
    name = "port_peek";
    stream_id = 0;
    fields := struct {
        string trace_id;
        string component;
        struct port_desc port;
        uint64_t value_id;
        int32_t thread_id;
        int32_t cpu_id;
    };
};

event {
    id = 30;
    name = "exec_start";
    stream_id = 0;
    fields := struct {
        string trace_id;
        string component;
        string description;
        float exec_time;
        int32_t thread_id;
        int32_t cpu_id;
    };
};

event {
    id = 35;
    name = "exec_end";
    stream_id = 0;
    fields := struct {
        string trace_id;
        string component;
        string description;
        float exec_time;
        int32_t thread_id;
        int32_t cpu_id;
    };
};

event {
    id = 40;
    name = "port_trigger_act";
    stream_id = 0;
    fields := struct {
        string trace_id;
        string component;
        struct trigger_desc trigger;
        int32_t thread_id;
        int32_t cpu_id;
    };
};

event {
    id = 50;
    name = "port_handler_start";
    stream_id = 0;
    fields := struct {
        string trace_id;
        string component;
        struct trigger_desc trigger;
        float exec_time;
        int32_t thread_id;
        int32_t cpu_id;
    };
};

event {
    id = 55;
    name = "port_handler_end";
    stream_id = 0;
    fields := struct {
        string trace_id;
        string component;
        struct trigger_desc trigger;
        float exec_time;
        int32_t thread_id;
        int32_t cpu_id;
    };
};


event {
    id = 60;
    name = "remote_transfer_start";
    stream_id = 0;
    fields := struct {
        string trace_id;
        string component;
        string description;
        float exec_time;
        int32_t thread_id;
        int32_t cpu_id;
    };
};

event {
    id = 65;
    name = "remote_transfer_end";
    stream_id = 0;
    fields := struct {
        string trace_id;
        string component;
        string description;
        float exec_time;
        int32_t thread_id;
        int32_t cpu_id;
    };
};

event {
    id = 70;
    name = "time_box_start";
    stream_id = 0;
    fields := struct {
        string trace_id;
        string box_name;
        uint32_t box_id;
        
        /* 
         * completion_status: Should take the value of ['SAFE', 'TIME_VIOLATION' OR 'LOST']
         * completion_status_id: is the index of the completion_status value in the above list 
         */
        uint32_t completion_status_id;
        string completion_status;
    };
};


event {
    id = 75;
    name = "time_box_end";
    stream_id = 0;
    fields := struct {
        string trace_id;
        string box_name;
        uint32_t box_id;
        
        /* 
         * completion_status: Should take the value of ['SAFE', 'TIME_VIOLATION' OR 'LOST']
         * completion_status_id: is the index of the completion_status value in the above list 
         */
        uint32_t completion_status_id;
        string completion_status;
    };
};

event {
    id = 80;
    name = "program_flow";
    stream_id = 0;
    fields := struct {
        string trace_id;
        string component;
        string eventName;
        int32_t thread_id;
        int32_t cpu_id;
    };
};

''')