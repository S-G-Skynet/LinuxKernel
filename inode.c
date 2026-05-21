// SPDX-License-Identifier: GPL-2.0

#include "internal.h"

void simplefs_inode_once(void *p)
{
    struct simplefs_inode_info *ei = p;

    inode_init_once(&ei->vfs_inode);
}

struct inode *simplefs_alloc_inode(struct super_block *sb)
{
    struct simplefs_inode_info *ei;

    ei = kmem_cache_alloc(
        simplefs_inode_cachep,
        GFP_KERNEL
    );

    if (unlikely(!ei))
        return NULL;

    memset(ei, 0, sizeof(*ei));

    inode_init_once(&ei->vfs_inode);

    return &ei->vfs_inode;
}

void simplefs_free_inode(struct inode *inode)
{
    kmem_cache_free(
        simplefs_inode_cachep,
        SIMPLEFS_I(inode)
    );
}

static void simplefs_init_vfs_inode(
    struct inode *inode,
    umode_t mode)
{
    inode->i_mode = mode;

    i_uid_write(inode, 0);
    i_gid_write(inode, 0);

    simple_inode_init_ts(inode);
}

struct inode *simplefs_get_file_inode(
    struct super_block *sb,
    u32 idx)
{
    struct simplefs_sb_info *sbi = sb->s_fs_info;

    struct inode *inode;

    struct simplefs_inode_info *ei;

    unsigned long ino;

    ino = SIMPLEFS_FIRST_FILE_INO + idx;

    inode = iget_locked(sb, ino);

    if (unlikely(!inode))
        return ERR_PTR(-ENOMEM);

    if (!(inode->i_state & I_NEW))
        return inode;

    ei = SIMPLEFS_I(inode);

    ei->file_index = idx;

    ei->start_sector = simplefs_file_start(
        sbi,
        idx
    );

    ei->file_size_bytes =
        (u64)sbi->max_file_sectors *
        SIMPLEFS_BLOCK_SIZE;

    simplefs_init_vfs_inode(
        inode,
        S_IFREG | 0644
    );

    inode->i_size   = ei->file_size_bytes;
    inode->i_blocks = sbi->max_file_sectors;

    inode->i_op  = &simplefs_file_iops;
    inode->i_fop = &simplefs_file_ops;

    inode->i_private = ei;

    set_nlink(inode, 1);

    unlock_new_inode(inode);

    return inode;
}

struct inode *simplefs_get_root(struct super_block *sb)
{
    struct inode *inode;

    inode = iget_locked(sb, 1);

    if (unlikely(!inode))
        return ERR_PTR(-ENOMEM);

    if (!(inode->i_state & I_NEW))
        return inode;

    simplefs_init_vfs_inode(
        inode,
        S_IFDIR | 0755
    );

    inode->i_op  = &simplefs_dir_iops;
    inode->i_fop = &simplefs_dir_ops;

    inode->i_private = NULL;

    set_nlink(inode, 2);

    unlock_new_inode(inode);

    return inode;
}