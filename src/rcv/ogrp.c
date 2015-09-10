#include "rtklib.h"
#include <json/json.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

const static double gpst0[]={1980,1, 6,0,0,0};
const static unsigned subframe_bytes_without_parity = 30;
/* Number of bits is 2*(120-6)=228 */
const static unsigned inav_page_bytes_without_tail = 29;

const static double c_2_pow_m_5  = 0.03125;
const static double c_2_pow_m_19 = 1.9073486328125e-6;
const static double c_2_pow_m_29 = 1.86264514923096e-9;
const static double c_2_pow_m_31 = 4.65661287307739e-10;
const static double c_2_pow_m_32 = 2.3283064365386962890625e-10;
const static double c_2_pow_m_33 = 1.16415321826935e-10;
const static double c_2_pow_m_34 = 5.82076609134674072265625e-11;
const static double c_2_pow_m_43 = 1.13686837721616e-13;
const static double c_2_pow_m_46 = 1.4210854715202003717422485351563e-14;
const static double c_2_pow_m_59 = 1.7347234759768070944119244813919e-18;


/* Copy from src/rcv/novatel.c */
static gtime_t adjweek(gtime_t time, double tow) {
    double tow_p;
    int week;
    tow_p = time2gpst(time,&week);
    if      (tow < tow_p - 302400.0) tow += 604800.0;
    else if (tow > tow_p + 302400.0) tow -= 604800.0;
    return gpst2time(week, tow);
}

static int get_value_check_type(json_object *jobj, char* key, json_object **value, json_type type) {
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
    if (strcmp(gnss_str, "Galileo") == 0) {
        *sys = SYS_GAL;
        if (strcmp(sig_str, "E1B") == 0) {
            *code = CODE_L1B;
            *freq = 0;
            return 0;
        }
        if (strcmp(sig_str, "E1C") == 0) {
            *code = CODE_L1C;
            *freq = 0;
            return 0;
        }
        if (strcmp(sig_str, "E5aI") == 0) {
            *code = CODE_L5I;
            *freq = 2;
            return 0;
        }
        if (strcmp(sig_str, "E5aQ") == 0) {
            *code = CODE_L5Q;
            *freq = 2;
            return 0;
        }
        if (strcmp(sig_str, "E5bI") == 0) {
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

static int decode_ogrp_ch_meas(raw_t *raw, json_object *jobj) {
    double pseudorange, doppler, carrier_phase, snr, locktime;
    int sat_id, sat;
    json_object *jpseudorange, *jdoppler, *jcarrier_phase, *jsnr, *jlocktime, *jsat_id, *jgnss, *jsignal_type, *jchannel_state;
    int freq_nr, obs_nr;
    double tt = timediff(raw->time,raw->tobs);
    int lli;
    char *gnss, *signal_type, *channel_state;
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
    if (!(sat = satno(sys, sat_id))) {
        trace(3,"satellite number error: sys=%d, sat_id=%d\n", sys, sat_id);
        return -1;
    }

    if (get_value_check_type(jobj, "locktime", &jlocktime, json_type_double) < 0) return -1;
    locktime = json_object_get_double(jlocktime);

    if (raw->tobs.time != 0) lli = locktime - raw->lockt[sat - 1][freq_nr] + 0.05 <= tt;
    else lli = 0;
    raw->lockt[sat - 1][freq_nr] = locktime;

    obs_nr = obsindex(&raw->obs, raw->time, sat);

    raw->obs.data[obs_nr].P[freq_nr]    = pseudorange;
    raw->obs.data[obs_nr].D[freq_nr]    = doppler;
    raw->obs.data[obs_nr].L[freq_nr]    = -carrier_phase;
    raw->obs.data[obs_nr].SNR[freq_nr]  = snr;
    raw->obs.data[obs_nr].LLI[freq_nr]  = lli;
    raw->obs.data[obs_nr].code[freq_nr] = code;

    raw->obs.data[obs_nr].time = raw->time;
    raw->obs.data[obs_nr].sat  = sat;

    return 1;
}

static int decode_ogrp_measurement(raw_t *raw, json_object *jobj) {
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

static int save_subfrm(int sat, raw_t *raw, unsigned char *data) {
    unsigned char *q;
    int i, id;

    trace(4,"save_subfrm: sat=%2d\n", sat);

    /* check navigation subframe preamble */
    if (data[0] != 0x8B) {
        trace(2,"stq subframe preamble error: 0x%02X\n", data[0]);
        return 0;
    }
    id = (data[5] >> 2) & 0x7;

    /* check subframe id */
    if (id < 1 || 5 < id) {
        trace(2,"stq subframe id error: id=%d\n", id);
        return 0;
    }
    q = raw->subfrm[satno(SYS_GPS, sat) - 1] + (id - 1) * subframe_bytes_without_parity;

    for (i = 0; i < subframe_bytes_without_parity; i++) q[i] = data[i];

    return id;
}

static int save_page(int prn, raw_t *raw, unsigned char *data) {
    unsigned char *q;
    int i, type, sat;

    sat = satno(SYS_GAL, prn);

    trace(4,"save_page: sat=%2d\n", sat);

    type = data[0] & 0x3F;

    /* check word type */
    if (type < 0 || 10 < type) {
        trace(2,"Galileo word type error: type=%d\n", type);
        return -1;
    }

    /* Only the ephemeris data (page 1-4) and TOW page (5) is stored and decoded, for now */
    if (type < 1 || 5 < type) return -1;

    /* Re-use the GPS subframe buffer for Galileo INAV pages */
    q = raw->subfrm[sat - 1] + (type - 1) * inav_page_bytes_without_tail;
    for (i = 0; i < inav_page_bytes_without_tail; i++) q[i] = data[i];

    return type;
}

static int decode_ephem_gps(int sat, raw_t *raw) {
    eph_t eph = {0};

    trace(4,"decode_ephem_gps: sat=%2d\n", sat);

    if (decode_frame(raw->subfrm[satno(SYS_GPS, sat) - 1] + (subframe_bytes_without_parity * 0), &eph, NULL, NULL, NULL, NULL) != 1 ||
        decode_frame(raw->subfrm[satno(SYS_GPS, sat) - 1] + (subframe_bytes_without_parity * 1), &eph, NULL, NULL, NULL, NULL) != 2 ||
        decode_frame(raw->subfrm[satno(SYS_GPS, sat) - 1] + (subframe_bytes_without_parity * 2), &eph, NULL, NULL, NULL, NULL) != 3) return 0;

    if (eph.iode == raw->nav.eph[satno(SYS_GPS, sat) - 1].iode) return 0; /* unchanged */
    eph.sat = satno(SYS_GPS, sat);
    raw->nav.eph[satno(SYS_GPS, sat) - 1] = eph;
    raw->ephsat = satno(SYS_GPS, sat);
    return 2;
}

static int decode_ephem_galileo(int prn, raw_t *raw) {
    eph_t eph = {0};
    int i, j, sat, week;
    const unsigned nr_ephem_pages_and_tow = 5;
    const unsigned word_len_bytes = 16;
    unsigned char words[nr_ephem_pages_and_tow][word_len_bytes];
    unsigned char *data;
    double sqrtA, tow, toc, tt;

    sat = satno(SYS_GAL, prn);

    trace(4,"decode_ephem_galileo: sat=%2d\n", sat);

    for (i = 0; i < nr_ephem_pages_and_tow; i++) {
        data = (unsigned char *) (raw->subfrm[sat - 1] + inav_page_bytes_without_tail * i);
        /* Extract data bytes from even page part */
        for (j = 0; j < word_len_bytes - 2; j++) words[i][j] = ((data[j] << 2) & 0xFC) | ((data[j + 1] >> 6) & 0x03);
        /* Extract data bytes from odd page part */
        words[i][14] = ((data[14] << 4) & 0xF0) | ((data[15] >> 4) & 0x0F);
        words[i][15] = ((data[15] << 4) & 0xF0) | ((data[16] >> 4) & 0x0F);
        /* Check the word type */
        if (getbitu(words[i], 0, 6) != (i + 1)) {
            trace(2,"Galileo word type error: type=%d\n", (i + 1));
            return -1;
        }
    }

    eph.code   = 0; /* INAV=0, FNAV=1 */
    eph.iode   = getbitu(words[1 - 1],   6, 10);
    eph.iodc   = eph.iode;
    eph.toes   = getbitu(words[1 - 1],  16, 14) * 60.0;
    sqrtA      = getbitu(words[1 - 1],  94, 32) * c_2_pow_m_19;
    eph.A      = sqrtA * sqrtA;
    eph.e      = getbitu(words[1 - 1],  62, 32) * c_2_pow_m_33;
    eph.i0     = getbitu(words[2 - 1],  48, 32) * c_2_pow_m_31 * PI;
    eph.OMG0   = getbitu(words[2 - 1],  16, 32) * c_2_pow_m_31 * PI;
    eph.omg    = getbitu(words[2 - 1],  80, 32) * c_2_pow_m_31 * PI;
    eph.M0     = getbitu(words[1 - 1],  30, 32) * c_2_pow_m_31 * PI;
    eph.deln   = getbitu(words[3 - 1],  40, 16) * c_2_pow_m_43 * PI;
    eph.OMGd   = getbitu(words[3 - 1],  16, 24) * c_2_pow_m_43 * PI;
    eph.idot   = getbitu(words[2 - 1], 112, 14) * c_2_pow_m_43 * PI;
    eph.crc    = getbitu(words[3 - 1],  88, 16) * c_2_pow_m_5;
    eph.crs    = getbitu(words[3 - 1], 104, 16) * c_2_pow_m_5;
    eph.cuc    = getbitu(words[3 - 1],  56, 16) * c_2_pow_m_29;
    eph.cus    = getbitu(words[3 - 1],  72, 16) * c_2_pow_m_29;
    eph.cic    = getbitu(words[4 - 1],  22, 16) * c_2_pow_m_29;
    eph.cis    = getbitu(words[4 - 1],  38, 16) * c_2_pow_m_29;
    eph.f0     = getbitu(words[4 - 1],  68, 31) * c_2_pow_m_34;
    eph.f1     = getbitu(words[4 - 1],  99, 21) * c_2_pow_m_46;
    eph.f2     = getbitu(words[4 - 1], 120,  6) * c_2_pow_m_59;
    eph.tgd[0] = getbitu(words[5 - 1],  47, 10) * c_2_pow_m_32; /* BGD: E5A-E1 (s) */
    eph.tgd[1] = getbitu(words[5 - 1],  57, 10) * c_2_pow_m_32; /* BGD: E5B-E1 (s) */
    eph.week   = getbitu(words[5 - 1],  73, 12);
    toc = getbitu(words[4 - 1],  54, 14) * 60.0;

    tow = time2gpst(raw->time, &week);
    eph.week   = week;
    eph.toe    = gpst2time(eph.week, eph.toes);

    /* for week-handover problem see novatel.c*/
    tt = timediff(eph.toe, raw->time);
    if      (tt < -302400.0) eph.week++;
    else if (tt >  302400.0) eph.week--;
    eph.toe = gpst2time(eph.week, eph.toes);
    eph.toc = adjweek(eph.toe, toc);
    eph.ttr = adjweek(eph.toe, tow);

    eph.sat = sat;
    raw->nav.eph[sat - 1] = eph;
    raw->ephsat = sat;
    return 2;
}

static int decode_ogrp_raw_nav_msg_gps_l1ca(raw_t *raw, unsigned char *data, int nav_msg_id, int sat_id) {
    int sub_id;
    sub_id = save_subfrm(sat_id, raw, data);
    if (sub_id != nav_msg_id) {
        trace(2,"GPS subframe error\n");
        return -1;
    }
    if (sub_id == 3) return decode_ephem_gps(sat_id, raw);
    /* TODO decode subframe 4 and 5 */
    return 0;
}

static int decode_ogrp_raw_nav_msg_galileo_e1b(raw_t *raw, unsigned char *data, int nav_msg_id, int sat_id) {
    int word_type;
    word_type = save_page(sat_id, raw, data);
    if (word_type != nav_msg_id) {
        trace(2,"Galileo page error\n");
        return -1;
    }
    if (word_type == 4) return decode_ephem_galileo(sat_id, raw);
    return 0;
}

static int decode_ogrp_raw_nav_msg(raw_t *raw, json_object *jobj) {
    int sat_id, nr_bits, nav_msg_id, nr_bytes, count;
    char *gnss, *signal_type, *data_temp;
    json_object *jgnss, *jsignal_type, *jsat_id, *jnr_bits, *jnav_msg_id, *jdata;

    trace(5,"decode_ogrp_raw_nav_msg:\n");

    if (get_value_check_type(jobj, "gnss", &jgnss, json_type_string) < 0) return -1;
    gnss = json_object_get_string(jgnss);

    if (get_value_check_type(jobj, "signal_type", &jsignal_type, json_type_string) < 0) return -1;
    signal_type = json_object_get_string(jsignal_type);

    if (get_value_check_type(jobj, "nr_bits", &jnr_bits, json_type_int) < 0) return -1;
    nr_bits = json_object_get_int(jnr_bits);

    nr_bytes = (int)((float)(nr_bits) / 8.f);
    if ((float)(nr_bits) / 8.f - (int)((float)(nr_bits) / 8.f) > 0.f) nr_bytes++;
    unsigned char data[nr_bytes];

    if (get_value_check_type(jobj, "data", &jdata, json_type_string) < 0) return -1;
    data_temp = json_object_get_string(jdata);
    if (strlen(data_temp) != nr_bytes * 2) return -1;
    for(count = 0; count < nr_bytes; count++) {
        sscanf(data_temp, "%2hhx", &data[count]);
        data_temp += 2 * sizeof(char);
    }

    if (get_value_check_type(jobj, "nav_msg_id", &jnav_msg_id, json_type_int) < 0) return -1;
    nav_msg_id = json_object_get_int(jnav_msg_id);

    if (get_value_check_type(jobj, "sat_id", &jsat_id, json_type_int) < 0) return -1;
    sat_id = json_object_get_int(jsat_id);

    if (strcmp(gnss, "GPS") == 0 && strcmp(signal_type, "L1CA") == 0) {
        return decode_ogrp_raw_nav_msg_gps_l1ca(raw, data, nav_msg_id, sat_id);
    } else if (strcmp(gnss, "Galileo") == 0 && strcmp(signal_type, "E1B") == 0) {
        return decode_ogrp_raw_nav_msg_galileo_e1b(raw, data, nav_msg_id, sat_id);
    }

    trace(2, "Unsupported raw navigation message: %s %s\n", gnss, signal_type);
    return -1;
}

static int decode_ogrp_timestamp(raw_t *raw, json_object *jobj) {
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
    if (strcmp(id_value_str, "measurement") == 0) return decode_ogrp_measurement(raw, jobj);
    if (strcmp(id_value_str, "raw_nav_msg") == 0) return decode_ogrp_raw_nav_msg(raw, jobj);
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
