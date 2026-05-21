// SPDX-License-Identifier: GPL-2.0

#include <linux/module.h>

#include "internal.h"

static int simplefs_read_block(
    struct super_block *sb,
    sector_t sector,
    struct buffer_head **bh_out)
{
    struct buffer_head *bh;

    bh = sb_bread(sb, sector);

    if (unlikely(!bh))
        return -EIO;

    *bh_out = bh;

    return 0;
}

static int simplefs_write_block(
    struct super_block *sb,
    sector_t sector,
    const void *data,
    size_t len)
{
    struct buffer_head *bh;

    bh = sb_bread(sb, sector);

    if (unlikely(!bh))
        return -EIO;

    memcpy(
        bh->b_data,
        data,
        len
    );

    mark_buffer_dirty(bh);

    sync_dirty_buffer(bh);

    brelse(bh);

    return 0;
}

u32 simplefs_sb_crc(
    const struct simplefs_super_block *dsb)
{
    struct simplefs_super_block tmp;

    memcpy(
        &tmp,
        dsb,
        sizeof(tmp)
    );

    tmp.crc32 = 0;

    return crc32(
        0,
        (const void *)&tmp,
        sizeof(tmp)
    );
}

sector_t simplefs_file_start(
    const struct simplefs_sb_info *sbi,
    u32 index)
{
    u64 first_chunk;

    first_chunk =
        (sbi->sb_second_offset -
         sbi->sb_first_offset - 1) /
        sbi->max_file_sectors;

    if (index < first_chunk) {

        return sbi->sb_first_offset +
               1 +
               (u64)index *
               sbi->max_file_sectors;
    }

    return sbi->sb_second_offset +
           1 +
           (u64)(index - first_chunk) *
           sbi->max_file_sectors;
}

u32 simplefs_total_files(
    const struct simplefs_sb_info *sbi)
{
    u64 first_chunk;
    u64 second_chunk = 0;

    if (unlikely(
            sbi->max_file_sectors == 0))
        return 0;

    if (sbi->sb_second_offset <=
        sbi->sb_first_offset + 1)
        return 0;

    first_chunk =
        (sbi->sb_second_offset -
         sbi->sb_first_offset - 1) /
        sbi->max_file_sectors;

    if (sbi->total_sectors >
        sbi->sb_second_offset + 1) {

        second_chunk =
            (sbi->total_sectors -
             sbi->sb_second_offset - 1) /
            sbi->max_file_sectors;
    }

    return (u32)(
        first_chunk +
        second_chunk
    );
}

int simplefs_read_sb_at(
    struct super_block *sb,
    sector_t where,
    struct simplefs_super_block *out)
{
    struct buffer_head *bh;

    int ret;

    ret = simplefs_read_block(
        sb,
        where,
        &bh
    );

    if (ret)
        return ret;

    memcpy(
        out,
        bh->b_data,
        sizeof(*out)
    );

    brelse(bh);

    return 0;
}

int simplefs_write_sb_at(
    struct super_block *sb,
    sector_t where,
    const struct simplefs_super_block *in)
{
    return simplefs_write_block(
        sb,
        where,
        in,
        sizeof(*in)
    );
}

void simplefs_build_disk_sb(
    const struct simplefs_sb_info *sbi,
    struct simplefs_super_block *dsb)
{
    memset(
        dsb,
        0,
        sizeof(*dsb)
    );

    dsb->magic =
        cpu_to_le32(SIMPLEFS_MAGIC);

    dsb->version =
        cpu_to_le32(sbi->version);

    dsb->block_size =
        cpu_to_le32(sbi->block_size);

    dsb->max_name_len =
        cpu_to_le32(sbi->max_name_len);

    dsb->max_file_sectors =
        cpu_to_le32(sbi->max_file_sectors);

    dsb->num_files =
        cpu_to_le32(sbi->num_files);

    dsb->sb_first_offset =
        cpu_to_le64(sbi->sb_first_offset);

    dsb->sb_second_offset =
        cpu_to_le64(sbi->sb_second_offset);

    dsb->total_sectors =
        cpu_to_le64(sbi->total_sectors);

    dsb->crc32 = 0;

    dsb->crc32 =
        cpu_to_le32(
            simplefs_sb_crc(dsb)
        );
}

bool simplefs_sb_valid(
    const struct simplefs_super_block *dsb)
{
    u32 expected;

    if (unlikely(
            le32_to_cpu(dsb->magic) !=
            SIMPLEFS_MAGIC))
        return false;

    expected = simplefs_sb_crc(dsb);

    return expected ==
           le32_to_cpu(dsb->crc32);
}

void simplefs_fill_info_from_disk(
    struct simplefs_sb_info *sbi,
    const struct simplefs_super_block *dsb)
{
    sbi->version =
        le32_to_cpu(dsb->version);

    sbi->block_size =
        le32_to_cpu(dsb->block_size);

    sbi->max_name_len =
        le32_to_cpu(dsb->max_name_len);

    sbi->max_file_sectors =
        le32_to_cpu(dsb->max_file_sectors);

    sbi->num_files =
        le32_to_cpu(dsb->num_files);

    sbi->sb_first_offset =
        le64_to_cpu(dsb->sb_first_offset);

    sbi->sb_second_offset =
        le64_to_cpu(dsb->sb_second_offset);

    sbi->total_sectors =
        le64_to_cpu(dsb->total_sectors);
}

int simplefs_file_hash(
    struct super_block *sb,
    const struct simplefs_sb_info *sbi,
    u32 idx,
    u32 *out)
{
    sector_t start;

    u32 sector;

    u32 crc = 0;

    start = simplefs_file_start(
        sbi,
        idx
    );

    for (sector = 0;
         sector < sbi->max_file_sectors;
         sector++) {

        struct buffer_head *bh;

        int ret;

        ret = simplefs_read_block(
            sb,
            start + sector,
            &bh
        );

        if (ret)
            return ret;

        crc = crc32(
            crc,
            bh->b_data,
            SIMPLEFS_BLOCK_SIZE
        );

        brelse(bh);
    }

    *out = crc;

    return 0;
}