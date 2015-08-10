#include "rtklib.h"
#include <json/json.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

const static double gpst0[]={1980,1, 6,0,0,0};

int get_value_check_type(json_object *jobj, char* key, json_object **value, json_type type) {
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

static int get_system_and_signal(char *gnss_str, char *sig_str, int *sys, int *code, int *freq) {
    /* signal frequency:  0:L1, 1:L2, 2:L5, 3:L6, 4:L7, 5:L8 */
    if (strcmp(gnss_str, "GPS") == 0) {
        *sys = SYS_GPS;
        if (strcmp(sig_str, "L1CA") == 0) {
            *code = CODE_L1C;
            *freq = 0;
            return 0;
        }
        if (strcmp(sig_str, "L5I") == 0) {
            *code = CODE_L5I;
            *freq = 2;
            return 0;
        }
        trace(2, "Unsupported GPS signal %s\n", sig_str);
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

int decode_ogrp_ch_meas(raw_t *raw, json_object *jobj) {
    double pseudorange, doppler, carrier_phase, snr, locktime;
    int sat_id;
    json_object *jpseudorange, *jdoppler, *jcarrier_phase, *jsnr, *jlocktime, *jsat_id, *jgnss, *jsignal_type;
    int freq_nr, obs_nr;
    double tt = timediff(raw->time,raw->tobs);
    int lli;
    char *gnss, *signal_type;
    int sys, code, freq;

    trace(5,"decode_ogrp_ch_meas:\n");

    if (json_object_get_type(jobj) != json_type_object) {
        trace(2, "decode_ogrp_ch_meas: Type error, no object\n");
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

    if (get_value_check_type(jobj, "pseudorange", &jpseudorange, json_type_double) < 0) return -1;
    pseudorange = json_object_get_double(jpseudorange);

    if (get_value_check_type(jobj, "doppler", &jdoppler, json_type_double) < 0) return -1;
    doppler = json_object_get_double(jdoppler);

    if (get_value_check_type(jobj, "carrier_phase", &jcarrier_phase, json_type_double) < 0) return -1;
    carrier_phase = json_object_get_double(jcarrier_phase);

    if (get_value_check_type(jobj, "snr", &jsnr, json_type_double) < 0) return -1;
    snr = json_object_get_double(jsnr);

    if (get_value_check_type(jobj, "sat_id", &jsat_id, json_type_int) < 0) return -1;
    sat_id = json_object_get_int(jsat_id);

    if (get_value_check_type(jobj, "locktime", &jlocktime, json_type_double) < 0) return -1;
    locktime = json_object_get_double(jlocktime);

    if (raw->tobs.time != 0) lli = locktime - raw->lockt[sat_id-1][freq_nr] + 0.05 <= tt;
    else lli = 0;
    raw->lockt[sat_id-1][freq_nr] = locktime;

    obs_nr = obsindex(&raw->obs, raw->time, sat_id);

    raw->obs.data[obs_nr].P[freq_nr]    = pseudorange;
    raw->obs.data[obs_nr].D[freq_nr]    = doppler;
    raw->obs.data[obs_nr].L[freq_nr]    = -carrier_phase;
    raw->obs.data[obs_nr].SNR[freq_nr]  = snr;
    raw->obs.data[obs_nr].LLI[freq_nr]  = lli;
    raw->obs.data[obs_nr].code[freq_nr] = code;

    raw->obs.data[obs_nr].time = raw->time;
    raw->obs.data[obs_nr].sat  = sat_id;

    return 1;
}

int decode_ogrp_measurement(raw_t *raw, json_object *jobj) {
    json_object *ch_meas_array;
    int i, num_ch_meas, status;

    trace(5,"decode_ogrp_measurement:\n");

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

int get_subframe(json_object *subframes, int num, unsigned char *sub, int sub_size) {
    json_object *jsub;
    char *pos;
    size_t count = 0;

    if (sub_size != 30) return -1;

    jsub = json_object_array_get_idx(subframes, num);
    if (json_object_get_type(jsub) != json_type_string) return -1;
    pos = json_object_get_string(jsub);
    if (strlen(pos) != sub_size * 2) return -1;

    for(count = 0; count < sub_size; count++) {
        sscanf(pos, "%2hhx", &sub[count]);
        pos += 2 * sizeof(char);
    }

    return 0;
}

int decode_ogrp_raw_ephemeris(raw_t *raw, json_object *jobj) {
    json_object *subframes, *jsat_id;
    int num_subframes, sat_id, sub_size = 30;
    unsigned char sub1[sub_size], sub2[sub_size], sub3[sub_size];
    eph_t eph={0};

    trace(5,"decode_ogrp_raw_ephemeris:\n");

    if (get_value_check_type(jobj, "subframe", &subframes, json_type_array) < 0) return -1;

    num_subframes = json_object_array_length(subframes);
    if (num_subframes != 3) return -1;

    if (get_value_check_type(jobj, "sat_id", &jsat_id, json_type_int) < 0) return -1;
    sat_id = json_object_get_int(jsat_id);

    if (get_subframe(subframes, 0, sub1, sub_size) < 0) return -1;
    if (get_subframe(subframes, 1, sub2, sub_size) < 0) return -1;
    if (get_subframe(subframes, 2, sub3, sub_size) < 0) return -1;

    if (decode_frame(sub1, &eph, NULL, NULL, NULL, NULL) != 1 ||
        decode_frame(sub2, &eph, NULL, NULL, NULL, NULL) != 2 ||
        decode_frame(sub3, &eph, NULL, NULL, NULL, NULL) != 3) {
        trace(2,"ogrp raw_ephemeris subframe error: sat ID: %d\n", sat_id);
        return -1;
    }

    eph.sat = sat_id;
    raw->nav.eph[sat_id - 1] = eph;
    raw->ephsat = sat_id;

    return 2;
}

int decode_ogrp_timestamp(raw_t *raw, json_object *jobj) {
    json_object* timestamp;
    json_type type;

    trace(5,"decode_ogrp_timestamp:\n");

    if (json_object_object_get_ex(jobj, "timestamp", &timestamp) == 0) {
        trace(2, "decode_ogrp_timestamp: Key error, no timestamp\n");
        return -1;
    }

    raw->time = epoch2time(gpst0);
    type = json_object_get_type(timestamp);
    switch (type) {
    case json_type_int:
        raw->time.time += json_object_get_int(timestamp);
        break;
    case json_type_double:
        raw->time.time += json_object_get_double(timestamp);
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

int decode_ogrp_msg(raw_t *raw) {
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
    if (strcmp(id_value_str, "measurement") == 0) return decode_ogrp_measurement(raw, jobj);
    if (strcmp(id_value_str, "raw_ephemeris") == 0) return decode_ogrp_raw_ephemeris(raw, jobj);
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
