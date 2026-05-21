// SPDX-License-Identifier: GPL-2.0

#include <linux/module.h>

#include "internal.h"

static int simplefs_parse_filename(
    const char *name,
    u32 *idx)
{
    if (strncmp(name, "file_", 5) != 0)
        return -EINVAL;

    return kstrtou32(name + 5, 10, idx);
}

static int simplefs_iterate(
    struct file *file,
    struct dir_context *ctx)
{
    struct inode *dir = file_inode(file);

    struct simplefs_sb_info *sbi =
        dir->i_sb->s_fs_info;

    if (!dir_emit_dots(file, ctx))
        return 0;

    while ((ctx->pos - 2) < sbi->num_files) {

        u32 idx;

        char name[48];

        int len;

        idx = (u32)(ctx->pos - 2);

        len = snprintf(
            name,
            sizeof(name),
            "file_%u",
            idx
        );

        if (unlikely(len <= 0))
            return -EINVAL;

        if (!dir_emit(
                ctx,
                name,
                len,
                SIMPLEFS_FIRST_FILE_INO + idx,
                DT_REG))
            return 0;

        ctx->pos++;
    }

    return 0;
}

static struct dentry *simplefs_lookup(
    struct inode *dir,
    struct dentry *dentry,
    unsigned int flags)
{
    struct super_block *sb = dir->i_sb;

    struct simplefs_sb_info *sbi =
        sb->s_fs_info;

    struct inode *inode = NULL;

    const char *name;

    u32 idx;

    name = dentry->d_name.name;

    if (unlikely(
            dentry->d_name.len >=
            SIMPLEFS_NAME_MAX))
        return ERR_PTR(-ENAMETOOLONG);

    if (simplefs_parse_filename(name, &idx) == 0 &&
        idx < sbi->num_files) {

        inode = simplefs_get_file_inode(
            sb,
            idx
        );

        if (IS_ERR(inode))
            return ERR_CAST(inode);
    }

    return d_splice_alias(
        inode,
        dentry
    );
}

const struct file_operations simplefs_dir_ops = {
    .owner          = THIS_MODULE,

    .read           = generic_read_dir,

    .iterate_shared = simplefs_iterate,

    .llseek         = generic_file_llseek,

    .unlocked_ioctl = simplefs_ioctl,
};

const struct inode_operations simplefs_dir_iops = {
    .lookup = simplefs_lookup,
};