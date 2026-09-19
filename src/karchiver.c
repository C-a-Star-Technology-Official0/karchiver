/*
 * Karchiver - مستخرج أرشيفات شامل لـ GNU/Linux
 * حقوق النشر (C) 2026 شركة C.a. Star Technology
 *
 * هذا البرنامج برمجية حرة: يمكنك إعادة توزيعه و/أو تعديله
 * بموجب شروط رخصة GNU العامة كما نشرتها مؤسسة البرمجيات الحرة،
 * إما الإصدار 3 من الرخصة، أو (حسب اختيارك) أي إصدار أحدث.
 *
 * هذا البرنامج يوزّع على أمل أن يكون مفيدًا،
 * لكن بدون أي ضمان، حتى الضمان الضمني لقابلية التسويق
 * أو الملاءمة لغرض معين. راجع رخصة GNU العامة لمزيد من التفاصيل.
 *
 * يجب أن تكون قد استلمت نسخة من رخصة GNU العامة مع هذا البرنامج.
 * إن لم يكن، راجع <https://www.gnu.org/licenses/>.
 *
 * للتواصل: شركة C.a. Star Technology <https://c-a-star.blogspot.com/>
 */
/*
 * Karchiver - نسخة بدون libarchive
 * تعتمد على أدوات النظام عبر execvp (بدون shell injection)
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <strings.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#define K_OK           0
#define K_ERR_ARGS     1
#define K_ERR_NOTFOUND 2
#define K_ERR_OPEN     3
#define K_ERR_WRITE    4
#define K_ERR_EXTERNAL 7
#define K_MAX_PATH     PATH_MAX

typedef struct {
    char archive[K_MAX_PATH];
    char dest[K_MAX_PATH];
    int  verbose;
    int  list_only;
} KOptions;

static void k_info(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    fprintf(stdout, "[karchiver] "); vfprintf(stdout, fmt, ap);
    fprintf(stdout, "\n"); va_end(ap);
}
static void k_err(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    fprintf(stderr, "[karchiver:error] "); vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n"); va_end(ap);
}

typedef struct {
    const char *name;
    char *argv[16];
    int  argc;
    const char *workdir;
} Cmd;

static void cmd_init(Cmd *c, const char *name) {
    memset(c, 0, sizeof(*c));
    c->name = name;
    c->argv[c->argc++] = (char*)name;
}
static void cmd_arg(Cmd *c, const char *a) {
    if (c->argc < 15) c->argv[c->argc++] = (char*)a;
}
static int cmd_run(Cmd *c) {
    c->argv[c->argc] = NULL;
    pid_t pid = fork();
    if (pid < 0) { k_err("fork: %s", strerror(errno)); return -1; }
    if (pid == 0) {
        if (c->workdir && chdir(c->workdir) != 0) _exit(127);
        execvp(c->name, c->argv);
        _exit(127);
    }
    int st = 0;
    if (waitpid(pid, &st, 0) < 0) return -1;
    if (!WIFEXITED(st) || WEXITSTATUS(st) != 0) return -1;
    return 0;
}

static int mkdir_p(const char *path, mode_t mode) {
    char buf[K_MAX_PATH];
    size_t len = strlen(path);
    if (len == 0 || len >= sizeof(buf)) return -1;
    memcpy(buf, path, len + 1);
    while (len > 1 && buf[len-1] == '/') buf[--len] = '\0';
    for (char *p = buf + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(buf, mode) != 0 && errno != EEXIST) return -1;
            *p = '/';
        }
    }
    if (mkdir(buf, mode) != 0 && errno != EEXIST) return -1;
    return 0;
}

typedef enum {
    T_UNKNOWN, T_TAR, T_TAR_GZ, T_TAR_BZ2, T_TAR_XZ, T_TAR_ZST,
    T_GZ, T_BZ2, T_XZ, T_ZST, T_ZIP, T_7Z, T_RAR,
    T_LHA, T_CAB, T_DEB
} AType;

static int ends_with(const char *s, const char *suf) {
    size_t ls = strlen(s), lf = strlen(suf);
    if (lf > ls) return 0;
    return strcasecmp(s + ls - lf, suf) == 0;
}

static AType detect_type(const char *f) {
    if (ends_with(f, ".tar"))      return T_TAR;
    if (ends_with(f, ".tar.gz") || ends_with(f, ".tgz"))  return T_TAR_GZ;
    if (ends_with(f, ".tar.bz2") || ends_with(f, ".tbz2") || ends_with(f, ".tbz")) return T_TAR_BZ2;
    if (ends_with(f, ".tar.xz")  || ends_with(f, ".txz"))  return T_TAR_XZ;
    if (ends_with(f, ".tar.zst"))  return T_TAR_ZST;
    if (ends_with(f, ".gz") || ends_with(f, ".z"))  return T_GZ;
    if (ends_with(f, ".bz2"))      return T_BZ2;
    if (ends_with(f, ".xz") || ends_with(f, ".lzma")) return T_XZ;
    if (ends_with(f, ".zst"))      return T_ZST;
    if (ends_with(f, ".zip") || ends_with(f, ".jar") ||
        ends_with(f, ".war") || ends_with(f, ".apk") ||
        ends_with(f, ".zipx"))     return T_ZIP;
    if (ends_with(f, ".7z"))       return T_7Z;
    if (ends_with(f, ".rar"))      return T_RAR;
    if (ends_with(f, ".lha") || ends_with(f, ".lzh")) return T_LHA;
    if (ends_with(f, ".cab"))      return T_CAB;
    if (ends_with(f, ".deb") || ends_with(f, ".ar"))  return T_DEB;
    return T_UNKNOWN;
}

static int decompress_single(const char *archive, const char *dest, int verbose) {
    const char *base = strrchr(archive, '/');
    base = base ? base + 1 : archive;
    char outname[K_MAX_PATH];
    snprintf(outname, sizeof(outname), "%s", base);
    char *dot = strrchr(outname, '.');
    if (dot) *dot = '\0';
    if (outname[0] == '\0') snprintf(outname, sizeof(outname), "output");

    char outpath[K_MAX_PATH];
    if ((size_t)snprintf(outpath, sizeof(outpath), "%s/%s", dest, outname) >= sizeof(outpath)) { k_err("path too long"); return -1; }

    const char *tool = NULL;
    int use_lzma_format = 0;
    if (ends_with(archive, ".gz"))  tool = "gzip";
    else if (ends_with(archive, ".bz2")) tool = "bzip2";
    else if (ends_with(archive, ".xz"))  tool = "xz";
    else if (ends_with(archive, ".zst")) tool = "zstd";
    else if (ends_with(archive, ".z"))   tool = "uncompress";
    else if (ends_with(archive, ".lzma")) { tool = "xz"; use_lzma_format = 1; }
    else return -1;

    if (verbose) k_info("decompress: %s -> %s", archive, outpath);

    int fd = open(outpath, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if (fd < 0) { k_err("open %s: %s", outpath, strerror(errno)); return -1; }

    pid_t pid = fork();
    if (pid < 0) { close(fd); return -1; }
    if (pid == 0) {
        dup2(fd, STDOUT_FILENO);
        close(fd);
        if (use_lzma_format)
            execlp("xz", "xz", "-d", "-c", "--format=lzma", archive, (char*)NULL);
        else if (strcmp(tool, "zstd") == 0)
            execlp("zstd", "zstd", "-d", "-c", archive, (char*)NULL);
        else
            execlp(tool, tool, "-d", "-c", archive, (char*)NULL);
        _exit(127);
    }
    close(fd);
    int st = 0;
    waitpid(pid, &st, 0);
    if (!WIFEXITED(st) || WEXITSTATUS(st) != 0) return -1;
    return 0;
}

static int extract_tar(const char *archive, const char *dest, int verbose) {
    Cmd c; cmd_init(&c, "tar");
    cmd_arg(&c, "-xf"); cmd_arg(&c, archive);
    cmd_arg(&c, "-C");  cmd_arg(&c, dest);
    if (verbose) cmd_arg(&c, "-v");
    if (verbose) k_info("tar -xf %s -C %s", archive, dest);
    if (cmd_run(&c) != 0) { k_err("tar failed"); return -1; }
    return 0;
}

static int extract_zip(const char *archive, const char *dest, int verbose) {
    Cmd c; cmd_init(&c, "unzip");
    cmd_arg(&c, "-o"); cmd_arg(&c, "-q");
    cmd_arg(&c, archive);
    cmd_arg(&c, "-d"); cmd_arg(&c, dest);
    if (verbose) k_info("unzip %s -d %s", archive, dest);
    if (cmd_run(&c) != 0) { k_err("unzip failed"); return -1; }
    return 0;
}

static int extract_7z(const char *archive, const char *dest, int verbose) {
    Cmd c; cmd_init(&c, "7z");
    cmd_arg(&c, "x"); cmd_arg(&c, "-y");
    char opt[K_MAX_PATH + 4];
    snprintf(opt, sizeof(opt), "-o%s", dest);
    cmd_arg(&c, opt);
    cmd_arg(&c, archive);
    if (verbose) k_info("7z x %s -o%s", archive, dest);
    if (cmd_run(&c) != 0) { k_err("7z failed"); return -1; }
    return 0;
}

static int extract_rar(const char *archive, const char *dest, int verbose) {
    /* نجرب unrar، وإن فشل نستخدم 7z مع إضافة rar */
    if (access("/usr/bin/unrar", X_OK) == 0 || access("/usr/local/bin/unrar", X_OK) == 0) {
        Cmd c; cmd_init(&c, "unrar");
        cmd_arg(&c, "x"); cmd_arg(&c, "-o+"); cmd_arg(&c, "-p-");
        cmd_arg(&c, archive);
        c.workdir = dest;
        if (verbose) k_info("unrar x %s (cwd=%s)", archive, dest);
        if (cmd_run(&c) != 0) { k_err("unrar failed"); return -1; }
        return 0;
    }
    if (verbose) k_info("unrar not found, falling back to 7z");
    return extract_7z(archive, dest, verbose);
}

static int extract_lha(const char *archive, const char *dest, int verbose) {
    Cmd c; cmd_init(&c, "lha");
    cmd_arg(&c, "xf"); cmd_arg(&c, archive);
    c.workdir = dest;
    if (verbose) k_info("lha xf %s (cwd=%s)", archive, dest);
    if (cmd_run(&c) != 0) { k_err("lha failed"); return -1; }
    return 0;
}

static int extract_cab(const char *archive, const char *dest, int verbose) {
    Cmd c; cmd_init(&c, "cabextract");
    cmd_arg(&c, "-q"); cmd_arg(&c, "-d"); cmd_arg(&c, dest);
    cmd_arg(&c, archive);
    if (verbose) k_info("cabextract %s -d %s", archive, dest);
    if (cmd_run(&c) != 0) { k_err("cabextract failed"); return -1; }
    return 0;
}

static int extract_deb(const char *archive, const char *dest, int verbose) {
    Cmd c; cmd_init(&c, "ar");
    cmd_arg(&c, "x"); cmd_arg(&c, archive);
    c.workdir = dest;
    if (verbose) k_info("ar x %s (cwd=%s)", archive, dest);
    if (cmd_run(&c) != 0) { k_err("ar failed"); return -1; }
    return 0;
}

static int dispatch(const KOptions *o) {
    AType t = detect_type(o->archive);
    if (o->verbose) k_info("detected type: %d", (int)t);

    switch (t) {
    case T_TAR: case T_TAR_GZ: case T_TAR_BZ2:
    case T_TAR_XZ: case T_TAR_ZST:
        return extract_tar(o->archive, o->dest, o->verbose);
    case T_GZ: case T_BZ2: case T_XZ: case T_ZST:
        return decompress_single(o->archive, o->dest, o->verbose);
    case T_ZIP: return extract_zip(o->archive, o->dest, o->verbose);
    case T_7Z:  return extract_7z(o->archive, o->dest, o->verbose);
    case T_RAR: return extract_rar(o->archive, o->dest, o->verbose);
    case T_LHA: return extract_lha(o->archive, o->dest, o->verbose);
    case T_CAB: return extract_cab(o->archive, o->dest, o->verbose);
    case T_DEB: return extract_deb(o->archive, o->dest, o->verbose);
    default:
        k_err("unsupported format: %s", o->archive);
        return K_ERR_OPEN;
    }
}

static void list_formats(void) {
    puts("Formats handled via system tools:");
    puts("  .tar                     -> tar");
    puts("  .tar.gz / .tgz           -> tar");
    puts("  .tar.bz2 / .tbz2         -> tar");
    puts("  .tar.xz / .txz           -> tar");
    puts("  .tar.zst                 -> tar");
    puts("  .gz .bz2 .xz .zst .lzma .z -> single-file decompress");
    puts("  .zip .jar .war .apk      -> unzip");
    puts("  .7z                      -> 7z");
    puts("  .rar                     -> unrar or 7z");
    puts("  .lha .lzh                -> lha");
    puts("  .cab                     -> cabextract");
    puts("  .deb .ar                 -> ar");
}

static int parse_args(int argc, char **argv, KOptions *o) {
    memset(o, 0, sizeof(*o));
    strcpy(o->dest, ".");
    int got = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose")) o->verbose = 1;
        else if (!strcmp(argv[i], "-l") || !strcmp(argv[i], "--list-formats")) o->list_only = 1;
        else if (!strcmp(argv[i], "-C")) {
            if (i + 1 >= argc) return -1;
            snprintf(o->dest, sizeof(o->dest), "%s", argv[++i]);
        } else if (argv[i][0] == '-') return -1;
        else { if (got) return -1; snprintf(o->archive, sizeof(o->archive), "%s", argv[i]); got = 1; }
    }
    if (!o->list_only && !got) return -1;
    return 0;
}

int main(int argc, char **argv) {
    KOptions o;
    if (parse_args(argc, argv, &o) != 0) {
        fprintf(stderr, "Usage: karchiver [-v] [-C DIR] <archive>\n"
                        "       karchiver --list-formats\n");
        return K_ERR_ARGS;
    }
    if (o.list_only) { list_formats(); return K_OK; }
    if (access(o.archive, F_OK) != 0) {
        k_err("archive not found: %s", o.archive);
        return K_ERR_NOTFOUND;
    }
    if (mkdir_p(o.dest, 0755) != 0) {
        k_err("cannot create dest: %s", o.dest);
        return K_ERR_WRITE;
    }
    if (o.verbose) k_info("archive=%s dest=%s", o.archive, o.dest);
    return dispatch(&o);
}
