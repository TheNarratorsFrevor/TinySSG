// tinyssg.c - brutal minimal static site generator in C w/ md4c-html

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include "md4c.h"
#include "md4c-html.h"

#define MAX_PATH 1024
#define MAX_BUFFER 4096
#define MAX_EXT 10

#define HTML_EXT ".html"
#define MARKDOWN_EXT ".md"
#define INPUT_DIR "input"
#define OUTPUT_DIR "output"
#define INDEX_FILE "index.html"
#define TEMPLATE_FILE "template.html"

typedef struct {
    char input_dir[MAX_PATH];
    char output_dir[MAX_PATH];
    char index_file[MAX_PATH];
    char template_file[MAX_PATH];
} Config;

typedef struct {
    char path[MAX_PATH];
    char name[MAX_PATH];
    char ext[MAX_EXT];
} File;

typedef struct Directory {
    char path[MAX_PATH];
    char name[MAX_PATH];
    File *files;
    size_t file_count;
    size_t file_capacity;
    struct Directory *next;
    struct Directory *prev;
} Directory;

Config config = {
    .input_dir = INPUT_DIR,
    .output_dir = OUTPUT_DIR,
    .index_file = INDEX_FILE,
    .template_file = TEMPLATE_FILE
};

int mkdir_p(const char *path) {
    char tmp[MAX_PATH];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    return mkdir(tmp, 0755);
}

int create_directory(const char *path) {
    if (!path || strlen(path) == 0) {
        fprintf(stderr, "error: invalid path\n");
        return EINVAL;
    }
    struct stat st = {0};
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) return 0;
    if (mkdir(path, 0755) != 0) {
        perror("mkdir");
        return errno;
    }
    return 0;
}

int copy_file(const char *src, const char *dest) {
    if (!src || !dest) return EINVAL;
    if (access(src, F_OK) != 0) {
        fprintf(stderr, "error: file not found: %s\n", src);
        return ENOENT;
    }

    FILE *fin = fopen(src, "rb");
    if (!fin) return errno;
    FILE *fout = fopen(dest, "wb");
    if (!fout) {
        fclose(fin);
        return errno;
    }

    char buf[MAX_BUFFER];
    size_t bytes;
    while ((bytes = fread(buf, 1, sizeof(buf), fin)) > 0)
        fwrite(buf, 1, bytes, fout);

    fclose(fin);
    fclose(fout);
    return 0;
}

typedef struct {
    char *buf;
    size_t len;
} Buffer;

void cb(const MD_CHAR *text, MD_SIZE size, void *userdata) {
    Buffer *b = (Buffer *)userdata;
    size_t old_len = b->len;
    b->len += size;
    char *tmp = realloc(b->buf, b->len + 1);
    if (!tmp) {
        free(b->buf);
        b->buf = NULL;
        b->len = 0;
        return;
    }
    b->buf = tmp;
    memcpy(b->buf + old_len, text, size);
    b->buf[b->len] = '\0';
}

char *md_to_html(const char *md, size_t len) {
    Buffer buf = {NULL, 0};
    int ret = md_html(md, len, cb, &buf, MD_DIALECT_COMMONMARK, 0);
    if (ret != 0) {
        free(buf.buf);
        return NULL;
    }
    return buf.buf;
}

int convert_md_to_html(const char *md_path, const char *html_path) {
    FILE *f = fopen(md_path, "rb");
    if (!f) {
        perror("fopen md_path");
        return -1;
    }
    fseek(f, 0, SEEK_END);
    size_t len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *md = malloc(len + 1);
    if (!md) {
        fclose(f);
        fprintf(stderr, "malloc failed\n");
        return -1;
    }
    fread(md, 1, len, f);
    md[len] = '\0';
    fclose(f);

    char *html = md_to_html(md, len);
    free(md);

    if (!html) {
        fprintf(stderr, "md4c conversion failed\n");
        return -1;
    }

    FILE *fo = fopen(html_path, "w");
    if (!fo) {
        perror("fopen html_path");
        free(html);
        return -1;
    }
    fwrite(html, 1, strlen(html), fo);
    fclose(fo);
    free(html);
    return 0;
}

int inject_and_write(const char *template_path, const char *input_md_path, const char *body_path) {
    char output_path[MAX_PATH];
    snprintf(output_path, sizeof(output_path), "%s/%s", config.output_dir, input_md_path + strlen(config.input_dir));

    char *dot = strrchr(output_path, '.');
    if (dot && strcmp(dot, ".md") == 0)
        strcpy(dot, ".html");

    char dir_path[MAX_PATH];
    snprintf(dir_path, sizeof(dir_path), "%s", output_path);
    char *last_slash = strrchr(dir_path, '/');
    if (last_slash) {
        *last_slash = '\0';
        mkdir_p(dir_path);
    }

    FILE *ft = fopen(template_path, "r");
    FILE *fb = fopen(body_path, "r");
    FILE *fo = fopen(output_path, "w");
    if (!ft || !fb || !fo) {
        perror("fopen");
        return -1;
    }

    char line[MAX_BUFFER];
    while (fgets(line, sizeof(line), ft)) {
        if (strstr(line, "{{ content }}")) {
            char buf[MAX_BUFFER];
            while (fgets(buf, sizeof(buf), fb))
                fputs(buf, fo);
        } else {
            fputs(line, fo);
        }
    }

    fclose(ft);
    fclose(fb);
    fclose(fo);
    return 0;
}

int build_page(const char *md_path) {
    char tmp_html[MAX_PATH] = "/tmp/tmp_page.html";

    // template path: same dir as md file or fallback
    char template_path[MAX_PATH];
    snprintf(template_path, sizeof(template_path), "%s", md_path);
    char *last_slash = strrchr(template_path, '/');
    if (last_slash) {
        *(last_slash + 1) = '\0';
        strncat(template_path, "template.html", MAX_PATH - strlen(template_path) - 1);
    }

    if (access(template_path, F_OK) != 0) {
        snprintf(template_path, sizeof(template_path), "%s/%s", config.input_dir, config.template_file);
        if (access(template_path, F_OK) != 0) {
            fprintf(stderr, "[-] no template found for %s\n", md_path);
            return -1;
        }
    }

    if (convert_md_to_html(md_path, tmp_html) != 0)
        return -1;

    if (inject_and_write(template_path, md_path, tmp_html) != 0)
        return -1;

    printf("[+] built: %s\n", md_path);
    return 0;
}

void walk_and_build(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) return;

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char fullpath[MAX_PATH];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);

        struct stat st;
        if (stat(fullpath, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            walk_and_build(fullpath);
        } else if (strstr(entry->d_name, ".md")) {
            build_page(fullpath);
        }
    }

    closedir(dir);
}

void banner() {
    printf("\n\033[1;32m");
    printf("\n"
"\n"
"  _____ _          ___ ___  ___ \n"
" |_   _(_)_ _ _  _/ __/ __|/ __|\n"
"   | | | | ' \\ || \\__ \\__ \\ (_ |\n"
"   |_| |_|_||_\\_, |___/___/\\___|\n"
"              |__/              \n"
"\n"
"  tinyssg - a brutally minimal static site generator in C\n"
);
    printf("\033[0m\n");
}

int main(void) {
    banner();

    printf("[*] tinyssg dry run started.\n");
    printf("[*] reading config file from ... just kidding, configs are bloat.\n");

    printf("[*] creating output directory: %s\n", config.output_dir);
    if (create_directory(config.output_dir) != 0) {
        fprintf(stderr, "[-] failed to create output dir\n");
        return EXIT_FAILURE;
    }

    printf("[*] walking input directory and building pages...\n");
    walk_and_build(config.input_dir);

    printf("[+] tinyssg dry run complete.\n");
    return EXIT_SUCCESS;
}

