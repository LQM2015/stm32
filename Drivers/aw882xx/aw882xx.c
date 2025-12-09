/*
* aw882xx.c
*
* Copyright (c) 2021 AWINIC Technology CO., LTD
*
* Author: <zhaolei@awinic.com>
*
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "aw882xx.h"
#include "aw882xx_base.h"
#include "aw882xx_device.h"
#include "aw882xx_init.h"

#define AW882XX_DRIVER_VERSION "v0.5.0"

#define AW_READ_CHIPID_RETRIES		5	/* 5 times */
#define AW_READ_CHIPID_RETRY_DELAY	5	/* 5 ms */

static unsigned int g_aw882xx_dev_cnt = 0;
struct aw882xx *g_aw882xx[AW_DEV_MAX];

static int aw882xx_hw_reset(struct aw882xx *aw882xx);


int aw882xx_get_version(char *buf, int size)
{
	if (size > strlen(AW882XX_DRIVER_VERSION)) {
		memcpy(buf, AW882XX_DRIVER_VERSION, strlen(AW882XX_DRIVER_VERSION));
		return strlen(AW882XX_DRIVER_VERSION);
	} else {
		return -ENOMEM;
	}
}

int aw882xx_get_dev_num(void) {
	return g_aw882xx_dev_cnt;
}

/******************************************************
 *
 * aw882xx i2c write/read
 *
 ******************************************************/
static int aw882xx_i2c_writes(struct aw882xx *aw882xx,
	unsigned char reg_addr, unsigned char *buf, unsigned int len)
{
	int ret = -1;

	ret = aw882xx->i2c_write_func(aw882xx->i2c_addr, reg_addr, buf, len);
	if (ret < 0) {
		aw_dev_err(aw882xx->dev_index, "i2c write error");
	}

	return ret;
}

static int aw882xx_i2c_reads(struct aw882xx *aw882xx,
	unsigned char reg_addr, unsigned char *data_buf, unsigned int data_len)
{
	int ret = -1;

	ret = aw882xx->i2c_read_func(aw882xx->i2c_addr, reg_addr, data_buf, data_len);
	if (ret < 0) {
		aw_dev_err(aw882xx->dev_index, "i2c read error");
	}

	return ret;
}

int aw882xx_i2c_write(struct aw882xx *aw882xx,
	unsigned char reg_addr, unsigned int reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;
	unsigned char buf[2];

	buf[0] = (reg_data&0xff00)>>8;
	buf[1] = (reg_data&0x00ff)>>0;

	while (cnt < AW_I2C_RETRIES) {
		ret = aw882xx_i2c_writes(aw882xx, reg_addr, buf, 2);
		if (ret < 0) {
			aw_dev_err(aw882xx->dev_index, "i2c_write cnt=%d error=%d",
					cnt, ret);
		} else {
			break;
		}
		cnt++;
	}

	return ret;
}

int aw882xx_i2c_read(struct aw882xx *aw882xx,
	unsigned char reg_addr, unsigned int *reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;
	unsigned char buf[2] = { 0 };
	uint16_t data = 0;

	while (cnt < AW_I2C_RETRIES) {
		ret = aw882xx_i2c_reads(aw882xx, reg_addr, buf, 2);
		if (ret < 0) {
			aw_dev_err(aw882xx->dev_index, "i2c_read cnt=%d error=%d",
						cnt, ret);
		} else {
					data = (uint16_t)(buf[0] & 0x00ff);
					data <<= 8;
					data |= (uint16_t)(buf[1] & 0x00ff);
					*reg_data = data;
			break;
		}
		cnt++;
	}

	return ret;
}

int aw882xx_i2c_write_bits(struct aw882xx *aw882xx,
	unsigned char reg_addr, unsigned int mask, unsigned int reg_data)
{
	int ret = -1;
	unsigned int reg_val = 0;

	ret = aw882xx_i2c_read(aw882xx, reg_addr, &reg_val);
	if (ret < 0) {
		aw_dev_err(aw882xx->dev_index, "i2c read error, ret=%d", ret);
		return ret;
	}
	reg_val &= mask;
	reg_val |= reg_data;
	ret = aw882xx_i2c_write(aw882xx, reg_addr, reg_val);
	if (ret < 0) {
		aw_dev_err(aw882xx->dev_index, "i2c read error, ret=%d", ret);
		return ret;
	}

	return 0;
}


/******************************************************
 * aw882xx interface
 ******************************************************/

#ifdef AW_DEBUG
int aw882xx_reg_store(aw_dev_index_t dev_index, uint8_t reg_addr, uint16_t reg_data)
{
	struct aw882xx *aw882xx = NULL;

	if ((dev_index >= AW_DEV_MAX) || (g_aw882xx[dev_index] == NULL)) {
		aw_pr_err("unsupported dev_index:%d or g_aw882xx[%d] is NULL", dev_index, dev_index);
		return -EINVAL;
	}

	aw882xx = g_aw882xx[dev_index];

	aw882xx_i2c_write(aw882xx, reg_addr, reg_data);

	return 0;
}

int aw882xx_reg_show(aw_dev_index_t dev_index)
{
	struct aw882xx *aw882xx = NULL;

	if ((dev_index >= AW_DEV_MAX) || (g_aw882xx[dev_index] == NULL)) {
		aw_pr_err("unsupported dev_index:%d or g_aw882xx[%d] is NULL", dev_index, dev_index);
		return -EINVAL;
	}

	aw882xx = g_aw882xx[dev_index];

	aw882xx_dev_reg_dump(aw882xx->aw_pa);

	return 0;
}

int aw882xx_hw_reset_by_index(aw_dev_index_t dev_index)
{
	if ((dev_index >= AW_DEV_MAX) || (g_aw882xx[dev_index] == NULL)) {
		aw_pr_err("unsupported dev_index:%d or g_aw882xx[%d] is NULL", dev_index, dev_index);
		return -EINVAL;
	}

	aw882xx_hw_reset(g_aw882xx[dev_index]);

	return 0;
}

int aw882xx_soft_reset(aw_dev_index_t dev_index)
{
	if ((dev_index >= AW_DEV_MAX) || (g_aw882xx[dev_index] == NULL)) {
		aw_pr_err("unsupported dev_index:%d or g_aw882xx[%d] is NULL", dev_index, dev_index);
		return -EINVAL;
	}

	aw882xx_dev_soft_reset(g_aw882xx[dev_index]->aw_pa);

	return 0;
}
#endif

#ifdef AW_FADE
int aw882xx_set_fade_step(aw_dev_index_t dev_index, uint32_t step)
{
	struct aw882xx *aw882xx = NULL;

	if ((dev_index >= AW_DEV_MAX) || (g_aw882xx[dev_index] == NULL)) {
		aw_pr_err("unsupported dev_index:%d or g_aw882xx[%d] is NULL", dev_index, dev_index);
		return -EINVAL;
	}

	aw882xx = g_aw882xx[dev_index];
	aw882xx_dev_set_fade_vol_step(aw882xx->aw_pa, step);

	aw_dev_info(aw882xx->dev_index, "set step %d DB Done", step);

	return 0;
}

int aw882xx_get_fade_step(aw_dev_index_t dev_index, uint32_t *step)
{
	struct aw882xx *aw882xx = NULL;

	if (dev_index >= AW_DEV_MAX) {
		aw_pr_err("unsupported dev_index:%d", dev_index);
		return -EINVAL;
	}

	if ((g_aw882xx[dev_index] == NULL) || (step == NULL)) {
		aw_pr_err("g_aw882xx[%d] is NULL or step is NULL", dev_index);
		return -EINVAL;
	}

	aw882xx = g_aw882xx[dev_index];

	*step = aw882xx_dev_get_fade_vol_step(aw882xx->aw_pa);
	aw_dev_info(aw882xx->dev_index, "get step %d Done", *step);

	return 0;
}

int aw882xx_set_fade_time(aw_fade_dir_t fade_dir, uint32_t time)
{
	aw882xx_dev_set_fade_time(time, fade_dir);

	return 0;
}

int aw882xx_get_fade_time(aw_fade_dir_t fade_dir, uint32_t *time)
{
	if (time == NULL) {
		aw_pr_err("time is NULL");
		return -EINVAL;
	}

	aw882xx_dev_get_fade_time(time, fade_dir);

	return 0;
}
#endif

#ifdef AW_VOLUME
int aw882xx_get_volume(aw_dev_index_t dev_index, uint32_t *volume)
{
	struct aw882xx *aw882xx = NULL;
	struct aw_volume_desc *vol_desc = NULL;

	if (dev_index >= AW_DEV_MAX) {
		aw_pr_err("unsupported dev_index:%d", dev_index);
		return -EINVAL;
	}

	if ((g_aw882xx[dev_index] == NULL) || (volume == NULL)) {
		aw_pr_err("g_aw882xx[%d] is NULL or volume is NULL", dev_index);
		return -EINVAL;
	}

	aw882xx = g_aw882xx[dev_index];
	vol_desc = &aw882xx->aw_pa->volume_desc;

	*volume = vol_desc->ctl_volume;

	aw_dev_info(aw882xx->dev_index, "get volume %d", *volume);

	return 0;
}

int aw882xx_set_volume(aw_dev_index_t dev_index,  uint32_t volume)
{
	struct aw882xx *aw882xx = NULL;
	struct aw_volume_desc *vol_desc = NULL;
	int ret = -1;

	if ((dev_index >= AW_DEV_MAX) || (g_aw882xx[dev_index] == NULL)) {
		aw_pr_err("unsupported dev_index:%d or g_aw882xx[%d] is NULL", dev_index, dev_index);
		return -EINVAL;
	}

	aw882xx = g_aw882xx[dev_index];

	ret = aw882xx_dev_set_volume(aw882xx->aw_pa, volume);
	if (ret < 0) {
		aw_dev_err(aw882xx->dev_index, "set volume %d failed", volume);
		return ret;
	}

	vol_desc = &aw882xx->aw_pa->volume_desc;
	vol_desc->ctl_volume = volume;

	aw_dev_info(aw882xx->dev_index, "set volume %d", volume);

	return 0;
}
#endif

static void aw882xx_start_pa(struct aw882xx *aw882xx)
{
	int ret = -1;
	int i;

	aw_dev_info(aw882xx->dev_index, "enter");

	for (i = 0; i < AW_START_RETRIES; i++) {
		/*if PA already power ,stop PA then start*/
		if (aw882xx->aw_pa->status) {
			aw_dev_info(aw882xx->dev_index, "pa already start");
			return;
		}

		ret = aw882xx_dev_reg_update(aw882xx->aw_pa, aw882xx->phase_sync);
		if (ret) {
			aw_dev_err(aw882xx->dev_index, "fw update failed, cnt:%d", i);
			continue;
		}

		ret = aw882xx_device_start(aw882xx->aw_pa);
		if (ret) {
			aw_dev_err(aw882xx->dev_index, "start failed, cnt:%d", i);
			continue;
		} else {
			/* ensure boost limits are re-applied after profile reload */
			if (aw882xx_dev_set_boost_ipeak_ma(aw882xx->aw_pa, AW_IPEAK_MAX)) {
				aw_dev_err(aw882xx->dev_index, "set boost ipeak failed");
			}
			if (aw882xx_dev_set_boost_voltage_uv(aw882xx->aw_pa, AW_VPEAK_MAX)) {
				aw_dev_err(aw882xx->dev_index, "set boost voltage failed");
			}
			aw_dev_info(aw882xx->dev_index, "start success");
			break;
		}
	}
}

static void aw882xx_stop_pa(struct aw882xx *aw882xx)
{
	aw882xx_device_stop(aw882xx->aw_pa);
}

int aw882xx_ctrl_state(aw_dev_index_t dev_index, aw_ctrl_t aw_ctrl)
{
	struct aw882xx *aw882xx = NULL;

	if ((dev_index >= AW_DEV_MAX) || (g_aw882xx[dev_index] == NULL)) {
		aw_pr_err("unsupported dev_index:%d or g_aw882xx[%d] is NULL", dev_index, dev_index);
		return -EINVAL;
	}

	aw882xx = g_aw882xx[dev_index];

	if (aw_ctrl) {
		aw882xx_stop_pa(aw882xx);
	} else {
		aw882xx_start_pa(aw882xx);
	}
	return 0;
}

int aw882xx_set_profile_byname(aw_dev_index_t dev_index, char *prof_name)
{
	int ret = -1;
	char *cur_name = NULL;
	struct aw882xx *aw882xx = NULL;

	if ((dev_index >= AW_DEV_MAX) || (g_aw882xx[dev_index] == NULL)) {
		aw_pr_err("unsupported dev_index:%d or g_aw882xx[%d] is NULL", dev_index, dev_index);
		return -EINVAL;
	}

	aw882xx = g_aw882xx[dev_index];

	/*check cur_name == set name*/
	cur_name = aw882xx_dev_get_profile_name(aw882xx->aw_pa);
	if (strncmp(cur_name, prof_name, AW_PROF_NAME_MAX) == 0) {
		aw_dev_info(aw882xx->dev_index, "prof no change");
		return 0;
	}

	ret = aw882xx_dev_set_profile_name(aw882xx->aw_pa, prof_name);
	if (ret < 0) {
		aw_dev_err(aw882xx->dev_index, "set profile [%s] failed", prof_name);
		return -EINVAL;
	}

	aw_dev_info(aw882xx->dev_index, "set prof name [%s]", prof_name);

	return 0;
}

static int aw882xx_check_init_info(struct aw_init_info *init_info)
{
	int ret = -1;

	if (g_aw882xx[init_info->dev_index] != NULL) {
		aw_pr_err("dev:%d exist", init_info->dev_index);
		return -EINVAL;
	}

	/*check dev index*/
	if (init_info->dev_index >= AW_DEV_MAX) {
		aw_pr_err("unsupported dev_index:%d", init_info->dev_index);
		return -EINVAL;
	}

	/*check i2c funtion*/
	if ((init_info->i2c_read_func == NULL) ||
		(init_info->i2c_write_func == NULL)) {
		aw_dev_err(init_info->dev_index, "i2c funtion is NULL");
		return -EINVAL;
	}

	ret = aw882xx_dev_check_prof(init_info->dev_index, init_info->prof_info);
	if (ret < 0) {
		aw_dev_err(init_info->dev_index, "check prof failed");
		return ret;
	}

	return 0;
}

static struct aw882xx *aw882xx_malloc_init(struct aw_init_info *init_info)
{
	struct aw882xx *aw882xx = calloc(1, sizeof(struct aw882xx));
	if (aw882xx == NULL) {
		aw_dev_err(init_info->dev_index, "calloc aw882xx failed.");
		return NULL;
	}

	aw882xx->init_info = init_info;
	aw882xx->aw_pa = NULL;
	aw882xx->dev_index = init_info->dev_index;
	aw882xx->i2c_addr = init_info->i2c_addr;
	aw882xx->i2c_read_func = init_info->i2c_read_func;
	aw882xx->i2c_write_func = init_info->i2c_write_func;
	if (init_info->phase_sync) {
		aw882xx->phase_sync = init_info->phase_sync;
	} else {
		aw882xx->phase_sync = AW_PHASE_SYNC_DISABLE;
	}

	if (init_info->reset_gpio_ctl) {
		aw882xx->reset_gpio_ctl = init_info->reset_gpio_ctl;
	} else {
		aw882xx->reset_gpio_ctl = NULL;
	}

	return aw882xx;
}

static int aw882xx_hw_reset(struct aw882xx *aw882xx)
{
	aw_dev_info(aw882xx->dev_index, "enter");

	if (aw882xx->reset_gpio_ctl == NULL) {
		aw_dev_info(aw882xx->dev_index, "no reset gpio control");
		return 0;
	}

	aw882xx->reset_gpio_ctl(AW_PIN_RESET);
	aw_dev_info(aw882xx->dev_index, "gpio_ctl=%d", AW_PIN_RESET);
	AW_MS_DELAY(AW_1_MS);
	aw882xx->reset_gpio_ctl(AW_PIN_SET);
	aw_dev_info(aw882xx->dev_index, "gpio_ctl=%d", AW_PIN_SET);
	AW_MS_DELAY(AW_2_MS);

	return 0;
}

static int aw882xx_read_chipid(struct aw882xx *aw882xx)
{
	int ret = -1;
	unsigned int cnt = 0;
	unsigned int reg_value = 0;

	while (cnt < AW_READ_CHIPID_RETRIES) {
		ret = aw882xx_i2c_read(aw882xx, AW882XX_CHIP_ID_REG, &reg_value);
		if (ret < 0) {
			aw_dev_err(aw882xx->dev_index, "failed to read REG_ID: %d", ret);
			return -EIO;
		}
		switch (reg_value) {
		case PID_1852_ID: {
			aw_dev_info(aw882xx->dev_index, "aw882xx 1852 detected");
			aw882xx->chip_id = reg_value;
			return 0;
		}
		case PID_2013_ID: {
			aw_dev_info(aw882xx->dev_index, "aw882xx 2013 detected");
			aw882xx->chip_id = reg_value;
			return 0;
		}
		case PID_2032_ID: {
			aw_dev_info(aw882xx->dev_index, "aw882xx 2032 detected");
			aw882xx->chip_id = reg_value;
			return 0;
		}
		case PID_2055_ID: {
			aw_dev_info(aw882xx->dev_index, "aw882xx 2055 detected");
			aw882xx->chip_id = reg_value;
			return 0;
		}
		case PID_2071_ID: {
			aw_dev_info(aw882xx->dev_index, "aw882xx 2071 detected");
			aw882xx->chip_id = reg_value;
			return 0;
		}
		case PID_2113_ID: {
			aw_dev_info(aw882xx->dev_index, "aw882xx 2113 detected");
			aw882xx->chip_id = reg_value;
			return 0;
		}
		default:
			aw_dev_info(aw882xx->dev_index, "unsupported device revision (0x%x)",
							reg_value);
			break;
		}
		cnt++;

		AW_MS_DELAY(AW_READ_CHIPID_RETRY_DELAY);
	}

	return -EINVAL;
}

static int aw882xx_init(struct aw882xx *aw882xx)
{
	int i = 0;
	int ret;
	struct aw_init_info *init_info = aw882xx->init_info;

	for (i = 0; i < init_info->mix_chip_count; i++) {
		aw_dev_dbg(aw882xx->dev_index, "%d enter", i);
		if (init_info->dev_init_ops[i] == NULL) {
			aw_dev_err(aw882xx->dev_index, "init_info->dev_init_ops[%d] is null", i);
			return -EINVAL;
		}

		ret = init_info->dev_init_ops[i]((void *)aw882xx);
		if (ret < 0) {
			aw_dev_dbg(aw882xx->dev_index, "%d enter", i);
			continue;
		} else {
			aw_dev_dbg(aw882xx->dev_index, "%d init success", i);
			break;
		}
	}

	return ret;
}

int aw882xx_smartpa_init(void *aw_info)
{
	int ret = -EIO;
	struct aw_init_info *aw_init_info = NULL;
	struct aw882xx *aw882xx = NULL;

	aw_pr_info("enter");

	if (aw_info == NULL) {
		aw_pr_err("aw_info is NULL");
		return -ENOMEM;
	}

	aw_init_info = (struct aw_init_info *)aw_info;

	ret = aw882xx_check_init_info(aw_init_info);
	if (ret < 0) {
		return ret;
	}
	aw882xx = aw882xx_malloc_init(aw_init_info);
	if (aw882xx == NULL) {
		aw_dev_err(aw_init_info->dev_index, "malloc aw882xx failed");
		return -ENOMEM;
	}

	/* hardware reset */
	aw882xx_hw_reset(aw882xx);

	/* aw882xx chip id */
	ret = aw882xx_read_chipid(aw882xx);
	if (ret < 0) {
		aw_dev_err(aw_init_info->dev_index, "aw882xx_read_chipid failed ret=%d", ret);
		goto read_chip_failed;
	}

	/*aw pa init*/
	ret = aw882xx_init(aw882xx);
	if (ret < 0) {
		goto init_failed;
	}

	if (aw_init_info->fade_en) {
		aw882xx->aw_pa->fade_en = aw_init_info->fade_en;
	} else {
		aw882xx->aw_pa->fade_en = AW_FADE_DISABLE;
	}

	g_aw882xx[aw_init_info->dev_index] = aw882xx;
	g_aw882xx_dev_cnt++;
	aw_dev_info(aw_init_info->dev_index, "dev_index:%d init success",
				aw_init_info->dev_index);

	return 0;

init_failed:
read_chip_failed:
	ret = (ret < 0) ? ret : -EIO;
	free(aw882xx);
	aw882xx = NULL;
	return ret;
}


void aw882xx_smartpa_deinit(aw_dev_index_t dev_index)
{
	struct aw882xx *aw882xx = NULL;

	if ((dev_index >= AW_DEV_MAX) || (g_aw882xx[dev_index] == NULL)) {
		aw_pr_err("unsupported dev_index:%d or g_aw882xx[%d] is NULL", dev_index, dev_index);
	}

	aw882xx = g_aw882xx[dev_index];

	if (aw882xx->aw_pa->ops.aw_monitor_deinit) {
		aw882xx->aw_pa->ops.aw_monitor_deinit((void *)aw882xx->aw_pa);
	}

	if (aw882xx->aw_pa != NULL) {
		free(aw882xx->aw_pa);
		aw882xx->aw_pa = NULL;
	}

	if (aw882xx != NULL) {
		free(aw882xx);
		aw882xx = NULL;
	}

	g_aw882xx[dev_index] = NULL;
	if (g_aw882xx_dev_cnt > 0) {
		g_aw882xx_dev_cnt--;
	}
}


/***************************************************************************
 *aw882xx irq
 ***************************************************************************/
#ifdef AW_IRQ

aw_hw_irq_handle_t aw882xx_get_hw_irq_status(aw_dev_index_t dev_index)
{
	struct aw882xx *aw882xx = NULL;

	if ((dev_index >= AW_DEV_MAX) || (g_aw882xx[dev_index] == NULL)) {
		aw_pr_err("unsupported dev_index:%d or g_aw882xx[%d] is NULL", dev_index, dev_index);
	}

	aw882xx = g_aw882xx[dev_index];
	aw_dev_info(aw882xx->dev_index, "irq_handle=%d", aw882xx->irq_handle);
	return aw882xx->irq_handle;
}

void aw882xx_irq_handler(aw_dev_index_t dev_index)
{
	struct aw882xx *aw882xx = NULL;

	if ((dev_index >= AW_DEV_MAX) || (g_aw882xx[dev_index] == NULL)) {
		aw_pr_err("unsupported dev_index:%d or g_aw882xx[%d] is NULL", dev_index, dev_index);
		return;
	}

	aw882xx = g_aw882xx[dev_index];

	if (aw882xx->irq_handle == AW_HW_IRQ_HANDLE_ON) {
		/*awinic:Add operations after interrupt triggering*/
		aw882xx_dev_interrupt_clear(aw882xx->aw_pa);
		aw882xx->irq_handle = AW_HW_IRQ_HANDLE_OFF;
		aw_dev_info(aw882xx->dev_index, "irq_handle=%d",aw882xx->irq_handle);
	}
}

void aw882xx_irq_trigger(aw_dev_index_t dev_index)
{
	struct aw882xx *aw882xx = NULL;

	if ((dev_index >= AW_DEV_MAX) || (g_aw882xx[dev_index] == NULL)) {
		aw_pr_err("unsupported dev_index:%d or g_aw882xx[%d] is NULL", dev_index, dev_index);
		return;
	}

	aw882xx = g_aw882xx[dev_index];

	aw882xx->irq_handle = AW_HW_IRQ_HANDLE_ON;
	aw_dev_info(aw882xx->dev_index, "irq_handle=%d",aw882xx->irq_handle);
}
#endif

/***********************************************************************
 * aw882xx monitor
 ***********************************************************************/

void aw882xx_monitor_work(aw_dev_index_t dev_index)
{
	struct aw882xx *aw882xx = NULL;

	if ((dev_index >= AW_DEV_MAX) || (g_aw882xx[dev_index] == NULL)) {
		aw_pr_err("unsupported dev_index:%d or g_aw882xx[%d] is NULL", dev_index, dev_index);
		return;
	}

	aw882xx = g_aw882xx[dev_index];
	if (aw882xx->aw_pa->ops.aw_monitor_work_func) {
		aw882xx->aw_pa->ops.aw_monitor_work_func((void *)aw882xx->aw_pa);
	}
}

void aw882xx_monitor_set_status(aw_dev_index_t dev_index)
{
	struct aw882xx *aw882xx = NULL;

	if ((dev_index >= AW_DEV_MAX) || (g_aw882xx[dev_index] == NULL)) {
		aw_pr_err("unsupported dev_index:%d or g_aw882xx[%d] is NULL", dev_index, dev_index);
		return;
	}

	aw882xx = g_aw882xx[dev_index];

	if (aw882xx->aw_pa->ops.aw_monitor_set_handle) {
		aw882xx->aw_pa->ops.aw_monitor_set_handle((void *)aw882xx->aw_pa);
	}
}

