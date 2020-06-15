#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <X11/Xlib.h>

char *tz = "Europe/Paris";

static Display *dpy;

char *
smprintf(char *fmt, ...)
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

void
settz(char *tzname)
{
	setenv("TZ", tzname, 1);
}

char *
mktimes(char *fmt, char *tzname)
{
	char buf[129];
	time_t tim;
	struct tm *timtm;

	settz(tzname);
	tim = time(NULL);
	timtm = localtime(&tim);
	if (timtm == NULL)
		return smprintf("");

	if (!strftime(buf, sizeof(buf)-1, fmt, timtm)) {
		fprintf(stderr, "strftime == 0\n");
		return smprintf("");
	}

	return smprintf("%s", buf);
}

void
setstatus(char *str)
{
	XStoreName(dpy, DefaultRootWindow(dpy), str);
	XSync(dpy, False);
}

char *
loadavg(void)
{
	double avgs[3];

	if (getloadavg(avgs, 3) < 0)
		return smprintf("");

	return smprintf("%.2f %.2f %.2f", avgs[0], avgs[1], avgs[2]);
}

char *
readfile(char *base, char *file)
{
	char *path, line[513];
	FILE *fd;

	memset(line, 0, sizeof(line));

	path = smprintf("%s/%s", base, file);
	fd = fopen(path, "r");
	free(path);
	if (fd == NULL)
		return NULL;

	if (fgets(line, sizeof(line)-1, fd) == NULL)
		return NULL;
	fclose(fd);

	return smprintf("%s", line);
}

char *
getbattery(char *base)
{
	char *co;
    int cap = -1;

	co = readfile(base, "present");
	if (co == NULL)
		return smprintf("");
	if (co[0] != '1') {
		free(co);
		return smprintf("not present");
	}
	free(co);

	co = readfile(base, "capacity");
	if (co == NULL) {
        return smprintf("");
	}
    cap = atoi(co);
    free(co);
    return smprintf("%d%%", cap);
}

char *
gettemperature(char *base, char *sensor)
{
    char* co;
    double temp = 0;
    
    co = readfile(base, sensor);
    if (co == NULL)
        return smprintf("");
    temp = atof(co);
    free(co);
    return smprintf("%02.0f°C", temp/1000);
}

char *
getfanspeed(char *base, char *sensor)
{
    char *co;
    int rpm = -1;

    co = readfile(base, sensor);
    if (co == NULL)
        return smprintf("");
    rpm = atoi(co);
    free(co);
    return smprintf("%drpm", rpm);
}

int
main(void)
{
	char *status;
	char *bat;
	char *time;
    char *tcpu;
    char *rpm1;
    char *rpm2;

	if (!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "dwmstatus: cannot open display.\n");
		return 1;
	}

    for (;;sleep(10)) {
        tcpu = gettemperature("/sys/class/hwmon/hwmon6", "temp1_input");
		bat = getbattery("/sys/class/power_supply/BAT0");
		time = mktimes("%H:%M", tz);
        rpm1 = getfanspeed("/sys/class/hwmon/hwmon5", "fan1_input");
        rpm2 = getfanspeed("/sys/class/hwmon/hwmon5", "fan2_input");

		status = smprintf("T:%s | Fans: %s %s | B:%s | %s",
				tcpu, rpm1, rpm2, bat, time);
		setstatus(status);
    
        free(tcpu);
        free(rpm1);
        free(rpm2);
		free(bat);
		free(time);
		free(status);
	}

	XCloseDisplay(dpy);

	return 0;
}

