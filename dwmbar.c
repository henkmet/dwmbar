/* made by profil 2011-12-29.
** personalised by Henk Metselaar
** Compile with:
** gcc -Wall -pedantic -std=c99 -lX11 -lmpdclient -march=native -O2 -o dwmbar dwmbar.c
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

void setstatus(char *str) {
    XStoreName(dpy, DefaultRootWindow(dpy), str);
    XSync(dpy, False);
}

char *
smprintf(char *fmt, ...) {
    va_list fmtargs;
    char *ret;
    int len;

    va_start(fmtargs, fmt);
    len = vsnprintf(NULL, 0, fmt, fmtargs);
    va_end(fmtargs);

    ret = malloc(++len);
    if(ret == NULL) {
        perror("malloc");
        exit(1);
    }

    va_start(fmtargs, fmt);
    vsnprintf(ret, len, fmt, fmtargs);
    va_end(fmtargs);

    return ret;
}

char *getnowplaying() {
    char *buffer = "dummy";
    FILE *f = fopen ("/home/henk/.radiotray/nowplaying", "r");

    if (f)
    {
        if (fseek (f, 0, SEEK_END) == 0) {
            int length = ftell(f);
            if  (length == 0) {
                buffer = smprintf("%s", "Even wachten", buffer);
                fclose(f);
                return buffer;
            }
            buffer = malloc(length+1);
            if (fseek(f, 0, SEEK_SET) != 0) {
                fclose(f);
                return "mislukt";
            }
            size_t newLen = fread(buffer, 1, length-1, f);
            if (newLen == 0) {
                fclose(f);
                return "mislukt";
            } else {
                buffer[newLen++] = '\0';
            }
        }
        fclose(f);
        return buffer;
    }
    else buffer = smprintf("%s", "Can't open file",buffer);
    return buffer;
}

char *getdatetime() {
    char *buf;
    time_t result;
    struct tm *resulttm;
    setlocale(LC_TIME,"nl_NL.utf8");

    if((buf = malloc(sizeof(char)*65)) == NULL) {
        fprintf(stderr, "Cannot allocate memory for buf.\n");
        exit(1);
    }
    result = time(NULL);
    resulttm = localtime(&result);
    if(resulttm == NULL) {
        fprintf(stderr, "Error getting localtime.\n");
        exit(1);
    }
    if(!strftime(buf, sizeof(char)*65-1, "%a %d %b %T", resulttm)) {
        fprintf(stderr, "strftime is 0.\n");
        exit(1);
    }

    return buf;
}

char *get_mpd() {
    struct mpd_connection *con;
    struct mpd_status *status = NULL;
    struct mpd_song *song = NULL;
    int status_type;
    char *res = NULL;
    char *title = NULL;
    int elapsed = 0, total = 0;
    char *token = NULL;
    char *deeltitel = NULL;

    if (!(con = mpd_connection_new("127.0.0.1", 0, 800)) ||
            mpd_connection_get_error(con)) {
        return smprintf("%s", "Geen verbinding");
    }
    mpd_command_list_begin(con, true);
    mpd_send_status(con);
    mpd_send_current_song(con);
    mpd_command_list_end(con);

    status = mpd_recv_status(con);
    if(status) {
        mpd_response_next(con);
        total = mpd_status_get_total_time(status);
        elapsed = mpd_status_get_elapsed_time(status);
        if(!(song=mpd_recv_song(con))) {
            mpd_response_finish(con);
            mpd_connection_free(con);
            free((char*)title);
            free((char*)deeltitel);
            res = smprintf("%s", "Geen speellijst");
            return res;
        }

        title = smprintf("%s",mpd_song_get_uri(song));

        if (strcmp(title, "http://streams.greenhost.nl:8080/live") == 0) {
					free((char*)title);
            title = smprintf("%s", "Concertzender");
        }
        else {
            token = strtok(title,"/");
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
        status_type = mpd_status_get_state(status);
        if(!status_type || status_type==MPD_STATE_UNKNOWN) {
            mpd_response_finish(con);
            mpd_connection_free(con);
            free((char*)title);
            free((char*)deeltitel);
            if (song!=NULL) {mpd_song_free(song);}
            mpd_response_finish(con);
            mpd_connection_free(con);
            res = smprintf("%s", "Geen titel");
            return res;
        }
        switch(status_type) {
        case(MPD_STATE_PLAY):
            res = smprintf("%s: %s", "Speelt", title);
            break;
        case(MPD_STATE_PAUSE):
            res = smprintf("%s: %s", "Gepauzeerd", title);
            break;
        case(MPD_STATE_STOP):
            res = smprintf("%s: %s", "Gestopt", title);
            break;
        default:
            res = smprintf("%s", "Geen gegevens2");
        }
    }
    else {
            res = smprintf("%s", "Geen gegevens3");
            if (song!=NULL) {mpd_song_free(song);}
            mpd_response_finish(con);
            mpd_connection_free(con);
            free((char*)title);
            free((char*)deeltitel);
            return res;
    }
    free((char*)title);
    free((char*)deeltitel);
    if (song !=NULL) {mpd_song_free(song);}
    mpd_response_finish(con);
    mpd_status_free(status);
    mpd_connection_free(con);
    return res;
}

int main(void) {
    char *status;
    char *datetime;
    char *nowplaying;
    char *mpd;

    if (!(dpy = XOpenDisplay(NULL))) {
        fprintf(stderr, "Cannot open display.\n");
        return 1;
    }

    if((status = malloc(200)) == NULL)
        exit(1);


    for (;; sleep(1)) {
        datetime = getdatetime();
        nowplaying = getnowplaying();
        mpd = get_mpd();
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
    }

    free(status);
    XCloseDisplay(dpy);

    return(0);
}
