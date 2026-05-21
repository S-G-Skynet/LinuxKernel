// SPDX-License-Identifier: GPL-2.0

#include <linux/module.h>

#include "internal.h"

static int simplefs_zero_sector(
    struct super_block *sb,
    sector_t sector)
{
    struct buffer_head *bh;

    bh = sb_bread(sb, sector);

    if (unlikely(!bh))
        return -EIO;

    memset(
        bh->b_data,
        0,
        SIMPLEFS_BLOCK_SIZE
    );

    mark_buffer_dirty(bh);

    sync_dirty_buffer(bh);

    brelse(bh);

    return 0;
}

static int simplefs_parse_file_index(
    const char *name,
    u32 *idx)
{
    if (strncmp(name, "file_", 5) != 0)
        return -EINVAL;

    return kstrtou32(name + 5, 10, idx);
}

int simplefs_ioc_zero_all(struct super_block *sb)
{
    struct simplefs_sb_info *sbi =
        sb->s_fs_info;

    u32 i;

    for (i = 0; i < sbi->num_files; i++) {

        sector_t start;

        u32 s;

        start = simplefs_file_start(
            sbi,
            i
        );

        for (s = 0;
             s < sbi->max_file_sectors;
             s++) {

            int ret;

            ret = simplefs_zero_sector(
                sb,
                start + s
            );

            if (ret)
                return ret;
        }
    }

    return 0;
}

int simplefs_ioc_erase_fs(struct super_block *sb)
{
    struct simplefs_sb_info *sbi =
        sb->s_fs_info;

    int ret;

    ret = simplefs_ioc_zero_all(sb);

    if (ret)
        return ret;

    ret = simplefs_zero_sector(
        sb,
        sbi->sb_first_offset
    );

    if (ret)
        return ret;

    ret = simplefs_zero_sector(
        sb,
        sbi->sb_second_offset
    );

    if (ret)
        return ret;

    return 0;
}

int simplefs_ioc_get_meta(
    struct super_block *sb,
    void __user *arg)
{
    struct simplefs_sb_info *sbi =
        sb->s_fs_info;

    struct simplefs_meta_list hdr;

    struct simplefs_file_meta __user *uentries;

    u32 n;
    u32 i;

    if (copy_from_user(
            &hdr,
            arg,
            sizeof(hdr)))
        return -EFAULT;

    if (unlikely(!hdr.entries_ptr))
        return -EINVAL;

    uentries =
        (struct simplefs_file_meta __user *)
        (uintptr_t)hdr.entries_ptr;

    n = min(
        hdr.max_count,
        sbi->num_files
    );

    for (i = 0; i < n; i++) {

        struct simplefs_file_meta meta;

        int ret;

        memset(
            &meta,
            0,
            sizeof(meta)
        );

        snprintf(
            meta.name,
            sizeof(meta.name),
            "file_%u",
            i
        );

        meta.offset_sector =
            simplefs_file_start(
                sbi,
                i
            );

        meta.size_bytes =
            (u64)sbi->max_file_sectors *
            SIMPLEFS_BLOCK_SIZE;

        ret = simplefs_file_hash(
            sb,
            sbi,
            i,
            &meta.content_hash
        );

        if (ret)
            return ret;

        if (copy_to_user(
                &uentries[i],
                &meta,
                sizeof(meta)))
            return -EFAULT;
    }

    hdr.count = n;

    if (copy_to_user(
            arg,
            &hdr,
            sizeof(hdr)))
        return -EFAULT;

    return 0;
}

int simplefs_ioc_get_mapping(
    struct super_block *sb,
    void __user *arg)
{
    struct simplefs_sb_info *sbi =
        sb->s_fs_info;

    struct simplefs_file_mapping mapping;

    u32 idx;

    if (copy_from_user(
            &mapping,
            arg,
            sizeof(mapping)))
        return -EFAULT;

    mapping.name[
        sizeof(mapping.name) - 1
    ] = '\0';

    if (simplefs_parse_file_index(
            mapping.name,
            &idx))
        return -ENOENT;

    if (idx >= sbi->num_files)
        return -ENOENT;

    mapping.start_sector =
        simplefs_file_start(
            sbi,
            idx
        );

    mapping.sector_count =
        sbi->max_file_sectors;

    mapping.size_bytes =
        (u64)sbi->max_file_sectors *
        SIMPLEFS_BLOCK_SIZE;

    if (copy_to_user(
            arg,
            &mapping,
            sizeof(mapping)))
        return -EFAULT;

    return 0;
}

long simplefs_ioctl(
    struct file *filp,
    unsigned int cmd,
    unsigned long arg)
{
    struct super_block *sb =
        file_inode(filp)->i_sb;

    switch (cmd) {

    case SIMPLEFS_IOC_ZERO_ALL:

        return simplefs_ioc_zero_all(sb);

    case SIMPLEFS_IOC_ERASE_FS:

        return simplefs_ioc_erase_fs(sb);

    case SIMPLEFS_IOC_GET_META:

        return simplefs_ioc_get_meta(
            sb,
            (void __user *)arg
        );

    case SIMPLEFS_IOC_GET_MAPPING:

        return simplefs_ioc_get_mapping(
            sb,
            (void __user *)arg
        );

    default:

        return -ENOTTY;
    }
}