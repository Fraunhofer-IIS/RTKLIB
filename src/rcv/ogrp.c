#include "rtklib.h"
#include <json/json.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

const static double gpst0[]={1980,1, 6,0,0,0};

static int get_value_check_type(json_object *jobj, const char* key, json_object **value, json_type type) {
    trace(5,"get_value_check_type:\n");

    if (json_object_object_get_ex(jobj, key, value) == 0) {
        trace(2, "get_value_check_type: Key error, no %s\n", key);
        return -1;
    }
    if (json_object_get_type(*value) != type) {
        trace(2, "get_value_check_type: Value type error, key %s\n", key);
        return -1;
    }
    return 0;
}

static int get_system_and_signal(const char *gnss_str, const char *sig_str, int *sys, int *code, int *freq) {
    /* signal frequency:  0:L1, 1:L2, 2:L5, 3:L6, 4:L7, 5:L8 */
    if (strcmp(gnss_str, "GPS") == 0) {
        *sys = SYS_GPS;
        if (strcmp(sig_str, "L1CA") == 0) {
            *code = CODE_L1C;
            *freq = 0;
            return 0;
        }
        if (strcmp(sig_str, "L2C") == 0) {
            *code = CODE_L2C;
            *freq = 1;
            return 0;
        }
        if (strcmp(sig_str, "L2CM") == 0){
            *code = CODE_L2S;
            *freq = 1;
            return 0;
        }
        if (strcmp(sig_str, "L5I") == 0) {
            *code = CODE_L5I;
            *freq = 2;
            return 0;
        }
        if (strcmp(sig_str, "L5Q") == 0) {
            *code = CODE_L5Q;
            *freq = 2;
            return 0;
        }
        trace(2, "Unsupported GPS signal %s\n", sig_str);
        return -1;
    }
    if (strcmp(gnss_str, "Galileo") == 0) {
        *sys = SYS_GAL;
        if ((strcmp(sig_str, "E1B") == 0) || strcmp(sig_str, "E1C") == 0) {
            *code = CODE_L1C;
            *freq = 0;
            return 0;
        }
        if ((strcmp(sig_str, "E5a") == 0) || (strcmp(sig_str, "E5aI") == 0)) {
            *code = CODE_L5I;
            *freq = 2;
            return 0;
        }
        if (strcmp(sig_str, "E5aQ") == 0) {
            *code = CODE_L5Q;
            *freq = 2;
            return 0;
        }
        if ((strcmp(sig_str, "E5b") == 0) || (strcmp(sig_str, "E5bI") == 0)) {
            *code = CODE_L7I;
            *freq = 4;
            return 0;
        }
        if (strcmp(sig_str, "E5bQ") == 0) {
            *code = CODE_L7Q;
            *freq = 4;
            return 0;
        }
        trace(2, "Unsupported Galileo signal %s\n", sig_str);
        return -1;
    }
    trace(2, "Unsupported GNSS system %s\n", gnss_str);
    return -1;
}

static int obsindex(obs_t *obs, gtime_t time, int sat) {
    int i,j;

    if (obs->n >= MAXOBS) return -1;
    for (i = 0; i < obs->n; i++) {
        if (obs->data[i].sat == sat) return i;
    }
    obs->data[i].time = time;
    obs->data[i].sat = sat;
    for (j = 0; j < NFREQ + NEXOBS; j++) {
        obs->data[i].L[j] = obs->data[i].P[j] = 0.0;
        obs->data[i].D[j] = 0.0;
        obs->data[i].SNR[j] = obs->data[i].LLI[j] = 0;
        obs->data[i].code[j] = CODE_NONE;
    }
    obs->n++;
    return i;
}

static int checkpri(int freq) {
    /* TODO Consider freq >= NFREQ (e.g. E5bI) */
    return freq < NFREQ ? freq : -1;
}

int json_get_number(json_object *jobj, const char *key, double *number) {
    json_object *jnumber;
    if (get_value_check_type(jobj, key, &jnumber, json_type_double) < 0) {
        if (get_value_check_type(jobj, key, &jnumber, json_type_int) < 0) return -1;
        *number = (double)json_object_get_int(jnumber);
    } else {
        *number = json_object_get_double(jnumber);
    }
    return 0;
}

static int decode_ogrp_ch_meas(raw_t *raw, json_object *jobj) {
    double pseudorange, doppler, carrier_phase, snr, locktime, sat_id;
    int sat;
    json_object *jgnss, *jsignal_type, *jchannel_state;
    int freq_nr, obs_nr;
    double tt = timediff(raw->time,raw->tobs);
    int lli;
    const char *gnss, *signal_type, *channel_state;
    int sys, code, freq;

    trace(5,"decode_ogrp_ch_meas:\n");

    if (json_object_get_type(jobj) != json_type_object) {
        trace(2, "decode_ogrp_ch_meas: Type error, no object\n");
        return -1;
    }

    if (get_value_check_type(jobj, "channel_state", &jchannel_state, json_type_string) < 0) return -1;
    channel_state = json_object_get_string(jchannel_state);
    if (strcmp(channel_state, "SYNCED") != 0) {
        trace(2, "Channel not synchronized. Skip channel measurement.\n");
        return -1;
    }

    /* Check supported GNSS and signal type */
    if (get_value_check_type(jobj, "gnss", &jgnss, json_type_string) < 0) return -1;
    gnss = json_object_get_string(jgnss);

    if (get_value_check_type(jobj, "signal_type", &jsignal_type, json_type_string) < 0) return -1;
    signal_type = json_object_get_string(jsignal_type);

    if (get_system_and_signal(gnss, signal_type, &sys, &code, &freq) != 0) return -1;

    freq_nr = checkpri(freq);
    if (freq_nr == -1) {
        trace(2, "Discard channel measurement. Signal priority is too low for %s %s\n", gnss, signal_type);
    }

    if (json_get_number(jobj, "pseudo_range",  &pseudorange)   != 0) return -1;
    if (json_get_number(jobj, "doppler",       &doppler)       != 0) return -1;
    if (json_get_number(jobj, "carrier_phase", &carrier_phase) != 0) return -1;
    if (json_get_number(jobj, "snr",           &snr)           != 0) return -1;
    if (json_get_number(jobj, "locktime",      &locktime)      != 0) return -1;

    if (json_get_number(jobj, "satellite_id", &sat_id) != 0) return -1;
    if (!(sat = satno(sys, (int)sat_id))) {
        trace(3,"satellite number error: sys=%d, sat_id=%d\n", sys, (int)sat_id);
        return -1;
    }

    if (raw->tobs.time != 0) lli = locktime - raw->lockt[sat - 1][freq_nr] + 0.05 <= tt;
    else lli = 0;
    raw->lockt[sat - 1][freq_nr] = locktime;

    obs_nr = obsindex(&raw->obs, raw->time, sat);

    raw->obs.data[obs_nr].P[freq_nr]    = pseudorange;
    raw->obs.data[obs_nr].D[freq_nr]    = doppler;
    raw->obs.data[obs_nr].L[freq_nr]    = -carrier_phase;
    raw->obs.data[obs_nr].SNR[freq_nr]  = snr * 4.0 + 0.5;
    raw->obs.data[obs_nr].LLI[freq_nr]  = lli;
    raw->obs.data[obs_nr].code[freq_nr] = code;

    raw->obs.data[obs_nr].time = raw->time;
    raw->obs.data[obs_nr].sat  = sat;

    return 1;
}

static int decode_ogrp_ephemeris_element(raw_t *raw, json_object *jobj) {
    eph_t eph = {0};

    json_object *jgnss;
    const char *gnss;
    int sat, sys;
    double sqrtA, tow, sat_id, toc, week, value;

    if (json_object_get_type(jobj) != json_type_object) {
        trace(2, "decode_ogrp_ephemeris_element: Type error, no object\n");
        return -1;
    }

    if (get_value_check_type(jobj, "gnss", &jgnss, json_type_string) < 0) return -1;
    gnss = json_object_get_string(jgnss);

    if (strcmp(gnss, "GPS") == 0) {
        sys = SYS_GPS;
    } else if (strcmp(gnss, "Galileo") == 0) {
        sys = SYS_GAL;
        eph.code = 0; /* INAV=0, FNAV=1 */
    } else {
        trace(2, "Unsupported GNSS system %s\n", gnss);
        return -1;
    }

    if (json_get_number(jobj, "satellite_id", &sat_id)   != 0) return -1;
    if (!(sat = satno(sys, (int)sat_id))) {
        trace(3,"satellite number error: sys=%d, sat_id=%d\n", sys, (int)sat_id);
        return -1;
    }

    if (json_get_number(jobj, "ephemeris_issue", &value) != 0) return -1;
    eph.iode = (int)value;
    if (eph.iode == raw->nav.eph[sat - 1].iode) return -1;

    if (json_get_number(jobj, "clock_issue", &value) != 0) return -1;
    eph.iodc = (int)value;
    if (eph.iodc == raw->nav.eph[sat - 1].iodc) return -1;

    if (json_get_number(jobj, "root_a", &sqrtA) != 0) return -1;
    eph.A = sqrtA * sqrtA;

    if (json_get_number(jobj, "ephemeris_reference", &eph.toes) != 0) return -1;
    if (json_get_number(jobj, "clock_reference",     &toc)      != 0) return -1;
    if (json_get_number(jobj, "week_number",         &week)     != 0) return -1;

    eph.week = adjgpsweek((int)week);
    tow = time2gpst(raw->time, (int*)&week);
    eph.ttr=gpst2time(eph.week,tow);
    eph.toc=gpst2time(eph.week,(int)toc);
    tow=time2gpst(eph.ttr,&eph.week);
    toc=time2gpst(eph.toc,NULL);
    if      (eph.toes<tow-302400.0) {eph.week++; tow-=604800.0;}
    else if (eph.toes>tow+302400.0) {eph.week--; tow+=604800.0;}
    eph.toe=gpst2time(eph.week,eph.toes);
    eph.toc=gpst2time(eph.week,(int)toc);
    eph.ttr=gpst2time(eph.week,tow);

    if (json_get_number(jobj, "ecc",         &eph.e)      != 0) return -1;
    if (json_get_number(jobj, "af_0",        &eph.f0)     != 0) return -1;
    if (json_get_number(jobj, "af_1",        &eph.f1)     != 0) return -1;
    if (json_get_number(jobj, "af_2",        &eph.f2)     != 0) return -1;
    if (json_get_number(jobj, "m_0",         &eph.M0)     != 0) return -1;
    if (json_get_number(jobj, "omega_0",     &eph.OMG0)   != 0) return -1;
    if (json_get_number(jobj, "omega_dot",   &eph.OMGd)   != 0) return -1;
    if (json_get_number(jobj, "crc",         &eph.crc)    != 0) return -1;
    if (json_get_number(jobj, "crs",         &eph.crs)    != 0) return -1;
    if (json_get_number(jobj, "cuc",         &eph.cuc)    != 0) return -1;
    if (json_get_number(jobj, "cus",         &eph.cus)    != 0) return -1;
    if (json_get_number(jobj, "cic",         &eph.cic)    != 0) return -1;
    if (json_get_number(jobj, "cis",         &eph.cis)    != 0) return -1;
    if (json_get_number(jobj, "deln",        &eph.deln)   != 0) return -1;
    if (json_get_number(jobj, "inc_0",       &eph.i0)     != 0) return -1;
    if (json_get_number(jobj, "inc_dot",     &eph.idot)   != 0) return -1;
    if (json_get_number(jobj, "group_delay", &eph.tgd[0]) != 0) return -1;
    if (json_get_number(jobj, "arg_per",     &eph.omg)    != 0) return -1;

    /* Missing parameter eph.tgd[1] --> BGD: E5B-E1 (s) */

    eph.sat = sat;
    raw->nav.eph[sat - 1] = eph;
    raw->ephsat = sat;

    return 2;
}

static int decode_ogrp_channel_measurements(raw_t *raw, json_object *jobj) {
    json_object *ch_meas_array;
    int i, num_ch_meas, status;

    trace(5,"decode_ogrp_channel_measurements:\n");

    status = get_value_check_type(jobj, "ch_meas", &ch_meas_array, json_type_array);
    if (status < 0) {
        status = get_value_check_type(jobj, "channel_measurements", &ch_meas_array, json_type_array);
    }
    if (status < 0) num_ch_meas = 0;
    else num_ch_meas = json_object_array_length(ch_meas_array);

    raw->obs.n = 0;
    for (i = 0; i < num_ch_meas; i++) {
        json_object *ch_meas = json_object_array_get_idx(ch_meas_array, i);
        if (decode_ogrp_ch_meas(raw, ch_meas) != 1) {
            continue;
        }
        if (raw->obs.n >= MAXOBS) return -1;
    }

    raw->tobs=raw->time;

    return 1;
}

static int decode_ogrp_ephemeris(raw_t *raw, json_object *jobj) {
    json_object *ephem;

    if (raw->obs.n == 0) return -1;

    trace(5,"decode_ogrp_ephemeris:\n");

    if (get_value_check_type(jobj, "ephemeris", &ephem, json_type_object) < 0) return -1;

    return decode_ogrp_ephemeris_element(raw, ephem);
}

static int decode_ogrp_timestamp(raw_t *raw, json_object *jobj) {
    json_object* timestamp;
    json_type type;
    double utc_offset, t;

    trace(5,"decode_ogrp_timestamp:\n");

    if (json_object_object_get_ex(jobj, "timestamp", &timestamp) == 0) {
        trace(2, "decode_ogrp_timestamp: Key error, no timestamp\n");
        return -1;
    }

    /* Time between GPS start (05.01.1980) and 01.01.1970
     * 432000.0 -> 5 days; 315532800.0 -> 10 years */
    utc_offset = 432000.0 + 315532800.0;
    raw->time = epoch2time(gpst0);
    type = json_object_get_type(timestamp);
    switch (type) {
    case json_type_int:
        t = json_object_get_int(timestamp);
        if (t < 0.0) return -1;
        raw->time.time += (t - (int)utc_offset);
        break;
    case json_type_double:
        t = json_object_get_double(timestamp);
        if (t < 0.0) return -1;
        raw->time.time += (t - utc_offset);
        raw->time.sec = json_object_get_double(timestamp) - (int)json_object_get_double(timestamp);
        break;
    case json_type_string:
        /* TODO Implement unix time string */
        assert(0 && "decode_ogrp_timestamp: Unix time string format is not supported yet");
        return -1;
    default:
        trace(2, "decode_ogrp_timestamp: Value type error, key timestamp\n");
        return -1;
    }

    return 1;
}

static int decode_ogrp_msg(raw_t *raw) {
    json_object *jobj, *id_value;
    json_type type;
    const char *id_value_str;

    trace(5,"decode_ogrp_msg:\n");

    jobj = json_tokener_parse((const char*)raw->buff);
    type = json_object_get_type(jobj);

    if (type != json_type_object) {
        trace(5, "decode_ogrp: Wrong json_type: %d\n", type);
        return -1;
    }

    if (get_value_check_type(jobj, "id", &id_value, json_type_string) < 0) return -1;

    if (decode_ogrp_timestamp(raw, jobj) != 1) return -1;

    id_value_str = json_object_get_string(id_value);
    if (strcmp(id_value_str, "channel_measurements") == 0) return decode_ogrp_channel_measurements(raw, jobj);
    if (strcmp(id_value_str, "ephemeris") == 0) return decode_ogrp_ephemeris(raw, jobj);
    /* TODO implement other message types */
    else
    {
        trace(2, "decode_ogrp: Message id not supported: %s\n", id_value_str);
        return -1;
    }
}

extern int input_ogrp(raw_t *raw, unsigned char data) {
    trace(5,"input_ogrp:\n");

    raw->buff[raw->nbyte++] = data;
    if (data != '\n') return 0;
    raw->len = raw->nbyte;
    raw->nbyte=0;

    return decode_ogrp_msg(raw);
}

extern int input_ogrpf(raw_t *raw, FILE *fp) {
    int data;

    trace(4,"input_ogrpf:\n");

    do {
        data = fgetc(fp);
        if (data == EOF) return -2;
        raw->buff[raw->nbyte++] = (unsigned char)data;
    } while (data != '\n');
    raw->len = raw->nbyte;
    raw->nbyte = 0;

    return decode_ogrp_msg(raw);
}
