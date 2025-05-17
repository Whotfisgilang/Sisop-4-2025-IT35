#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>

void download() {
    system("wget -O anomali.zip \"https://drive.google.com/uc?export=download&id=1hi_GDdP51Kn2JJMw02WmCOxuc3qrXzh5\"");
    system("unzip -o anomali.zip -d .");
    system("rm -f anomali.zip");
}

void create_folder(const char *path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        mkdir(path, 0755);
    }
}

int is_hex_char(char c) {
    return isdigit(c) || (tolower(c) >= 'a' && tolower(c) <= 'f');
}

unsigned char hex_to_byte(char a, char b) {
    unsigned char value = 0;
    if (isdigit(a)) value += (a - '0') << 4;
    else value += (tolower(a) - 'a' + 10) << 4;

    if (isdigit(b)) value += (b - '0');
    else value += (tolower(b) - 'a' + 10);

    return value;
}

void convert(const char *filename) {
    char path[256];
    snprintf(path, sizeof(path), "anomali/%s", filename);

    FILE *fp = fopen(path, "r");
    if (!fp) return;

    char *filtered = malloc(1);
    size_t cap = 1, len = 0;
    int c;

    while ((c = fgetc(fp)) != EOF) {
        if (is_hex_char(c)) {
            if (len + 1 >= cap) {
                cap *= 2;
                filtered = realloc(filtered, cap);
            }
            filtered[len++] = c;
        }
    }
    fclose(fp);

    if (len % 2 != 0) len--;
    filtered[len] = '\0';

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char imgname[256];
    snprintf(imgname, sizeof(imgname), "%.*s_image_%04d-%02d-%02d_%02d:%02d:%02d.png",
             (int)(strlen(filename) - 4), filename,
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
             t->tm_hour, t->tm_min, t->tm_sec);

    char imgpath[256];
    snprintf(imgpath, sizeof(imgpath), "anomali/image/%s", imgname);

    FILE *out = fopen(imgpath, "wb");
    if (!out) {
        free(filtered);
        return;
    }

    for (int i = 0; i < len; i += 2) {
        unsigned char byte = hex_to_byte(filtered[i], filtered[i+1]);
        fwrite(&byte, 1, 1, out);
    }

    fclose(out);

    FILE *log = fopen("anomali/conversion.log", "a");
    if (log) {
        fprintf(log, "[%04d-%02d-%02d][%02d:%02d:%02d]: Successfully converted %s to %s\n",
                t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                t->tm_hour, t->tm_min, t->tm_sec,
                filename, imgname);
        fclose(log);
    }

    free(filtered);
    sleep(1);
}

void process_all_txt() {
    DIR *dir = opendir("anomali");
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".txt")) {
            char fullpath[512];
            snprintf(fullpath, sizeof(fullpath), "anomali/%s", entry->d_name);
            struct stat st;
            if (stat(fullpath, &st) == 0 && S_ISREG(st.st_mode)) {
                convert(entry->d_name);
            }
        }
    }

    closedir(dir);
}

static void *fs_init(struct fuse_conn_info *conn) {
    (void)conn;
    download();
    create_folder("anomali/image");
    process_all_txt();
    printf(">> Semua file telah dikonversi.\n");
    return NULL;
}

static int fs_getattr(const char *path, struct stat *st) {
    char real_path[512];
    snprintf(real_path, sizeof(real_path), "anomali%s", path);
    return lstat(real_path, st);
}

static int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    (void) offset;
    (void) fi;

    char real_path[512];
    snprintf(real_path, sizeof(real_path), "anomali%s", path);

    DIR *dp = opendir(real_path);
    if (!dp) return -ENOENT;

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        filler(buf, de->d_name, NULL, 0);
    }

    closedir(dp);
    return 0;
}

static int fs_open(const char *path, struct fuse_file_info *fi) {
    char real_path[512];
    snprintf(real_path, sizeof(real_path), "anomali%s", path);
    FILE *f = fopen(real_path, "rb");
    if (!f) return -ENOENT;
    fclose(f);
    return 0;
}

static int fs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    (void) fi;
    char real_path[512];
    snprintf(real_path, sizeof(real_path), "anomali%s", path);
    FILE *f = fopen(real_path, "rb");
    if (!f) return -ENOENT;

    fseek(f, offset, SEEK_SET);
    size_t res = fread(buf, 1, size, f);
    fclose(f);
    return res;
}

static struct fuse_operations fs_oper = {
    .init       = fs_init,
    .getattr    = fs_getattr,
    .readdir    = fs_readdir,
    .open       = fs_open,
    .read       = fs_read,
};

int main(int argc, char *argv[]) {
    return fuse_main(argc, argv, &fs_oper, NULL);
}
