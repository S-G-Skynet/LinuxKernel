// SPDX-License-Identifier: GPL-2.0

#include <linux/module.h>

#include "internal.h"

static int simplefs_access_sector(
    struct super_block *sb,
    sector_t sector,
    void **data,
    struct buffer_head **bh_out)
{
    struct buffer_head *bh;

    bh = sb_bread(sb, sector);

    if (unlikely(!bh))
        return -EIO;

    *data = bh->b_data;
    *bh_out = bh;

    return 0;
}

static ssize_t simplefs_do_rw(
    struct file *filp,
    char __user *ubuf,
    size_t len,
    loff_t *ppos,
    bool write)
{
    struct inode *inode = file_inode(filp);

    struct simplefs_inode_info *ei =
        SIMPLEFS_I(inode);

    struct super_block *sb = inode->i_sb;

    loff_t pos = *ppos;

    size_t processed = 0;

    if (unlikely(pos < 0))
        return -EINVAL;

    if (pos >= ei->file_size_bytes)
        return write ? -ENOSPC : 0;

    if (pos + len > ei->file_size_bytes)
        len = ei->file_size_bytes - pos;

    while (len > 0) {

        sector_t sector;

        size_t offset;
        size_t chunk;

        struct buffer_head *bh;

        void *data;

        int ret;

        sector = ei->start_sector +
                 (pos / SIMPLEFS_BLOCK_SIZE);

        offset = pos % SIMPLEFS_BLOCK_SIZE;

        chunk = min_t(
            size_t,
            len,
            SIMPLEFS_BLOCK_SIZE - offset
        );

        ret = simplefs_access_sector(
            sb,
            sector,
            &data,
            &bh
        );

        if (ret)
            return processed ? processed : ret;

        if (write) {

            if (copy_from_user(
                    data + offset,
                    ubuf + processed,
                    chunk)) {

                brelse(bh);

                return processed ?
                    processed : -EFAULT;
            }

            mark_buffer_dirty(bh);

            sync_dirty_buffer(bh);

        } else {

            if (copy_to_user(
                    ubuf + processed,
                    data + offset,
                    chunk)) {

                brelse(bh);

                return processed ?
                    processed : -EFAULT;
            }
        }

        brelse(bh);

        processed += chunk;

        pos += chunk;

        len -= chunk;
    }

    *ppos = pos;

    if (write) {
        
        inode_set_ctime_current(inode);

        mark_inode_dirty(inode);
    }

    return processed;
}

static ssize_t simplefs_read(
    struct file *filp,
    char __user *buf,
    size_t len,
    loff_t *ppos)
{
    return simplefs_do_rw(
        filp,
        buf,
        len,
        ppos,
        false
    );
}

static ssize_t simplefs_write(
    struct file *filp,
    const char __user *buf,
    size_t len,
    loff_t *ppos)
{
    return simplefs_do_rw(
        filp,
        (char __user *)buf,
        len,
        ppos,
        true
    );
}

static loff_t simplefs_llseek(
    struct file *filp,
    loff_t off,
    int whence)
{
    struct inode *inode = file_inode(filp);

    return generic_file_llseek_size(
        filp,
        off,
        whence,
        MAX_LFS_FILESIZE,
        inode->i_size
    );
}

static int simplefs_setattr(
    struct mnt_idmap *idmap,
    struct dentry *dentry,
    struct iattr *iattr)
{
    struct inode *inode = d_inode(dentry);

    int ret;

    /*
     * File size changes are not supported.
     */
    iattr->ia_valid &= ~ATTR_SIZE;

    ret = setattr_prepare(
        idmap,
        dentry,
        iattr
    );

    if (ret)
        return ret;

    setattr_copy(
        idmap,
        inode,
        iattr
    );

    mark_inode_dirty(inode);

    return 0;
}

const struct file_operations simplefs_file_ops = {
    .owner          = THIS_MODULE,

    .llseek         = simplefs_llseek,

    .read           = simplefs_read,
    .write          = simplefs_write,

    .unlocked_ioctl = simplefs_ioctl,
    .compat_ioctl   = simplefs_ioctl,
};

const struct inode_operations simplefs_file_iops = {
    .setattr = simplefs_setattr,
};