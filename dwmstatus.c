#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <math.h>

#include <alsa/asoundlib.h>
#include <alsa/control.h>


#include <X11/Xlib.h>


typedef struct {
    char  icon[32];
    char  text[64];
    char color[32];
} BlockData;

typedef struct {
    void (*query)(BlockData*);
    const int interval;
    const time_t align;
    const int delay;
} Block;

/* function declarations */
char* strip(char* str);
char* smprintf(char *fmt, ...);
void setstatus(char *str);
char* read_file(char *path);

void get_time(BlockData* data);
void get_battery(BlockData* data);
void get_power(BlockData* data);
void get_temperature(BlockData* data);
void get_fan_speed(BlockData* data);
void get_volume(BlockData* data);

#define LENGTH(X) (sizeof X / sizeof X[0])

/* variables */
static Display *dpy;

/* configuration */
static const char bar_color[] = "#282828";


static const Block blocks[] = {
    /* query:    function to call periodically
     * interval: how many seconds between each call of `query`
     * align:    align the interval with the specified epoch time if non zero
     * delay:    time to wait before the first call of the `query`.
     *           If -1 and align != 0, start immediately the `query` and align the next calls.
     */
    /* query      interval         align  delay */
    { get_volume,       1,             0,    0 },
    { get_fan_speed,    20,            0,    0 },
    { get_battery,      120,           0,    0 },
    { get_power,        20,            0,    0 },
    { get_temperature,  20,            0,    0 },
    { get_time,         60,   1592384460,   -1 },
};



char* strip(char* str)
{
    size_t ln = strlen(str) - 1;
    if (*str && str[ln] == '\n') {
        str[ln] = '\0';
    }
    return str;
}

char* smprintf(char *fmt, ...)
{
    va_list fmtargs;
    char *ret;
    int len;

    va_start(fmtargs, fmt);
    len = vsnprintf(NULL, 0, fmt, fmtargs);
    va_end(fmtargs);

    ret = malloc(++len);
    if (ret == NULL) {
        perror("malloc");
        exit(1);
    }

    va_start(fmtargs, fmt);
    vsnprintf(ret, len, fmt, fmtargs);
    va_end(fmtargs);

    return ret;
}

void setstatus(char *str)
{
    XStoreName(dpy, DefaultRootWindow(dpy), str);
    XSync(dpy, False);
}


char* read_file(char *path)
{
    char line[513];
    FILE *fd;

    memset(line, 0, sizeof(line));

    fd = fopen(path, "r");
    if (fd == NULL)
        return NULL;

    if (fgets(line, sizeof(line)-1, fd) == NULL)
        return NULL;
    fclose(fd);

    return smprintf("%s", line);
}

void get_time(BlockData* data)
{
    char buf[129];
    time_t tim;
    struct tm *timtm;

    char *str;
    int hour = -1;

    tim = time(NULL);
    timtm = localtime(&tim);
    if (timtm == NULL){
        str = smprintf("\uf071 ");
    } else{

        if (!strftime(buf, sizeof(buf)-1, "%H:%M", timtm)) {
            fprintf(stderr, "strftime == 0\n");
            str = smprintf("\uf071 ");
        }else{
            str = smprintf("%s", buf);
            hour = timtm->tm_hour;
        }
    }

    if(hour == -1){
        strcpy(data->icon, "\ue38a");
    }
    else{
        char *clocks[12] = {"\ue381", "\ue382", "\ue383", "\ue384", "\ue385", "\ue386", "\ue387", "\ue388", "\ue389", "\ue38a", "\ue38b", "\ue38c"};
        if (hour >= 12){
            hour -= 12;
        }
        if (hour < 0){
            strcpy(data->icon, "\uf071 ");
        }else{
            strcpy(data->icon, clocks[hour]);
        }
    }
    strcpy(data->color, "#ffffff");
    strcpy(data->text, str);
    free(str);
}

void get_battery(BlockData* data)
{
    char *str;
    int cap = -1;

    str = read_file("/sys/class/power_supply/BAT0/present");
    if (str == NULL){
        str = smprintf("\uf071 ");
    }
    else if (str[0] != '1') {
        free(str);
        str = smprintf("\uf128");
    }
    else{
        free(str);
        str = read_file("/sys/class/power_supply/BAT0/capacity");
        if (str == NULL) {
            str = smprintf("\uf071 ");
        }else{
            cap = atoi(str);
            free(str);
            str = smprintf("%d%%", cap);
        }
    }

    if(cap == -1 || cap >= 80){
        strcpy(data->icon, "\uf240 ");
    }else if(cap >= 60){
        strcpy(data->icon, "\uf241 ");
    }else if(cap >= 40){
        strcpy(data->icon, "\uf242 ");
    }else if(cap >= 20){
        strcpy(data->icon, "\uf243 ");
    }else{
        strcpy(data->icon, "\uf244 ");
    }

    strcpy(data->color, "#a3be8c");
    strcpy(data->text, str);
    free(str); 
}

void get_power(BlockData* data)
{  
    /* circular buffer */
    static float history[5];
    static size_t end = 0;
    static size_t len = 0;

    long int current = 0;
    long int voltage = 0;

    strcpy(data->color, "#d06c4c");

    char *file;

    /* Hide the block if battery full */
    file = read_file("/sys/class/power_supply/BAT0/status");
    if(file != NULL && !strcmp(file,"Full\n")){
        strcpy(data->icon, "");
        strcpy(data->text, "");
        return;
    }

    strcpy(data->icon, "\uf0e7");


    file = read_file("/sys/class/power_supply/BAT0/current_now");
    if (file == NULL){
        strcpy(data->text, "\uf071 ");
        return;
    }else{
        current = strtol(file, NULL, 10);
    }

    file = read_file("/sys/class/power_supply/BAT0/voltage_now");
    if (file == NULL){
        strcpy(data->text, "\uf071 ");
        return;
    }else{
        voltage = strtol(file, NULL, 10);
    }

    if(voltage == 0 || current == 0){
        strcpy(data->text, "\uf071 ");
        return;
    }
    else{
        float power = current/1e6*voltage/1e6;
        history[end] = power;
        if(len < LENGTH(history)){
            len += 1;
        }
        end = (end+1)%LENGTH(history);

        /* Averaging */
        float sum = 0;
        for(size_t i=0; i < len; ++i){
            sum += history[i];
        }
        sum /= len;

        char *str = smprintf("%.1fW", sum);
        strcpy(data->text, str);
        free(str);
    }
}

void get_temperature(BlockData* data)
{
    char* str;
    double temp = 0;
    
    str = read_file("/sys/class/hwmon/hwmon6/temp1_input");
    if (str == NULL){
        str = smprintf("\uf071 ");
    } else{
        temp = atof(str)/1000;
        free(str);
        str = smprintf("%02.0f°C", temp);
    }

    if (temp >= 60){
        strcpy(data->icon, "\ue20b");
    } else if (temp >= 40){
        strcpy(data->icon, "\ue20a");
    } else{
        strcpy(data->icon, "\ue20c");
    }
    strcpy(data->color, "#e85c6a");
    strcpy(data->text, str);
    free(str);
}

void get_fan_speed(BlockData* data)
{
    char *rpm1;
    char *rpm2;
    char *txt;

    int rpm1_i, rpm2_i;
    rpm1_i = -1;
    rpm2_i = -1;

    rpm1 = read_file("/sys/class/hwmon/hwmon5/fan1_input");
    if (rpm1 == NULL){
        rpm1 = smprintf("\uf071 ");
    }else{
        rpm1_i = atoi(rpm1);
        free(rpm1);
        rpm1 = smprintf("%d", rpm1_i);
    }

    rpm2 = read_file("/sys/class/hwmon/hwmon5/fan2_input");
    if (rpm2 == NULL){
        rpm2 = smprintf("\uf071 ");
    }else{
        rpm2_i = atoi(rpm2);
        free(rpm2);
        rpm2 = smprintf("%d", rpm2_i);
    }

    if(rpm1_i == -1 && rpm2_i == -1){
        txt = smprintf("%s %s", rpm1, rpm2);
    }else{
        if(rpm1_i == 0 && rpm2_i == 0){
            txt = smprintf(" ");
        }else{
            txt = smprintf("%s %s rpm", rpm1, rpm2);
        }
    }

    if((rpm1_i != -1 || rpm2_i != -1) && (rpm1_i == 0 && rpm2_i == 0)){
        strcpy(data->icon, "\ufd1b");
    }else{
        strcpy(data->icon, "\uf70f");
    }

    strcpy(data->color, "#88c0d0");
   
    free(rpm1);
    free(rpm2);
    strcpy(data->text, txt);
    free(txt);
}

void get_volume(BlockData* data)
{
    strcpy(data->icon, "\ufa7d");
    strcpy(data->color, "#ebcb8b");
    strcpy(data->text, "\uf071 ");

    int vol;
    snd_hctl_t *hctl;
    snd_ctl_elem_id_t *id;
    snd_ctl_elem_value_t *control;

    // To find card and subdevice: /proc/asound/, aplay -L, amixer controls
    if(snd_hctl_open(&hctl, "hw:0", 0)<0){
        fprintf(stderr, "%s", "snd_hctl_open"); 
        return;
    }
    if(snd_hctl_load(hctl)<0){
        fprintf(stderr, "%s", "snd_hctl_load"); 
        return;
    }

    snd_ctl_elem_id_alloca(&id);
    snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_MIXER);

    // amixer controls
    snd_ctl_elem_id_set_name(id, "Master Playback Volume");

    snd_hctl_elem_t *elem = snd_hctl_find_elem(hctl, id);
    if(elem == NULL){
        fprintf(stderr, "%s", "snd_hctl_find_elemooo"); 
        return;
    }

    snd_ctl_elem_value_alloca(&control);
    snd_ctl_elem_value_set_id(control, id);

    snd_hctl_elem_read(elem, control);
    vol = (int)snd_ctl_elem_value_get_integer(control,0);

    snd_hctl_close(hctl);

    /* The volume is in the range 0 - 127 but it follows
     * a pseudo logarithmic relation with the actual volume
     * (between 0 and 100%). No equation fits perfectly the
     * curve so I do a linear interpolation with the measured
     * values.
     */
    int volumes[] = {
        0, 1, 2, 3, 4, 5, 7, 8, 9, 10, 11, 12, 14, 15, 16, 17, 18, 20,
        21, 22, 23, 24, 26, 27, 29, 30, 32, 33, 35, 37, 38, 40, 42, 43,
        44, 45, 46, 47, 49, 50, 51, 52, 53, 54, 56, 57, 58, 60, 61, 62,
        64, 65, 67, 68, 69, 71, 73, 74, 76, 77, 79, 81, 83, 84, 86, 88,
        90, 92, 94, 96, 98, 100, 
    };

    int alsa_volumes[] = {
        0, 5, 10, 15, 19, 23, 27, 31, 34, 37, 40, 43, 46, 49, 52, 54, 56,
        58, 60, 62, 64, 66, 68, 70, 72, 74, 76, 78, 80, 82, 84, 86, 88, 89,
        90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105,
        106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119,
        120, 121, 122, 123, 124, 125, 126, 127, 
    };

    size_t pos = 0;
    while(pos < LENGTH(alsa_volumes)-1 && alsa_volumes[pos+1]<vol){
        ++pos;
    }

    int actual_volume = 0;
    if(pos != 0){
        float vol_f =volumes[pos] + (vol-alsa_volumes[pos])*(volumes[pos+1]-volumes[pos])/(float)(alsa_volumes[pos+1]-alsa_volumes[pos]);
        actual_volume = round(vol_f);
    }

    /* BlockData generation */
    if(actual_volume == 0){
        strcpy(data->icon, "\ufc5d");
    }else if(actual_volume < 25){
        strcpy(data->icon, "\ufa7e");
    }else if(actual_volume < 50){
        strcpy(data->icon, "\ufa7f");
    }else{
        strcpy(data->icon, "\ufa7d");
    }

    strcpy(data->color, "#ebcb8b");

    if (actual_volume != 0){
        char *str = smprintf("%d%%", actual_volume);
        strcpy(data->text, str);
        free(str);     
    } else{
        strcpy(data->text, " ");
    }

}


void sleep_until(time_t seconds)
{
    sleep(seconds-time(NULL));
}

int all_space(char *str){
    while(*str != 0){
        if(*str != ' '){
            return 0;
        }
        ++str;
    }
    return 1;
}


int main(void)
{
    if (!(dpy = XOpenDisplay(NULL))) {
        fprintf(stderr, "dwmstatus: cannot open display.\n");
        return 1;
    }

    BlockData data;
    char status[LENGTH(blocks)*128];

    /* flags:
     * 0x01 -> call it now
     */ 
    int flags[LENGTH(blocks)] = {0};

    time_t next_update[LENGTH(blocks)];
    char* block_strings[LENGTH(blocks)];

    time_t now = time(NULL);
    for(int i=0; i < LENGTH(blocks); ++i){
        if(blocks[i].align != 0){
            time_t delta = now - blocks[i].align;
            double passed = delta / (double)blocks[i].interval;
            next_update[i] = ceil(passed)*blocks[i].interval + blocks[i].align + blocks[i].delay;
           
            if(blocks[i].delay == -1){
                flags[i] |= 1<<0;
                next_update[i] += 1;
            }

        }else{
            next_update[i] = now+blocks[i].delay;
        }
        block_strings[i] = NULL;
    }

    while(1){

        /* Run tasks and update next_update if needed */
        now = time(NULL);
        int changed = 0;
        for(int i=0; i < LENGTH(blocks); ++i){

            if (next_update[i] <= now || (flags[i] & (1<<0) )){

                /* Query informations and format them using status2d color codes */
                blocks[i].query(&data);

                if(block_strings[i] != NULL){
                     free(block_strings[i]);
                }
                if(strlen(data.icon) != 0 && strlen(data.text) != 0){
                    if(all_space(data.text)){
                        block_strings[i] = smprintf("^c%s^^b%s^ %s ^c%s^^b%s^%s", bar_color, data.color, data.icon, data.color, bar_color, data.text);
                    }else{
                        block_strings[i] = smprintf("^c%s^^b%s^ %s ^c%s^^b%s^ %s ", bar_color, data.color, data.icon, data.color, bar_color, data.text);
                    }
                }else if (strlen(data.icon) == 0 && strlen(data.text) != 0){
                    if(all_space(data.text)){
                        block_strings[i] = smprintf("%s", data.text);
                    }else{
                        block_strings[i] = smprintf("^c%s^^b%s^ %s ", data.color, bar_color, data.text);
                    }
                }else if (strlen(data.icon) != 0 && strlen(data.text) == 0){
                    block_strings[i] = smprintf("^c%s^^b%s^ %s ", bar_color, data.color, data.icon);
                }else{
                    block_strings[i] = smprintf("");
                }

                /* normal case */
                if (next_update[i] <= now){
                    next_update[i] += blocks[i].interval;
                }
                if(flags[i] & (1<<0)){
                    flags[i] &= ~(1<<0);
                }

                changed = 1;
            }

        }

        /* Update status */
        if(changed){
            memset(status, 0, sizeof(status));
            for(int i=0; i < LENGTH(blocks); ++i){
                if(block_strings[i] != NULL){
                    strcat(status, block_strings[i]);
                }
            }
            setstatus(status);
        }

        /* Determine how long we can sleep */
        time_t min_next = next_update[0];
        for(int i=1; i < LENGTH(blocks); ++i){
            if (next_update[i] < min_next){
                min_next = next_update[i];
            }
        }

        sleep_until(min_next);
    }

    XCloseDisplay(dpy);

    return 0;
}

