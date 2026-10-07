/*
 * Mitochodria SensorHub Firmware Loader
 *
 * Loads DT-described CM4 segments through the factory Trusty verification API.
 *
 * Authors:
 * worryzu <worryzu@gmail.com> @LinearTeam
 *
 * Copyright (C) 2026 Evarentha
 * SPDX-License-Identifier: MIT
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <stdarg.h>

#define CALI_PATH "/vendor/firmware/EXEC_CALIBRATE_MAG_IMAGE"

/* The kernel exports the DT-owned regions; addresses are never inferred here. */
struct load_segment { char name[32]; uint32_t base, size; };
_Static_assert(sizeof(struct load_segment) == 40, "ldinfo ABI");

static int valid_layout(const struct load_segment segments[2])
{
    return !memcmp(segments[0].name, "pm_sys\0", 7) &&
        !memcmp(segments[1].name, "cali_lib\0", 9) &&
        segments[0].size && segments[1].size &&
        (uint64_t)segments[0].base + segments[0].size == segments[1].base &&
        (uint64_t)segments[1].base + segments[1].size <= (UINT64_C(1) << 32);
}

static void load_log(const char *fmt, ...)
{
    char line[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    fprintf(stderr, "mitochodria-sensorhub-boot: %s\n", line);
}

static int read_exact_at(int fd, void *buffer, size_t size, off_t offset)
{
    uint8_t *p = buffer;
    while (size) {
        ssize_t n = pread(fd, p, size, offset);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { if (!n) errno = EIO; return -1; }
        p += n; offset += n; size -= n;
    }
    return 0;
}

/* Prepare the complete factory region before unlocking or stopping CM4.
 * Zero-fill the unused tail: factory modem_control clears it before loading.
 */
static int read_cali_image(int fd, size_t region_size, uint8_t **image, size_t *image_size)
{
    struct stat st;
    *image = NULL;
    if (fstat(fd, &st)) return -1;
    if (!S_ISREG(st.st_mode) || st.st_size <= 0 || (uint64_t)st.st_size > region_size) {
        errno = EINVAL; return -1;
    }
    uint8_t *data = calloc(1, region_size);
    if (!data) return -1;
    if (read_exact_at(fd, data, st.st_size, 0)) { free(data); return -1; }
    *image_size = st.st_size;
    *image = data;
    return 0;
}

static int command(const char *path)
{
    int fd=open(path,O_WRONLY); if(fd<0)return -1;
    int ret=write(fd,"1",1)==1?0:-1;close(fd);return ret;
}
static int write_all(int fd,const uint8_t *data,size_t size)
{
    while(size) {ssize_t n=write(fd,data,size); if(n<0&&errno==EINTR)continue;
        if(n<=0){if(!n)errno=EIO;return -1;}data+=n;size-=n;}
    return 0;
}
int main(void)
{
    uint64_t table[16]={0}; uint8_t *t=(void *)table;
    uint8_t buf[4096], footer[64];
    uint8_t *cali = NULL;
    size_t cali_bytes = 0;
    struct load_segment segments[2];
    int src=-1,dst=-1,ret=1;
    void *lib=NULL;
    setbuf(stdout,NULL);setbuf(stderr,NULL);
    alarm(40);
    int layout = open("/proc/mitochodria_cm4/ldinfo", O_RDONLY | O_CLOEXEC);
    if (layout < 0) goto out;
    int layout_ret = read_exact_at(layout, segments, sizeof(segments), 0);
    close(layout);
    if (layout_ret || !valid_layout(segments)) {
        errno = EINVAL; goto out;
    }
    src=open("/dev/block/by-name/l_pmsys",O_RDONLY);
    if(src<0)goto out;
    off_t size=lseek(src,0,SEEK_END);
    if(size < 64 || (uint64_t)(size - 64) < segments[0].size ||
       read_exact_at(src, footer, 64, size - 64) || memcmp(footer,"AVBf",4))goto out;
    int calfd = open(CALI_PATH, O_RDONLY | O_CLOEXEC);
    if (calfd < 0) { load_log("cannot open cali image errno=%d", errno); goto out; }
    int calret = read_cali_image(calfd, segments[1].size, &cali, &cali_bytes);
    close(calfd);
    if (calret) { load_log("invalid cali image errno=%d", errno); goto out; }
    lib=dlopen("/vendor/lib64/libkernelbootcp.trusty.so",RTLD_NOW);
    if(!lib)goto out;
    int (*unlock)(void *)=dlsym(lib,"kernel_bootcp_unlock_ddr");
    int (*verify)(void *)=dlsym(lib,"kernel_bootcp_verify_all");
    if(!unlock||!verify)goto out;
    ((uint32_t *)t)[28]=1;
    int rc=unlock(table);
    if(rc<0 || command("/proc/mitochodria_cm4/stop"))goto out;
    dst=open("/proc/mitochodria_cm4/pm_sys",O_WRONLY);
    if(dst<0)goto out;
    for(off_t off=0;off<segments[0].size;off+=sizeof(buf)) {
        size_t n=segments[0].size-off<sizeof(buf)?segments[0].size-off:sizeof(buf);
        if(read_exact_at(src,buf,n,off) || write_all(dst,buf,n))goto out;
    }
    close(dst);dst=-1;
    dst = open("/proc/mitochodria_cm4/cali_lib", O_WRONLY | O_CLOEXEC);
    if (dst < 0 || write_all(dst, cali, segments[1].size)) {
        load_log("cali_lib write failed errno=%d", errno);
        goto out;
    }
    close(dst); dst = -1;
    table[0]=segments[0].base;
    ((uint32_t *)t)[2]=segments[0].size;((uint32_t *)t)[3]=segments[0].size;
    memcpy(t+16,footer,64);strcpy((char *)t+80,"l_pmsys");
    if(command("/proc/mitochodria_cm4/start"))goto out;
    rc=verify(table);
    if(rc<0)goto out;
    for(int i=0;i<15;i++) {
        int fd=open("/sys/class/sprd_sensorhub/sensor_hub/version",O_RDONLY);
        ssize_t n=fd<0?-1:read(fd,buf,sizeof(buf)-1);
        if(fd>=0)close(fd);
        if(n>0) {ret=0;break;}
        sleep(1);
    }
out:
    if(dst>=0)close(dst);
    if(src>=0)close(src);
    if(lib)dlclose(lib);
    free(cali);
    if(ret)load_log("load failed errno=%d",errno);
    return ret;
}
