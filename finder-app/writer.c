#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

int main(int argc, char *argv[]) {
    openlog("writer", LOG_PID, LOG_USER);

    if (argc != 3) {
        syslog(LOG_ERR, "Invalid number of arguments: expected 2, got %d", argc - 1);
        fprintf(stderr, "Usage: %s <file_path> <string_to_write>\n", argv[0]);
        closelog();
        return 1;
    }

    const char *file_path = argv[1];
    const char *string_to_write = argv[2];

    FILE *fp = fopen(file_path, "w");
    if (fp == NULL) {
        syslog(LOG_ERR, "Failed to open file: %s", file_path);
        perror("fopen");
        closelog();
        return 1;
    }

    if (fputs(string_to_write, fp) == EOF) {
        syslog(LOG_ERR, "Failed to write to file: %s", file_path);
        perror("fputs");
        fclose(fp);
        closelog();
        return 1;
    }

    syslog(LOG_DEBUG, "Writing %s to %s", string_to_write, file_path);

    fclose(fp);
    closelog();
    return 0;
}

