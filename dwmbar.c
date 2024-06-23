/* Original code by profil 2011-12-29, personalized by Henk Metselaar
	 Compile with:
	 gcc -Wall -pedantic -std=c99 -lX11 -lmpdclient -march=native -O2 -o dwmbar dwmbar.c
	 */
#include <mpd/client.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <time.h>
#include <X11/Xlib.h>
#include <locale.h>
#include <string.h>
#include <errno.h>

static Display *dpy;

void setstatus(const char *str) {
	XStoreName(dpy, DefaultRootWindow(dpy), str);
	XSync(dpy, False);
}

char *smprintf(const char *fmt, ...) {
	va_list fmtargs;
	char *ret;
	int len;

	va_start(fmtargs, fmt);
	len = vsnprintf(NULL, 0, fmt, fmtargs);
	va_end(fmtargs);

	ret = malloc(len + 1);
	if (ret == NULL) {
		perror("malloc");
		exit(1);
	}

	va_start(fmtargs, fmt);
	vsnprintf(ret, len + 1, fmt, fmtargs);
	va_end(fmtargs);

	return ret;
}

char *read_file_content(const char *path) {
	FILE *f = fopen(path, "r");
	if (!f) {
		return smprintf("Can't open file");
	}

	fseek(f, 0, SEEK_END);
	long length = ftell(f);
	fseek(f, 0, SEEK_SET);

	if (length == 0) {
		fclose(f);
		return smprintf("Even wachten");
	}

	char *buffer = malloc(length + 1);
	if (!buffer) {
		fclose(f);
		perror("malloc");
		exit(1);
	}

	fread(buffer, 1, length, f);
	buffer[length] = '\0';
	fclose(f);

	return buffer;
}

char *getdatetime() {
	char buf[65];
	time_t result = time(NULL);
	struct tm *resulttm = localtime(&result);

	if (!resulttm) {
		fprintf(stderr, "Error getting localtime.\n");
		exit(1);
	}

	setlocale(LC_TIME, "nl_NL.utf8");
	if (!strftime(buf, sizeof(buf), "%a %d %b %T", resulttm)) {
		fprintf(stderr, "strftime failed.\n");
		exit(1);
	}

	return smprintf("%s", buf);
}

char *get_mpd() {
	struct mpd_connection *con = mpd_connection_new("127.0.0.1", 0, 800);
	if (!con || mpd_connection_get_error(con)) {
		mpd_connection_free(con);
		return smprintf("Geen verbinding");
	}

	mpd_command_list_begin(con, true);
	mpd_send_status(con);
	mpd_send_current_song(con);
	mpd_command_list_end(con);

	struct mpd_status *status = mpd_recv_status(con);
	if (!status) {
		mpd_connection_free(con);
		return smprintf("Geen gegevens");
	}

	mpd_response_next(con);
	struct mpd_song *song = mpd_recv_song(con);
	int total = mpd_status_get_total_time(status);
	int elapsed = mpd_status_get_elapsed_time(status);

	char *title = NULL;
	if (song) {
		title = smprintf("%s", mpd_song_get_uri(song));
	}

	if (strcmp(title, "http://streams.greenhost.nl:8080/live") == 0) {
		free((char*)title);
		title = smprintf("%s", "Concertzender");
	}

	else {
		char *token = strtok(title,"/");
		char *deeltitel=NULL;
		while (token != NULL) {
			deeltitel = smprintf("%s",token);
			token = strtok(NULL,"/");
		}
		free((char*)title);
		title = smprintf("%s %2d:%.2d/%2d:%.2d",
				strtok(deeltitel,"."),
				elapsed/60, elapsed%60,
				total/60, total%60);
	}

	int status_type = mpd_status_get_state(status);
	char *res = NULL;
	switch (status_type) {
		case MPD_STATE_PLAY:
			res = smprintf("Speelt: %s", title);
			break;
		case MPD_STATE_PAUSE:
			res = smprintf("Gepauzeerd: %s", title);
			break;
		case MPD_STATE_STOP:
		default:
			res = smprintf("Gestopt: %s", title);
			break;
	}

	if (song) {
		mpd_song_free(song);
	}
	mpd_status_free(status);
	mpd_connection_free(con);
	free(title);

	return res;
}

int main(void) {
	if (!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "Cannot open display.\n");
		return 1;
	}

	char status[200];

	while (1) {
		char *datetime = getdatetime();
		char *nowplaying = read_file_content("/home/henk/.radiotray/nowplaying");
		char *mpd = get_mpd();

		if (strcmp(mpd, "Speelt: Concertzender") == 0) {
			snprintf(status, 200, "%s | %s |  %s", mpd, nowplaying, datetime);
		}
		else {
			snprintf(status, 200, "%s | %s |  %s", mpd, nowplaying, datetime);
		}

		setstatus(status);

		free(datetime);
		free(nowplaying);
		free(mpd);

		sleep(1);
	}

	XCloseDisplay(dpy);
	return 0;
}
