/*
 * Debugfs support for hosts and cards
 *
 * Copyright (C) 2008 Atmel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/moduleparam.h>
#include <linux/export.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/fault-inject.h>

#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>

#include <linux/scatterlist.h>

#include "core.h"
#include "mmc_ops.h"

#ifdef CONFIG_FAIL_MMC_REQUEST

static DECLARE_FAULT_ATTR(fail_default_attr);
static char *fail_request;
module_param(fail_request, charp, 0);

#endif /* CONFIG_FAIL_MMC_REQUEST */

/* The debugfs functions are optimized away when CONFIG_DEBUG_FS isn't set. */
/*Controller SRAM Dump CMD for debugging Samsung EMMC beginning*/
#define READ_SRAM_COUNT 128

static int mmc_vendor_cmd62(struct mmc_host *host, u32 arg)
{
	struct mmc_command cmd;
	int err;

	BUG_ON(!host);

	memset(&cmd, 0, sizeof(struct mmc_command));

	cmd.opcode = 62;
	cmd.arg = arg;
	cmd.flags = MMC_RSP_R1B;
	err = mmc_wait_for_cmd(host, &cmd, MMC_CMD_RETRIES);

	if (err)
		return err;

	return 0;
}

static int mmc_set_blocksize(struct mmc_host *host, u32 arg)
{
        struct mmc_command cmd;
        int err;

        BUG_ON(!host);

        memset(&cmd, 0, sizeof(struct mmc_command));

        cmd.opcode = MMC_SET_BLOCK_COUNT;
        cmd.arg = arg;
        cmd.flags = MMC_RSP_R1 | MMC_CMD_ADTC;
        err = mmc_wait_for_cmd(host, &cmd, MMC_CMD_RETRIES);

        if (err)
                return err;

        return 0;
}

static int mmc_sram_dump_read(struct mmc_host *host, int read_cnt, u8 *buf)
{
	struct mmc_request mrq;
	struct mmc_command cmd;
	struct mmc_command stop;
	struct mmc_data data;
	struct scatterlist sg;
	int len = 0;


        BUG_ON(!host);
        BUG_ON(!buf);

	len = read_cnt * 512;


	memset(&mrq, 0, sizeof(struct mmc_request));
	memset(&cmd, 0, sizeof(struct mmc_command));
	memset(&data, 0, sizeof(struct mmc_data));
	memset(&stop, 0, sizeof(struct mmc_command));

	mrq.cmd = &cmd;
	mrq.data = &data;
	mrq.stop = &stop;

	sg_init_one(&sg, buf, len);

	cmd.opcode = MMC_READ_MULTIPLE_BLOCK;
	cmd.arg = 0;
	cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC;

	data.blksz = 512;
	data.blocks = read_cnt;
	data.flags = MMC_DATA_READ;
	data.sg = &sg;
	data.sg_len = 1;

	stop.opcode = MMC_STOP_TRANSMISSION;
	stop.arg = 0;
	stop.flags = MMC_RSP_SPI_R1B | MMC_RSP_R1B | MMC_CMD_AC;

	mmc_set_data_timeout(&data, host->card);

	mmc_wait_for_req(host, &mrq);


	if (cmd.error)
		return cmd.error;
	if (data.error)
		return data.error;

	return 0;
}


static int mmc_erase_start(struct mmc_host *host, u32 arg)
{
	struct mmc_command cmd;
        int err;

        BUG_ON(!host);

        memset(&cmd, 0, sizeof(struct mmc_command));

        cmd.opcode = MMC_ERASE_GROUP_START;
        cmd.arg = arg;
        cmd.flags = MMC_RSP_R1 | MMC_CMD_AC;
        err = mmc_wait_for_cmd(host, &cmd, MMC_CMD_RETRIES);

        if (err)
                return err;

        return 0;
}


static int mmc_erase_end(struct mmc_host *host, u32 arg)
{
	struct mmc_command cmd;

	int err;

        BUG_ON(!host);

        memset(&cmd, 0, sizeof(struct mmc_command));

        cmd.opcode = MMC_ERASE_GROUP_END;
        cmd.arg = arg;
        cmd.flags = MMC_RSP_R1 | MMC_CMD_AC;
        err = mmc_wait_for_cmd(host, &cmd, MMC_CMD_RETRIES);

        if (err)
                return err;

        return 0;
}

static int mmc_erase_new(struct mmc_host *host, u32 arg)
{
        struct mmc_command cmd;

        int err;

        BUG_ON(!host);

        memset(&cmd, 0, sizeof(struct mmc_command));

        cmd.opcode = MMC_ERASE;
        cmd.arg = arg;
        cmd.flags = MMC_RSP_R1B | MMC_CMD_AC;
        err = mmc_wait_for_cmd(host, &cmd, MMC_CMD_RETRIES);

        if (err)
                return err;

        return 0;
}

static int mmc_get_sram_dump(struct mmc_card *card,u8 *buf)
{
	int err;
	const int read_cnt = READ_SRAM_COUNT;

	struct mmc_host *host = card->host;
	struct device *dev = (struct device *)&(card->dev);

	BUG_ON(!card);
	BUG_ON(!buf);

/*CMD62 0XEFAC62EC enter into vendor mode*/
	err =  mmc_vendor_cmd62(host,0xEFAC62EC);
	if(err){
		dev_info(dev, "mmc_get_sram_dump error step1 \n");
		return err;
	}
/*CMD62 0X10210002 SRAM DUMP FEATURE SELECTED*/
	err =  mmc_vendor_cmd62(host,0x10210002);
        if(err){
                dev_info(dev, "mmc_get_sram_dump error step2\n");
                return err;
        }

/*CMD35 0X0004000 DUMP START ADDRESS*/
	err =  mmc_erase_start(host,0x40000);
        if(err){
                dev_info(dev, "mmc_get_sram_dump error step3\n");
                return err;
        }

/*CMD36 0X0001000 DUMP SIZE,64K BYTES*/
	err =  mmc_erase_end(host,0x10000);
        if(err){
                dev_info(dev, "mmc_get_sram_dump error step4\n");
                return err;
        }

/*CMD38 0X0000000*/
	err =  mmc_erase_new(host,0x0);
        if(err){
                dev_info(dev, "mmc_get_sram_dump error step5\n");
                return err;
        }


/*CMD23 0X00000080 128 SECTOR*/
	err =  mmc_set_blocksize(host,0x80);
        if(err){
                dev_info(dev, "mmc_get_sram_dump error step6\n");
                return err;
        }

/*CMD18 0X00000000 READ SRAM DUMP DATA*/
	err = mmc_sram_dump_read(host, read_cnt, buf);
	if(err){
                dev_info(dev,  "mmc_get_sram_dump error step7\n");
		/*buff*/
                return err;
        }


/*CMM62 0XEFAC62EC*/
	err =  mmc_vendor_cmd62(host,0xEFAC62EC);
        if(err){
                dev_info(dev, "mmc_get_sram_dump error step8\n");
                return err;
        }

/*CMD62 0X00DECCEE EXIT VENDOR MODE*/
	err =  mmc_vendor_cmd62(host,0x00DECCEE);
	if(err){
                dev_info(dev, "mmc_get_sram_dump error step9\n");
                return err;
        }

	return err;
}

static int mmc_sram_open(struct inode *inode, struct file *filp)
{
	struct mmc_card *card = inode->i_private;
	char *buf;
	ssize_t len = 0;
	u8 *sram_dump;
	int err = 0;

	len = 512*READ_SRAM_COUNT;

	sram_dump = kmalloc(len, GFP_KERNEL);
	if (!sram_dump) {
		err = -ENOMEM;
		return err;
	}

	mmc_claim_host(card->host);
	err = mmc_get_sram_dump(card, sram_dump);
	mmc_release_host(card->host);
	if (err)
		goto out_free;

	buf = (char *)sram_dump;

	filp->private_data = buf;
	return 0;

out_free:
	kfree(sram_dump);
	return err;
}

static ssize_t mmc_sram_read(struct file *filp, char __user *ubuf,
				size_t cnt, loff_t *ppos)
{
	char *buf = filp->private_data;

	return simple_read_from_buffer(ubuf, cnt, ppos,
				       buf, 512*READ_SRAM_COUNT);
}

static int mmc_sram_release(struct inode *inode, struct file *file)
{
	kfree(file->private_data);
	return 0;
}

static const struct file_operations mmc_dbg_sram_fops = {
        .open           = mmc_sram_open,
        .read           = mmc_sram_read,
        .release        = mmc_sram_release,
};

/*Controller SRAM Dump CMD for debugging Samsung EMMC ending*/

/*Samsung Emmc for PA17 PATCH workaround modification beginning*/
int mmc_samsung_p17_apply(struct mmc_card *card)
{
	int err = 0;

	struct mmc_host *host = card->host;
	struct device *dev = (struct device *)&(card->dev);

	BUG_ON(!card);
	mmc_claim_host(host);
/*CMD62 0XEFAC62EC enter into vendor mode*/
	err =  mmc_vendor_cmd62(host,0xEFAC62EC);
	if(err){
		dev_info(dev, "Emmc_change_cmd_seq error step1\n");
		goto out;
	}
/*CMD62 0X10210000 CMD MODE*/
	err =  mmc_vendor_cmd62(host,0x10210000);
        if(err){
                dev_info(dev, "Emmc_change_cmd_seq  error step2\n");
                goto out;
        }

/*CMD35 0X00037994 ADDRESS*/
	err =  mmc_erase_start(host,0x37994);
        if(err){
                dev_info(dev, "Emmc_change_cmd_seq  error step3\n");
                goto out;
        }

/*CMD36 0X28FF5C08 value*/
	err =  mmc_erase_end(host,0x28FF5C08);
        if(err){
                dev_info(dev, "Emmc_change_cmd_seq  error step4\n");
                goto out;
        }

/*CMD38 0X0000000*/
	err =  mmc_erase_new(host,0x0);
        if(err){
                dev_info(dev, "Emmc_change_cmd_seq  error step5\n");
                goto out;
        }

/*CMM62 0XEFAC62EC*/
	err =  mmc_vendor_cmd62(host,0xEFAC62EC);
        if(err){
                dev_info(dev, "Emmc_change_cmd_seq  error step6\n");
                goto out;
        }

/*CMD62 0X00DECCEE EXIT VENDOR MODE*/
	err =  mmc_vendor_cmd62(host,0x00DECCEE);
	if(err){
                dev_info(dev, "Emmc_change_cmd_seq  error step7\n");
                goto out;
        }

	dev_info(dev, "Applied the Samsung P17 corruption fix\n");

out:
	mmc_release_host(host);
	return err;
}

/*Samsung Emmc for PA17 PATCH workaround modification ending*/

/*Samsung Emmc for PA17 PATCH workaround verify beginning*/
static int mmc_read_single_block(struct mmc_host *host, u8 *buf)
{
	struct mmc_request mrq;
        struct mmc_command cmd;
        struct mmc_data data;
	struct scatterlist sg;
        int len = 512;


        BUG_ON(!host);
        BUG_ON(!buf);


        memset(&mrq, 0, sizeof(struct mmc_request));
        memset(&cmd, 0, sizeof(struct mmc_command));
        memset(&data, 0, sizeof(struct mmc_data));

        mrq.cmd = &cmd;
        mrq.data = &data;
	sg_init_one(&sg, buf, len);


        cmd.opcode = MMC_READ_SINGLE_BLOCK;
        cmd.arg = 0;
        cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC;

        data.blksz = 512;
        data.blocks = 1;
        data.flags = MMC_DATA_READ;
	data.sg = &sg;
	data.sg_len = 1;


        mmc_set_data_timeout(&data, host->card);

        mmc_wait_for_req(host, &mrq);

	if (cmd.error)
		return cmd.error;
	if (data.error)
		return data.error;

        return 0;
}

static int mmc_read_samsung_p17_data(struct mmc_card *card,u8 *buf)
{
	int err;

	struct mmc_host *host = card->host;
	struct device *dev = &(card->dev);

	BUG_ON(!card);
	BUG_ON(!buf);
/*CMD62 0XEFAC62EC enter into vendor mode*/
	err =  mmc_vendor_cmd62(host,0xEFAC62EC);
	if(err){
		dev_info(dev, "mmc_read_cmd_seq error step1\n");
		return err;
	}
/*CMD62 0X10210002 SRAM DUMP FEATURE SELECTED*/
	err =  mmc_vendor_cmd62(host,0x10210002);
        if(err){
                dev_info(dev, "mmc_read_cmd_seq  error step2\n");
                return err;
        }

/*CMD35 0X00037994  ADDRESS*/
	err =  mmc_erase_start(host,0x37994);
        if(err){
                dev_info(dev, "mmc_read_cmd_seq  error step3\n");
                return err;
        }

/*CMD36 0X0000004  SIZE,4 BYTES*/
	err =  mmc_erase_end(host,0x4);
        if(err){
                dev_info(dev, "mmc_read_cmd_seq  error step4\n");
                return err;
        }

/*CMD38 0X0000000*/
	err =  mmc_erase_new(host,0x0);
        if(err){
                dev_info(dev, "mmc_read_cmd_seq  error step5\n");
                return err;
        }



/*CMD17 0X00000000 READ sector DATA*/
	err =  mmc_read_single_block(host, buf);
	if(err){
                dev_info(dev, "mmc_read_cmd_seq  error step6\n");
		/*buff*/
                return err;
        }


/*CMM62 0XEFAC62EC*/
	err =  mmc_vendor_cmd62(host,0xEFAC62EC);
        if(err){
                dev_info(dev, "mmc_read_cmd_seq error step7\n");
                return err;
        }

/*CMD62 0X00DECCEE EXIT VENDOR MODE*/
	err =  mmc_vendor_cmd62(host,0x00DECCEE);
	if(err){
                dev_info(dev, "mmc_read_cmd_seq  error step8\n");
                return err;
        }

	return err;
}

int mmc_samsung_p17_verify(struct mmc_card *card)
{
	ssize_t len = 512;
	u8 *cmdseq_data;
	int err = 0;

	struct mmc_host *host = card->host;
	struct device *dev = &(card->dev);

	cmdseq_data = kmalloc(len, GFP_KERNEL);
	if (!cmdseq_data) {
		err = -ENOMEM;
		goto out_free;
	}

	mmc_claim_host(host);
	err = mmc_read_samsung_p17_data(card, cmdseq_data);
	mmc_release_host(host);
	if (err)
		goto out_free;

	if (cmdseq_data[2] == 0xff){
		err = CMD_SEQ_MOD;
                dev_info(dev, "Samsung P17 firmware patch is applied\n");
	}
	else if (cmdseq_data[2] == 0x20){
		err = CMD_SEQ_ORIGINAL;
                dev_info(dev, "Samsung P17 firmware patch is NOT applied\n");
	}

out_free:
	kfree(cmdseq_data);
	return err;
}
/*Samsung Emmc for PA17 PATCH workaround verify ending*/

static int mmc_ios_show(struct seq_file *s, void *data)
{
	static const char *vdd_str[] = {
		[8]	= "2.0",
		[9]	= "2.1",
		[10]	= "2.2",
		[11]	= "2.3",
		[12]	= "2.4",
		[13]	= "2.5",
		[14]	= "2.6",
		[15]	= "2.7",
		[16]	= "2.8",
		[17]	= "2.9",
		[18]	= "3.0",
		[19]	= "3.1",
		[20]	= "3.2",
		[21]	= "3.3",
		[22]	= "3.4",
		[23]	= "3.5",
		[24]	= "3.6",
	};
	struct mmc_host	*host = s->private;
	struct mmc_ios	*ios = &host->ios;
	const char *str;
	struct mmc_card *card = host->card;
	int resul = 0;

	seq_printf(s, "clock:\t\t%u Hz\n", ios->clock);
	if (host->actual_clock)
		seq_printf(s, "actual clock:\t%u Hz\n", host->actual_clock);
	seq_printf(s, "vdd:\t\t%u ", ios->vdd);
	if ((1 << ios->vdd) & MMC_VDD_165_195)
		seq_printf(s, "(1.65 - 1.95 V)\n");
	else if (ios->vdd < (ARRAY_SIZE(vdd_str) - 1)
			&& vdd_str[ios->vdd] && vdd_str[ios->vdd + 1])
		seq_printf(s, "(%s ~ %s V)\n", vdd_str[ios->vdd],
				vdd_str[ios->vdd + 1]);
	else
		seq_printf(s, "(invalid)\n");

	switch (ios->bus_mode) {
	case MMC_BUSMODE_OPENDRAIN:
		str = "open drain";
		break;
	case MMC_BUSMODE_PUSHPULL:
		str = "push-pull";
		break;
	default:
		str = "invalid";
		break;
	}
	seq_printf(s, "bus mode:\t%u (%s)\n", ios->bus_mode, str);

	switch (ios->chip_select) {
	case MMC_CS_DONTCARE:
		str = "don't care";
		break;
	case MMC_CS_HIGH:
		str = "active high";
		break;
	case MMC_CS_LOW:
		str = "active low";
		break;
	default:
		str = "invalid";
		break;
	}
	seq_printf(s, "chip select:\t%u (%s)\n", ios->chip_select, str);

	switch (ios->power_mode) {
	case MMC_POWER_OFF:
		str = "off";
		break;
	case MMC_POWER_UP:
		str = "up";
		break;
	case MMC_POWER_ON:
		str = "on";
		break;
	default:
		str = "invalid";
		break;
	}
	seq_printf(s, "power mode:\t%u (%s)\n", ios->power_mode, str);
	seq_printf(s, "bus width:\t%u (%u bits)\n",
			ios->bus_width, 1 << ios->bus_width);

	switch (ios->timing) {
	case MMC_TIMING_LEGACY:
		str = "legacy";
		break;
	case MMC_TIMING_MMC_HS:
		str = "mmc high-speed";
		break;
	case MMC_TIMING_SD_HS:
		str = "sd high-speed";
		break;
	case MMC_TIMING_UHS_SDR50:
		str = "sd uhs SDR50";
		break;
	case MMC_TIMING_UHS_SDR104:
		str = "sd uhs SDR104";
		break;
	case MMC_TIMING_UHS_DDR50:
		str = "sd uhs DDR50";
		break;
	case MMC_TIMING_MMC_HS200:
		str = "mmc high-speed SDR200";
		break;
	default:
		str = "invalid";
		break;
	}
	seq_printf(s, "timing spec:\t%u (%s)\n", ios->timing, str);

	/*verify Samsung Vendor CMD sequnce beginning*/
	if (card){
		if ((mmc_card_mmc(card))
			&& (host->index == 0)
			&& (card->cid.manfid == MMC_SAMSUNG_MANFID)){
			resul =  mmc_samsung_p17_verify(card);
		}

		switch (resul) {
                        case CMD_SEQ_MOD:
                                str = "modified";
                                break;
                        case CMD_SEQ_ORIGINAL:
                                str = "original";
                                break;
                        case NO_SAMSUNG:
                                str = "no Samsung Emmc";
                                break;
                        default:
                                str = "invalid";
                                break;
                }
		seq_printf(s, "Samsung P17 MMC patch:\t%u (%s)\n", resul, str);
	}
	/*verify Samsung Vendor CMD sequnce ending*/

	return 0;
}

static int mmc_ios_open(struct inode *inode, struct file *file)
{
	return single_open(file, mmc_ios_show, inode->i_private);
}

static const struct file_operations mmc_ios_fops = {
	.open		= mmc_ios_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int mmc_clock_opt_get(void *data, u64 *val)
{
	struct mmc_host *host = data;

	*val = host->ios.clock;

	return 0;
}

static int mmc_clock_opt_set(void *data, u64 val)
{
	struct mmc_host *host = data;

	/* We need this check due to input value is u64 */
	if (val > host->f_max)
		return -EINVAL;

	mmc_claim_host(host);
	mmc_set_clock(host, (unsigned int) val);
	mmc_release_host(host);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(mmc_clock_fops, mmc_clock_opt_get, mmc_clock_opt_set,
	"%llu\n");

void mmc_add_host_debugfs(struct mmc_host *host)
{
	struct dentry *root;

	root = debugfs_create_dir(mmc_hostname(host), NULL);
	if (IS_ERR(root))
		/* Don't complain -- debugfs just isn't enabled */
		return;
	if (!root)
		/* Complain -- debugfs is enabled, but it failed to
		 * create the directory. */
		goto err_root;

	host->debugfs_root = root;

	if (!debugfs_create_file("ios", S_IRUSR, root, host, &mmc_ios_fops))
		goto err_node;

	if (!debugfs_create_file("clock", S_IRUSR | S_IWUSR, root, host,
			&mmc_clock_fops))
		goto err_node;

#ifdef CONFIG_MMC_CLKGATE
	if (!debugfs_create_u32("clk_delay", (S_IRUSR | S_IWUSR),
				root, &host->clk_delay))
		goto err_node;
#endif
#ifdef CONFIG_FAIL_MMC_REQUEST
	if (fail_request)
		setup_fault_attr(&fail_default_attr, fail_request);
	host->fail_mmc_request = fail_default_attr;
	if (IS_ERR(fault_create_debugfs_attr("fail_mmc_request",
					     root,
					     &host->fail_mmc_request)))
		goto err_node;
#endif
	return;

err_node:
	debugfs_remove_recursive(root);
	host->debugfs_root = NULL;
err_root:
	dev_err(&host->class_dev, "failed to initialize debugfs\n");
}

void mmc_remove_host_debugfs(struct mmc_host *host)
{
	debugfs_remove_recursive(host->debugfs_root);
}

static int mmc_dbg_card_status_get(void *data, u64 *val)
{
	struct mmc_card	*card = data;
	u32		status;
	int		ret;

	mmc_claim_host(card->host);

	ret = mmc_send_status(data, &status);
	if (!ret)
		*val = status;

	mmc_release_host(card->host);

	return ret;
}
DEFINE_SIMPLE_ATTRIBUTE(mmc_dbg_card_status_fops, mmc_dbg_card_status_get,
		NULL, "%08llx\n");

#define EXT_CSD_STR_LEN 1025

static int mmc_ext_csd_open(struct inode *inode, struct file *filp)
{
	struct mmc_card *card = inode->i_private;
	char *buf;
	ssize_t n = 0;
	u8 *ext_csd;
	int err, i;

	buf = kmalloc(EXT_CSD_STR_LEN + 1, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	ext_csd = kmalloc(512, GFP_KERNEL);
	if (!ext_csd) {
		err = -ENOMEM;
		goto out_free;
	}

	mmc_claim_host(card->host);
	err = mmc_send_ext_csd(card, ext_csd);
	mmc_release_host(card->host);
	if (err)
		goto out_free;

	for (i = 0; i < 512; i++)
		n += sprintf(buf + n, "%02x", ext_csd[i]);
	n += sprintf(buf + n, "\n");
	BUG_ON(n != EXT_CSD_STR_LEN);

	filp->private_data = buf;
	kfree(ext_csd);
	return 0;

out_free:
	kfree(buf);
	kfree(ext_csd);
	return err;
}

static ssize_t mmc_ext_csd_read(struct file *filp, char __user *ubuf,
				size_t cnt, loff_t *ppos)
{
	char *buf = filp->private_data;

	return simple_read_from_buffer(ubuf, cnt, ppos,
				       buf, EXT_CSD_STR_LEN);
}

static int mmc_ext_csd_release(struct inode *inode, struct file *file)
{
	kfree(file->private_data);
	return 0;
}

static const struct file_operations mmc_dbg_ext_csd_fops = {
	.open		= mmc_ext_csd_open,
	.read		= mmc_ext_csd_read,
	.release	= mmc_ext_csd_release,
	.llseek		= default_llseek,
};

void mmc_add_card_debugfs(struct mmc_card *card)
{
	struct mmc_host	*host = card->host;
	struct dentry	*root;

	if (!host->debugfs_root)
		return;

	root = debugfs_create_dir(mmc_card_id(card), host->debugfs_root);
	if (IS_ERR(root))
		/* Don't complain -- debugfs just isn't enabled */
		return;
	if (!root)
		/* Complain -- debugfs is enabled, but it failed to
		 * create the directory. */
		goto err;

	card->debugfs_root = root;

	if ((mmc_card_mmc(card))
		&& (host->index == 0)
		&& (card->cid.manfid == MMC_SAMSUNG_MANFID)){


	        if (!debugfs_create_file("sram_dump", S_IRUSR, root, card,
                                        &mmc_dbg_sram_fops))
                        goto err;
	}

	if (!debugfs_create_x32("state", S_IRUSR, root, &card->state))
		goto err;

	if (mmc_card_mmc(card) || mmc_card_sd(card))
		if (!debugfs_create_file("status", S_IRUSR, root, card,
					&mmc_dbg_card_status_fops))
			goto err;

	if (mmc_card_mmc(card))
		if (!debugfs_create_file("ext_csd", S_IRUSR, root, card,
					&mmc_dbg_ext_csd_fops))
			goto err;

	return;

err:
	debugfs_remove_recursive(root);
	card->debugfs_root = NULL;
	dev_err(&card->dev, "failed to initialize debugfs\n");
}

void mmc_remove_card_debugfs(struct mmc_card *card)
{
	debugfs_remove_recursive(card->debugfs_root);
}
