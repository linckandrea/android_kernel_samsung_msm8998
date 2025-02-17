/*
 *  drivers/misc/sec_param.c
 *
 * Copyright (c) 2011 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/sec_param.h>
#include <linux/file.h>
#include <linux/syscalls.h>
#include <linux/delay.h>
#include <linux/sec_class.h>
#ifdef CONFIG_SEC_NAD
#include <linux/sec_nad.h>
#endif
#define PARAM_RD	0
#define PARAM_WR	1

#define SEC_PARAM_FILE_NAME	"/dev/block/bootdevice/by-name/param"	/* parameter block */

static DEFINE_MUTEX(sec_param_mutex);

/* single global instance */
struct sec_param_data *param_data;
struct sec_param_data_s sched_sec_param_data;
static char* salescode_from_cmdline;

static void param_sec_operation(struct work_struct *work)
{
	/* Read from PARAM(parameter) partition  */
	struct file *filp;
	mm_segment_t fs;
	int ret = true;
	struct sec_param_data_s *sched_param_data =
		container_of(work, struct sec_param_data_s, sec_param_work);

	int flag = (sched_param_data->direction == PARAM_WR) ? (O_RDWR | O_SYNC) : O_RDONLY;

	pr_debug("%s %p %x %d %d\n", __func__, sched_param_data->value, sched_param_data->offset, sched_param_data->size, sched_param_data->direction);

	fs = get_fs();
	set_fs(get_ds());

	filp = filp_open(SEC_PARAM_FILE_NAME, flag, 0);

	if (IS_ERR(filp)) {
		pr_err("%s: filp_open failed. (%ld)\n",
				__func__, PTR_ERR(filp));
		set_fs(fs);
		complete(&sched_sec_param_data.work);
		return;
	}

	ret = filp->f_op->llseek(filp, sched_param_data->offset, SEEK_SET);
	if (ret < 0) {
		pr_err("%s FAIL LLSEEK\n", __func__);
		ret = false;
		goto param_sec_debug_out;
	}

	if (sched_param_data->direction == PARAM_RD)
		vfs_read(filp, (char __user *)sched_param_data->value,
				sched_param_data->size, &filp->f_pos);
	else if (sched_param_data->direction == PARAM_WR)
		vfs_write(filp, (char __user *)sched_param_data->value,
				sched_param_data->size, &filp->f_pos);

param_sec_debug_out:
	set_fs(fs);
	filp_close(filp, NULL);
	complete(&sched_sec_param_data.work);
	return;
}

bool sec_open_param(void)
{
	int ret = true;

	pr_info("%s start \n",__func__);

	if (!param_data)
		param_data = kmalloc(sizeof(struct sec_param_data), GFP_KERNEL);

	if (unlikely(!param_data)) {
		pr_err("failed to alloc for param_data\n");
		return false;
	}

	sched_sec_param_data.value=param_data;
	sched_sec_param_data.offset=SEC_PARAM_FILE_OFFSET;
	sched_sec_param_data.size=sizeof(struct sec_param_data);
	sched_sec_param_data.direction=PARAM_RD;

	schedule_work(&sched_sec_param_data.sec_param_work);
	wait_for_completion(&sched_sec_param_data.work);

	pr_info("%s end \n",__func__);

	return ret;

	}

bool sec_write_param(void)
{
	int ret = true;

	pr_info("%s start\n",__func__);

	sched_sec_param_data.value=param_data;
	sched_sec_param_data.offset=SEC_PARAM_FILE_OFFSET;
	sched_sec_param_data.size=sizeof(struct sec_param_data);
	sched_sec_param_data.direction=PARAM_WR;

	schedule_work(&sched_sec_param_data.sec_param_work);
	wait_for_completion(&sched_sec_param_data.work);

	pr_info("%s end\n",__func__);

	return ret;

}

bool sec_get_param(enum sec_param_index index, void *value)
{
	bool ret = true;

	mutex_lock(&sec_param_mutex);

	ret = sec_open_param();
	if (!ret)
		goto out;

	switch (index) {
	case param_index_debuglevel:
		memcpy(value, &(param_data->debuglevel), sizeof(unsigned int));
		break;
	case param_index_uartsel:
		memcpy(value, &(param_data->uartsel), sizeof(unsigned int));
		break;
	case param_rory_control:
		memcpy(value, &(param_data->rory_control),
				sizeof(unsigned int));
		break;
	case param_index_movinand_checksum_done:
		memcpy(value, &(param_data->movinand_checksum_done),
				sizeof(unsigned int));
		break;
	case param_index_movinand_checksum_pass:
		memcpy(value, &(param_data->movinand_checksum_pass),
				sizeof(unsigned int));
		break;
#ifdef CONFIG_GSM_MODEM_SPRD6500
	case param_update_cp_bin:
		memcpy(value, &(param_data->update_cp_bin),
				sizeof(unsigned int));
		printk(KERN_INFO "param_data.update_cp_bin :[%d]!!", param_data->update_cp_bin);
		break;
#endif
#ifdef CONFIG_RTC_AUTO_PWRON_PARAM
	case param_index_sapa:
		memcpy(value, param_data->sapa, sizeof(unsigned int)*3);
		break;
#endif
#ifdef CONFIG_SEC_MONITOR_BATTERY_REMOVAL
	case param_index_normal_poweroff:
		memcpy(&(param_data->normal_poweroff), value, sizeof(unsigned int));
		break;
#endif
#ifdef CONFIG_BARCODE_PAINTER
	case param_index_barcode_imei:
		memcpy(value, param_data->param_barcode_imei,
			sizeof(param_data->param_barcode_imei));
		break;
	case param_index_barcode_meid:
		memcpy(value, param_data->param_barcode_meid,
			sizeof(param_data->param_barcode_meid));
		break;
	case param_index_barcode_sn:
		memcpy(value, param_data->param_barcode_sn,
			sizeof(param_data->param_barcode_sn));
		break;
	case param_index_barcode_prdate:
		memcpy(value, param_data->param_barcode_prdate,
			sizeof(param_data->param_barcode_prdate));
		break;
	case param_index_barcode_sku:
		memcpy(value, param_data->param_barcode_sku,
			sizeof(param_data->param_barcode_sku));
		break;
#endif
#ifdef CONFIG_WIRELESS_CHARGER_HIGH_VOLTAGE
	case param_index_wireless_charging_mode:
		memcpy(value, &(param_data->wireless_charging_mode), sizeof(unsigned int));
		break;
#endif
#ifdef CONFIG_MUIC_HV
	case param_index_afc_disable:
		memcpy(value, &(param_data->afc_disable), sizeof(unsigned int));
		break;
#endif
	case param_index_cp_reserved_mem:
		memcpy(value, &(param_data->cp_reserved_mem), sizeof(unsigned int));
		break;
	case param_index_carrierid:
		memcpy(value, param_data->param_carrierid, sizeof(param_data->param_carrierid));
		break;
	case param_index_sales:
		memcpy(value, param_data->param_sales, sizeof(param_data->param_sales));
		break;
	case param_index_lcd_resolution:
		memcpy(value, param_data->param_lcd_resolution,
			sizeof(param_data->param_lcd_resolution));
		break;
	case param_index_api_gpio_test:
		memcpy(value, &(param_data->api_gpio_test),
			sizeof(param_data->api_gpio_test));
		break;
	case param_index_api_gpio_test_result:
		memcpy(value, param_data->api_gpio_test_result,
			sizeof(param_data->api_gpio_test_result));
		break;
	case param_index_reboot_recovery_cause:
		memcpy(value, param_data->reboot_recovery_cause,
				sizeof(param_data->reboot_recovery_cause));
		break;
	case param_index_FMM_lock:
		memcpy(value, &(param_data->FMM_lock),
				sizeof(param_data->FMM_lock));
		break;
#ifdef CONFIG_SEC_NAD
	case param_index_qnad:
		sched_sec_param_data.value=value;
		sched_sec_param_data.offset=SEC_PARAM_NAD_OFFSET;
		sched_sec_param_data.size=sizeof(struct param_qnad);
		sched_sec_param_data.direction=PARAM_RD;
		schedule_work(&sched_sec_param_data.sec_param_work);
		wait_for_completion(&sched_sec_param_data.work);
		break;
	case param_index_qnad_ddr_result:
		sched_sec_param_data.value=value;
		sched_sec_param_data.offset=SEC_PARAM_NAD_DDR_RESULT_OFFSET;
		sched_sec_param_data.size=sizeof(struct param_qnad_ddr_result);
		sched_sec_param_data.direction=PARAM_RD;
		schedule_work(&sched_sec_param_data.sec_param_work);
		wait_for_completion(&sched_sec_param_data.work);
		break;
#endif
	default:
		ret = false;
	}

out:
	mutex_unlock(&sec_param_mutex);
	return ret;
}
EXPORT_SYMBOL(sec_get_param);

bool sec_set_param(enum sec_param_index index, void *value)
{
	bool ret = true;

	mutex_lock(&sec_param_mutex);

	ret = sec_open_param();
	if (!ret)
		goto out;

	switch (index) {
	case param_index_debuglevel:
		memcpy(&(param_data->debuglevel),
				value, sizeof(unsigned int));
		break;
	case param_index_uartsel:
		memcpy(&(param_data->uartsel),
				value, sizeof(unsigned int));
		break;
	case param_rory_control:
		memcpy(&(param_data->rory_control),
				value, sizeof(unsigned int));
		break;
	case param_index_movinand_checksum_done:
		memcpy(&(param_data->movinand_checksum_done),
				value, sizeof(unsigned int));
		break;
	case param_index_movinand_checksum_pass:
		memcpy(&(param_data->movinand_checksum_pass),
				value, sizeof(unsigned int));
		break;
#ifdef CONFIG_GSM_MODEM_SPRD6500
	case param_update_cp_bin:
		memcpy(&(param_data->update_cp_bin),
				value, sizeof(unsigned int));
		break;
#endif
#ifdef CONFIG_RTC_AUTO_PWRON_PARAM
	case param_index_sapa:
		memcpy(param_data->sapa, value, sizeof(unsigned int)*3);
		break;
#endif
#ifdef CONFIG_SEC_MONITOR_BATTERY_REMOVAL
	case param_index_normal_poweroff:
		memcpy(&(param_data->normal_poweroff), value, sizeof(unsigned int));
		break;
#endif
#ifdef CONFIG_BARCODE_PAINTER
	case param_index_barcode_imei:
		memcpy(param_data->param_barcode_imei,
				value, sizeof(param_data->param_barcode_imei));
		break;
	case param_index_barcode_meid:
		memcpy(param_data->param_barcode_meid,
				value, sizeof(param_data->param_barcode_meid));
		break;
	case param_index_barcode_sn:
		memcpy(param_data->param_barcode_sn,
				value, sizeof(param_data->param_barcode_sn));
		break;
	case param_index_barcode_prdate:
		memcpy(param_data->param_barcode_prdate,
				value, sizeof(param_data->param_barcode_prdate));
		break;
	case param_index_barcode_sku:
		memcpy(param_data->param_barcode_sku,
				value, sizeof(param_data->param_barcode_sku));
		break;
#endif
#ifdef CONFIG_WIRELESS_CHARGER_HIGH_VOLTAGE
	case param_index_wireless_charging_mode:
		memcpy(&(param_data->wireless_charging_mode),
				value, sizeof(unsigned int));
		break;
#endif
#ifdef CONFIG_MUIC_HV
	case param_index_afc_disable:
		memcpy(&(param_data->afc_disable),
				value, sizeof(unsigned int));
		break;
#endif
	case param_index_cp_reserved_mem:
		memcpy(&(param_data->cp_reserved_mem),
				value, sizeof(unsigned int));
		break;
	case param_index_carrierid:
		memcpy(param_data->param_carrierid,value, sizeof(param_data->param_carrierid));
		break;
	case param_index_sales:
		memcpy(param_data->param_sales,value, sizeof(param_data->param_sales));
		break;
	case param_index_lcd_resolution:
		memcpy(&(param_data->param_lcd_resolution),
				value, sizeof(param_data->param_lcd_resolution));
		break;
	case param_index_api_gpio_test:
		memcpy(&(param_data->api_gpio_test),
				value, sizeof(param_data->api_gpio_test));
		break;
	case param_index_api_gpio_test_result:
		memcpy(&(param_data->api_gpio_test_result),
				value, sizeof(param_data->api_gpio_test_result));
		break;
	case param_index_reboot_recovery_cause:
		memcpy(param_data->reboot_recovery_cause,
				value, sizeof(param_data->reboot_recovery_cause));
		break;
	case param_index_FMM_lock:
		if (*(unsigned int*)value == (unsigned int)FMMLOCK_MAGIC_NUM || *(unsigned int*)value == (unsigned int)0 ) {
			memcpy(&(param_data->FMM_lock),
					value, sizeof(param_data->FMM_lock));
		}
		break;
#ifdef CONFIG_SEC_NAD
	case param_index_qnad:
		sched_sec_param_data.value=(struct param_qnad *)value;
		sched_sec_param_data.offset=SEC_PARAM_NAD_OFFSET;
		sched_sec_param_data.size=sizeof(struct param_qnad);
		sched_sec_param_data.direction=PARAM_WR;
		schedule_work(&sched_sec_param_data.sec_param_work);
		wait_for_completion(&sched_sec_param_data.work);
		break;
	case param_index_qnad_ddr_result:
		sched_sec_param_data.value=(struct param_qnad_ddr_result *)value;
		sched_sec_param_data.offset=SEC_PARAM_NAD_DDR_RESULT_OFFSET;
		sched_sec_param_data.size=sizeof(struct param_qnad_ddr_result);
		sched_sec_param_data.direction=PARAM_WR;
		schedule_work(&sched_sec_param_data.sec_param_work);
		wait_for_completion(&sched_sec_param_data.work);
		break;
#endif
	default:
		ret = false;
		goto out;
	}

	ret = sec_write_param();

out:
	mutex_unlock(&sec_param_mutex);
	return ret;
}
EXPORT_SYMBOL(sec_set_param);

bool sales_code_is(char* str)
{
	bool status = 0;
	char* salescode;

	salescode = kmalloc(sizeof(param_data->param_sales), GFP_KERNEL);
	if (!salescode) {
		goto out;
	}
	memset(salescode, 0x00,sizeof(param_data->param_sales));

	salescode = salescode_from_cmdline;

	pr_info("%s: %s\n", __func__,salescode);

	if(!strncmp((char *)salescode, str, 3))
		status = 1;

out:	return status;
}
EXPORT_SYMBOL(sales_code_is);

static int __init sales_code_setup(char *str)
{
	salescode_from_cmdline = str;
	return 1;
}
__setup("androidboot.sales_code=", sales_code_setup);

static int __init sec_param_work_init(void)
{
	pr_info("%s: start\n", __func__);

	sched_sec_param_data.offset=0;
	sched_sec_param_data.direction=0;
	sched_sec_param_data.size=0;
	sched_sec_param_data.value=NULL;

	init_completion(&sched_sec_param_data.work);
	INIT_WORK(&sched_sec_param_data.sec_param_work, param_sec_operation);

	pr_info("%s: end\n", __func__);

	return 0;
}

static void __exit sec_param_work_exit(void)
{
	cancel_work_sync(&sched_sec_param_data.sec_param_work);
	pr_info("%s: exit\n", __func__);
}

static struct device *sec_debug_dev = NULL;

static ssize_t show_FMM_lock(struct device *dev,
                struct device_attribute *attr, char *buf)
{
        int lock=0;
	char str[30];

        sec_get_param(param_index_FMM_lock, &lock);
        snprintf(str,sizeof(str),"FMM lock : [%s]\n", lock?"ON":"OFF");

        return scnprintf(buf, sizeof(str), "%s", str);
}

static ssize_t store_FMM_lock(struct device *dev,
                struct device_attribute *attr, const char *buf, size_t count)
{
        int lock;

	sscanf(buf, "%d", &lock);
	if(lock)
		lock = FMMLOCK_MAGIC_NUM;
	else
		lock = 0;

	pr_err("FMM lock[%s]\n", lock?"ON":"OFF");
        sec_set_param(param_index_FMM_lock, &lock);

        return count;
}

static DEVICE_ATTR(FMM_lock, 0660, show_FMM_lock, store_FMM_lock);

static int __init sec_debug_FMM_lock_init(void)
{
        int ret;

	if(!sec_debug_dev){
		/* create sec_debug_dev */
		sec_debug_dev = device_create(sec_class, NULL, 0, NULL, "sec_debug");
		if (IS_ERR(sec_debug_dev)) {
			pr_err("Failed to create device for sec_debug\n");
			return PTR_ERR(sec_debug_dev);
		}
	}

        ret = sysfs_create_file(&sec_debug_dev->kobj, &dev_attr_FMM_lock.attr);
        if (ret) {
                pr_err("Failed to create sysfs group for sec_debug\n");
		device_destroy(sec_class, sec_debug_dev->devt);
                sec_debug_dev = NULL;
                return ret;
        }

        return 0;
}
device_initcall(sec_debug_FMM_lock_init);

module_init(sec_param_work_init);
module_exit(sec_param_work_exit);

