#include "lvgl.h"
#include "lvfs_fatfs.h"
#include "ff.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

static void make_path(char * dst, size_t dst_size, char letter, const char * path)
{
    // path may start with '/' or not; ensure colon after letter
    if(path[0] == '/') {
        snprintf(dst, dst_size, "%c:%s", letter, path);
    } else {
        snprintf(dst, dst_size, "%c:/%s", letter, path);
    }
}

static void * fs_open(lv_fs_drv_t * drv, const char * path, lv_fs_mode_t mode)
{
    char full_path[256];
    make_path(full_path, sizeof(full_path), drv->letter, path);

    FIL * f = lv_malloc(sizeof(FIL));
    if(!f) {
        return NULL;
    }

    BYTE fat_mode = 0;
    if(mode & LV_FS_MODE_WR) fat_mode |= FA_WRITE | FA_OPEN_ALWAYS;
    if(mode & LV_FS_MODE_RD) fat_mode |= FA_READ;

    if(f_open(f, full_path, fat_mode) != FR_OK) {
        lv_free(f);
        return NULL;
    }
    if(mode & LV_FS_MODE_WR) {
        f_lseek(f, f_size(f));
    }
    return f;
}

static lv_fs_res_t fs_read(lv_fs_drv_t * drv, void * file_p, void * buf, uint32_t btr, uint32_t * br)
{
    UINT br_tmp = 0;
    FRESULT res = f_read((FIL *)file_p, buf, btr, &br_tmp);
    if(br) *br = br_tmp;
    return res == FR_OK ? LV_FS_RES_OK : LV_FS_RES_FS_ERR;
}

static lv_fs_res_t fs_close(lv_fs_drv_t * drv, void * file_p)
{
    (void)drv;
    FRESULT res = f_close((FIL *)file_p);
    lv_free(file_p);
    return res == FR_OK ? LV_FS_RES_OK : LV_FS_RES_FS_ERR;
}

static lv_fs_res_t fs_seek(lv_fs_drv_t *drv, void *file_p, int32_t offset, lv_fs_whence_t whence)
{
    (void)drv;

    FIL *f = (FIL *)file_p;
    int64_t target = 0;

    switch(whence) {
        case LV_FS_SEEK_SET:
            target = offset;
            break;
        case LV_FS_SEEK_CUR:
            target = (int64_t)f_tell(f) + offset;
            break;
        case LV_FS_SEEK_END:
            target = (int64_t)f_size(f) + offset;
            break;
        default:
            return LV_FS_RES_INV_PARAM;
    }

    if(target < 0) {
        return LV_FS_RES_INV_PARAM;
    }

    FRESULT res = f_lseek(f, (FSIZE_t)target);
    return res == FR_OK ? LV_FS_RES_OK : LV_FS_RES_FS_ERR;
}

static lv_fs_res_t fs_tell(lv_fs_drv_t * drv, void * file_p, uint32_t * pos)
{
    (void)drv;
    if(pos) *pos = f_tell((FIL *)file_p);
    return LV_FS_RES_OK;
}

static void * fs_dir_open(lv_fs_drv_t * drv, const char * path)
{
    char full_path[256];
    make_path(full_path, sizeof(full_path), drv->letter, path);

    FF_DIR * dir = lv_malloc(sizeof(FF_DIR));
    if(!dir) {
        return NULL;
    }
    if(f_opendir(dir, full_path) != FR_OK) {
        lv_free(dir);
        return NULL;
    }
    return dir;
}

static lv_fs_res_t fs_dir_read(lv_fs_drv_t * drv, void * dir_p, char * fn, uint32_t size)
{
    (void)drv;
    FILINFO info;
    FRESULT res = f_readdir((FF_DIR *)dir_p, &info);
    if(res != FR_OK) return LV_FS_RES_FS_ERR;
    if(info.fname[0] == '\0') {
        if(size > 0) fn[0] = '\0';
    } else {
        if(size > 0) {
            strncpy(fn, info.fname, size - 1);
            fn[size - 1] = '\0';
        }
    }
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_dir_close(lv_fs_drv_t * drv, void * dir_p)
{
    (void)drv;
    FRESULT res = f_closedir((FF_DIR *)dir_p);
    lv_free(dir_p);
    return res == FR_OK ? LV_FS_RES_OK : LV_FS_RES_FS_ERR;
}

void lvfs_fatfs_register(char letter)
{
    static lv_fs_drv_t drv;
    lv_fs_drv_init(&drv);

    drv.letter = letter;
    drv.open_cb = fs_open;
    drv.read_cb = fs_read;
    drv.close_cb = fs_close;
    drv.seek_cb = fs_seek;
    drv.tell_cb = fs_tell;
    drv.dir_open_cb = fs_dir_open;
    drv.dir_read_cb = fs_dir_read;
    drv.dir_close_cb = fs_dir_close;

    lv_fs_drv_register(&drv);
}

