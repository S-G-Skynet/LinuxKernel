#ifndef SIMPLEFS_INTERNAL_H
#define SIMPLEFS_INTERNAL_H

#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/slab.h>
#include <linux/crc32.h>
#include <linux/string.h>
#include <linux/statfs.h>
#include <linux/blkdev.h>
#include <linux/uaccess.h>
#include <linux/mount.h>
#include <linux/pagemap.h>

#include "simplefs.h"

struct simplefs_sb_info {
    u32 version;
    u32 block_size;
    u32 max_name_len;
    u32 max_file_sectors;
    u32 num_files;
    u64 sb_first_offset;
    u64 sb_second_offset;
    u64 total_sectors;
    bool erased;
};

struct simplefs_inode_info {
    sector_t start_sector;
    u64      file_size_bytes;
    u32      file_index;
    struct inode vfs_inode;
};

extern struct kmem_cache *simplefs_inode_cachep;

static inline struct simplefs_inode_info *
SIMPLEFS_I(struct inode *inode)
{
    return container_of(inode,
                        struct simplefs_inode_info,
                        vfs_inode);
}

/* utils.c */
u32 simplefs_sb_crc(const struct simplefs_super_block *dsb);

sector_t simplefs_file_start(
    const struct simplefs_sb_info *sbi,
    u32 index);

u32 simplefs_total_files(
    const struct simplefs_sb_info *sbi);

int simplefs_read_sb_at(
    struct super_block *sb,
    sector_t where,
    struct simplefs_super_block *out);

int simplefs_write_sb_at(
    struct super_block *sb,
    sector_t where,
    const struct simplefs_super_block *in);

void simplefs_build_disk_sb(
    const struct simplefs_sb_info *sbi,
    struct simplefs_super_block *dsb);

bool simplefs_sb_valid(
    const struct simplefs_super_block *dsb);

void simplefs_fill_info_from_disk(
    struct simplefs_sb_info *sbi,
    const struct simplefs_super_block *dsb);

int simplefs_file_hash(
    struct super_block *sb,
    const struct simplefs_sb_info *sbi,
    u32 idx,
    u32 *out);

/* inode.c */
struct inode *simplefs_alloc_inode(
    struct super_block *sb);

void simplefs_free_inode(
    struct inode *inode);

struct inode *simplefs_get_file_inode(
    struct super_block *sb,
    u32 idx);

struct inode *simplefs_get_root(
    struct super_block *sb);

void simplefs_inode_once(void *p);

/* file.c */
extern const struct file_operations simplefs_file_ops;
extern const struct inode_operations simplefs_file_iops;

/* dir.c */
extern const struct file_operations simplefs_dir_ops;
extern const struct inode_operations simplefs_dir_iops;

/* ioctl.c */
long simplefs_ioctl(
    struct file *filp,
    unsigned int cmd,
    unsigned long arg);

int simplefs_ioc_zero_all(
    struct super_block *sb);

int simplefs_ioc_erase_fs(
    struct super_block *sb);

int simplefs_ioc_get_meta(
    struct super_block *sb,
    void __user *arg);

int simplefs_ioc_get_mapping(
    struct super_block *sb,
    void __user *arg);

/* super.c */
extern const struct super_operations simplefs_sops;

int simplefs_fill_super(
    struct super_block *sb,
    void *data,
    int silent);

struct dentry *simplefs_mount(
    struct file_system_type *fs_type,
    int flags,
    const char *dev_name,
    void *data);

#endif