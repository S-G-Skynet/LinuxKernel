// SPDX-License-Identifier: GPL-2.0

#include <linux/module.h>

#include "internal.h"

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SimpleFS");
MODULE_VERSION("0.1");

struct kmem_cache *simplefs_inode_cachep;

static char *device_name = "";
static ulong sb_first_offset = 0;
static ulong sb_second_offset = 64;
static uint max_name_len_param = SIMPLEFS_NAME_MAX;
static uint max_file_sectors = 8;

module_param(device_name, charp, 0444);
MODULE_PARM_DESC(device_name, "Block device name");

module_param(sb_first_offset, ulong, 0444);
MODULE_PARM_DESC(sb_first_offset, "Primary superblock sector");

module_param(sb_second_offset, ulong, 0444);
MODULE_PARM_DESC(sb_second_offset, "Backup superblock sector");

module_param(max_name_len_param, uint, 0444);
MODULE_PARM_DESC(max_name_len_param, "Maximum filename length");

module_param(max_file_sectors, uint, 0444);
MODULE_PARM_DESC(max_file_sectors, "Maximum sectors per file");

static int simplefs_statfs(struct dentry *dentry, struct kstatfs *buf)
{
    struct simplefs_sb_info *sbi = dentry->d_sb->s_fs_info;

    buf->f_type    = SIMPLEFS_MAGIC;
    buf->f_bsize   = SIMPLEFS_BLOCK_SIZE;
    buf->f_blocks  = sbi->total_sectors;

    buf->f_bfree   = 0;
    buf->f_bavail  = 0;

    buf->f_files   = sbi->num_files;
    buf->f_ffree   = 0;

    buf->f_namelen = sbi->max_name_len;

    return 0;
}

static void simplefs_put_super(struct super_block *sb)
{
    kfree(sb->s_fs_info);
    sb->s_fs_info = NULL;
}

const struct super_operations simplefs_sops = {
    .alloc_inode = simplefs_alloc_inode,
    .free_inode  = simplefs_free_inode,
    .statfs      = simplefs_statfs,
    .put_super   = simplefs_put_super,
    .drop_inode  = generic_delete_inode,
};

static bool simplefs_sb_blank(const struct simplefs_super_block *dsb)
{
    return memchr_inv(dsb, 0, sizeof(*dsb)) == NULL;
}

int simplefs_fill_super(struct super_block *sb, void *data, int silent)
{
    struct simplefs_sb_info *sbi;
    struct simplefs_super_block *dsb_primary = NULL;
    struct simplefs_super_block *dsb_backup  = NULL;
    struct simplefs_super_block *dsb_new     = NULL;

    struct inode *root;

    u64 total_sectors;

    bool primary_ok;
    bool backup_ok;

    int ret;

    if (unlikely(!sb_set_blocksize(sb, SIMPLEFS_BLOCK_SIZE))) {
        pr_err("simplefs: failed to set block size\n");
        return -EINVAL;
    }

    sb->s_magic     = SIMPLEFS_MAGIC;
    sb->s_op        = &simplefs_sops;
    sb->s_maxbytes  = MAX_LFS_FILESIZE;
    sb->s_time_gran = 1;

    sbi = kzalloc(sizeof(*sbi), GFP_KERNEL);
    if (!sbi)
        return -ENOMEM;

    sb->s_fs_info = sbi;

    dsb_primary = kzalloc(sizeof(*dsb_primary), GFP_KERNEL);
    dsb_backup  = kzalloc(sizeof(*dsb_backup), GFP_KERNEL);
    dsb_new     = kzalloc(sizeof(*dsb_new), GFP_KERNEL);

    if (!dsb_primary || !dsb_backup || !dsb_new) {
        ret = -ENOMEM;
        goto err;
    }

    total_sectors = bdev_nr_sectors(sb->s_bdev);

    ret = simplefs_read_sb_at(sb, sb_first_offset, dsb_primary);
    if (ret)
        goto err;

    ret = simplefs_read_sb_at(sb, sb_second_offset, dsb_backup);
    if (ret)
        goto err;

    primary_ok = simplefs_sb_valid(dsb_primary);
    backup_ok  = simplefs_sb_valid(dsb_backup);

    if (primary_ok) {

        simplefs_fill_info_from_disk(sbi, dsb_primary);

        pr_info("simplefs: primary superblock valid\n");

        if (!backup_ok) {

            pr_warn("simplefs: restoring backup superblock\n");

            simplefs_build_disk_sb(sbi, dsb_new);

            ret = simplefs_write_sb_at(
                sb,
                sbi->sb_second_offset,
                dsb_new
            );

            if (ret)
                goto err;
        }

    } else if (backup_ok) {

        simplefs_fill_info_from_disk(sbi, dsb_backup);

        pr_warn("simplefs: primary superblock corrupted\n");
        pr_warn("simplefs: restoring primary from backup\n");

        simplefs_build_disk_sb(sbi, dsb_new);

        ret = simplefs_write_sb_at(
            sb,
            sbi->sb_first_offset,
            dsb_new
        );

        if (ret)
            goto err;

    } else if (simplefs_sb_blank(dsb_primary) &&
               simplefs_sb_blank(dsb_backup)) {

        pr_info("simplefs: no valid superblock found\n");
        pr_info("simplefs: creating new filesystem\n");

        sbi->version          = SIMPLEFS_VERSION;
        sbi->block_size       = SIMPLEFS_BLOCK_SIZE;
        sbi->max_name_len     = max_name_len_param;
        sbi->max_file_sectors = max_file_sectors;
        sbi->sb_first_offset  = sb_first_offset;
        sbi->sb_second_offset = sb_second_offset;
        sbi->total_sectors    = total_sectors;
        sbi->num_files        = simplefs_total_files(sbi);

        simplefs_build_disk_sb(sbi, dsb_new);

        ret = simplefs_write_sb_at(sb, sbi->sb_first_offset, dsb_new);
        if (ret)
            goto err;

        ret = simplefs_write_sb_at(sb, sbi->sb_second_offset, dsb_new);
        if (ret)
            goto err;

        pr_info(
            "simplefs: initialized %u files "
            "(max sectors per file=%u)\n",
            sbi->num_files,
            sbi->max_file_sectors
        );

    } else {

        pr_err("simplefs: no valid superblock found\n");
        pr_err("simplefs: filesystem is corrupted\n");

        ret = -EUCLEAN;
        goto err;
    }

    root = simplefs_get_root(sb);

    if (IS_ERR(root)) {
        ret = PTR_ERR(root);
        goto err;
    }

    sb->s_root = d_make_root(root);

    if (!sb->s_root) {
        ret = -ENOMEM;
        goto err;
    }

    pr_info(
        "simplefs: mounted "
        "(files=%u, sb1=%llu, sb2=%llu)\n",
        sbi->num_files,
        sbi->sb_first_offset,
        sbi->sb_second_offset
    );

    kfree(dsb_primary);
    kfree(dsb_backup);
    kfree(dsb_new);

    return 0;

err:

    kfree(dsb_primary);
    kfree(dsb_backup);
    kfree(dsb_new);

    kfree(sbi);

    sb->s_fs_info = NULL;

    return ret;
}

struct dentry *simplefs_mount(
    struct file_system_type *fs_type,
    int flags,
    const char *dev_name,
    void *data)
{
    return mount_bdev(
        fs_type,
        flags,
        dev_name,
        data,
        simplefs_fill_super
    );
}

static struct file_system_type simplefs_type = {
    .owner    = THIS_MODULE,
    .name     = "simplefs",
    .mount    = simplefs_mount,
    .kill_sb  = kill_block_super,
    .fs_flags = FS_REQUIRES_DEV,
};

static int __init simplefs_init(void)
{
    int ret;

    simplefs_inode_cachep = kmem_cache_create(
        "simplefs_inode_cache",
        sizeof(struct simplefs_inode_info),
        0,
        SLAB_RECLAIM_ACCOUNT | SLAB_ACCOUNT,
        simplefs_inode_once
    );

    if (!simplefs_inode_cachep)
        return -ENOMEM;

    ret = register_filesystem(&simplefs_type);

    if (ret) {
        kmem_cache_destroy(simplefs_inode_cachep);
        return ret;
    }

    pr_info("simplefs: filesystem registered\n");

    return 0;
}

static void __exit simplefs_exit(void)
{
    unregister_filesystem(&simplefs_type);

    rcu_barrier();

    kmem_cache_destroy(simplefs_inode_cachep);

    pr_info("simplefs: filesystem unregistered\n");
}

module_init(simplefs_init);
module_exit(simplefs_exit);