/*
* aw_device.c
*
* Copyright (c) 2021 AWINIC Technology CO., LTD
*
* Author: <zhaolei@awinic.com>
*
*/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "aw882xx_base.h"
#include "aw882xx_device.h"
#include "aw_profile_process.h"

#define AW_DEV_SYSST_CHECK_MAX		(10)

static int aw_dev_set_vcalb(struct aw_device *aw_dev);


#ifdef AW_MONITOR
extern int aw882xx_monitor_init(void *dev);
extern void aw882xx_monitor_start(void *dev);
extern void aw882xx_monitor_stop(void *dev);
extern void aw882xx_monitor_deinit(void *dev);
extern int aw882xx_monitor_work_func(void *dev);
extern void aw882xx_monitor_set_handle(void *dev);

void aw882xx_dev_get_monitor_func(struct aw_device *aw_dev)
{
	aw_dev->ops.aw_monitor_init = aw882xx_monitor_init;
	aw_dev->ops.aw_monitor_start = aw882xx_monitor_start;
	aw_dev->ops.aw_monitor_stop = aw882xx_monitor_stop;
	aw_dev->ops.aw_monitor_deinit = aw882xx_monitor_deinit;
	aw_dev->ops.aw_monitor_work_func = aw882xx_monitor_work_func;
	aw_dev->ops.aw_monitor_set_handle = aw882xx_monitor_set_handle;
}

#else
void aw882xx_dev_get_monitor_func(struct aw_device *aw_dev)
{
	aw_dev->ops.aw_monitor_init = NULL;
	aw_dev->ops.aw_monitor_start = NULL;
	aw_dev->ops.aw_monitor_stop = NULL;
	aw_dev->ops.aw_monitor_deinit = NULL;
	aw_dev->ops.aw_monitor_work_func = NULL;
	aw_dev->ops.aw_monitor_work_func = NULL;
	aw_dev->ops.aw_monitor_set_handle = NULL;
}
#endif


static unsigned int g_fade_in_time = AW_1_MS;
static unsigned int g_fade_out_time = AW_1_MS;

static unsigned int aw_dev_get_shift_from_mask(unsigned int mask)
{
	unsigned int shift = 0;

	while (shift < 32 && (mask & (1u << shift)))
		shift++;

	return shift;
}

static unsigned int aw_dev_get_width_from_mask(unsigned int mask, unsigned int shift)
{
	unsigned int width = 0;

	if (shift >= 32)
		return 0;

	while ((shift + width) < 32 && ((mask & (1u << (shift + width))) == 0))
		width++;

	return width;
}

static unsigned int aw_dev_get_field_mask(unsigned int mask)
{
	unsigned int shift = aw_dev_get_shift_from_mask(mask);
	unsigned int width = aw_dev_get_width_from_mask(mask, shift);

	if (width == 0 || width >= 32)
		return 0;

	return ((1u << width) - 1u) << shift;
}

static unsigned int aw_dev_clamp_unsigned(unsigned int value, unsigned int min,
	unsigned int max)
{
	if (value < min)
		return min;
	if (value > max)
		return max;
	return value;
}

int aw882xx_dev_check_prof(aw_dev_index_t dev_index, struct aw_prof_info *prof_info)
{
	struct aw_prof_desc *prof_desc = NULL;
	int i = 0;
	int j = 0;

	if (prof_info == NULL) {
		aw_dev_err(dev_index, "prof_info is NULL");
		return -EINVAL;
	}

	if (prof_info->count <= 0) {
		aw_dev_err(dev_index, "prof count :%d unsupported", prof_info->count);
		return -EINVAL;
	}

	prof_desc = prof_info->prof_desc;
	for (i = 0; i < prof_info->count; i++) {
		if (prof_desc == NULL) {
			aw_dev_err(dev_index, "invalid prof_desc");
			return -EINVAL;
		}

		if (prof_desc->sec_desc->len <= 0) {
			aw_dev_err(dev_index, "prof len:%d unsupported", prof_desc->sec_desc->len);
			return -EINVAL;
		}

		if (prof_desc->sec_desc->data == NULL) {
			aw_dev_err(dev_index, "prof data is NULL");
			return -EINVAL;
		}
	}

	for (i = 0; i < prof_info->count; i++) {
		for (j = i + 1; j < prof_info->count; j++) {
			if (strncmp(prof_info->prof_desc[i].name, prof_info->prof_desc[j].name, AW_PROF_NAME_MAX) == 0) {
				aw_dev_err(dev_index, "prof_desc pos[%d] and prof_desc pos[%d] conflict with prof_name[%s]",
						i, j, prof_info->prof_desc[j].name);
				return -EINVAL;
			}
		}
	}

	return 0;
}

int aw882xx_dev_set_boost_ipeak_ma(struct aw_device *aw_dev, unsigned int ipeak_ma)
{
	struct aw_ipeak_desc *desc = NULL;
	unsigned int shift = 0;
	unsigned int width = 0;
	unsigned int field_mask = 0;
	unsigned int max_code = 0;
	unsigned int max_valid_code = 0;
	unsigned int target_ma = 0;
	unsigned int code = 0;
	int ret = 0;

	if (aw_dev == NULL)
		return -EINVAL;

	if (aw_dev->ops.aw_i2c_write_bits == NULL)
		return -EINVAL;

	desc = &aw_dev->ipeak_desc;
	if (desc->mask == 0 || desc->reg == 0)
		return -EINVAL;

	if (desc->min_ma == 0 || desc->max_ma == 0 || desc->step_ma == 0)
		return -EINVAL;

	shift = desc->shift ? desc->shift : aw_dev_get_shift_from_mask(desc->mask);
	width = aw_dev_get_width_from_mask(desc->mask, shift);
	if (width == 0 || width >= 32)
		return -EINVAL;

	field_mask = aw_dev_get_field_mask(desc->mask);
	if (field_mask == 0)
		return -EINVAL;

	if (desc->max_ma < desc->min_ma)
		return -EINVAL;

	max_code = (1u << width) - 1u;
	max_valid_code = (desc->max_ma - desc->min_ma) / desc->step_ma;
	if (max_valid_code < max_code)
		max_code = max_valid_code;
	if (max_code == 0)
		return -EINVAL;

	target_ma = aw_dev_clamp_unsigned(ipeak_ma, desc->min_ma, desc->max_ma);

	code = (unsigned int)(((unsigned long long)(target_ma - desc->min_ma) + (desc->step_ma / 2)) / desc->step_ma);
	if (code > max_code)
		code = max_code;

	target_ma = desc->min_ma + code * desc->step_ma;

	ret = aw_dev->ops.aw_i2c_write_bits(aw_dev, desc->reg, desc->mask, code << shift);
	if (ret < 0)
		return ret;

	aw_dev_info(aw_dev->dev_index, "boost peak current %u mA (code %u)", target_ma, code);

	return 0;
}

int aw882xx_dev_get_boost_ipeak_ma(struct aw_device *aw_dev, unsigned int *ipeak_ma)
{
	struct aw_ipeak_desc *desc = NULL;
	unsigned int shift = 0;
	unsigned int width = 0;
	unsigned int field_mask = 0;
	unsigned int max_code = 0;
	unsigned int reg_val = 0;
	unsigned int code = 0;
	int ret = 0;

	if (aw_dev == NULL || ipeak_ma == NULL)
		return -EINVAL;

	if (aw_dev->ops.aw_i2c_read == NULL)
		return -EINVAL;

	desc = &aw_dev->ipeak_desc;
	if (desc->mask == 0 || desc->reg == 0)
		return -EINVAL;

	if (desc->min_ma == 0 || desc->step_ma == 0)
		return -EINVAL;

	shift = desc->shift ? desc->shift : aw_dev_get_shift_from_mask(desc->mask);
	width = aw_dev_get_width_from_mask(desc->mask, shift);
	if (width == 0 || width >= 32)
		return -EINVAL;

	field_mask = aw_dev_get_field_mask(desc->mask);
	if (field_mask == 0)
		return -EINVAL;

	ret = aw_dev->ops.aw_i2c_read(aw_dev, desc->reg, &reg_val);
	if (ret < 0)
		return ret;

	code = (reg_val & field_mask) >> shift;

	if (desc->max_ma > desc->min_ma)
		max_code = (desc->max_ma - desc->min_ma) / desc->step_ma;
	else
		max_code = (1u << width) - 1u;
	if (code > max_code)
		code = max_code;

	*ipeak_ma = desc->min_ma + code * desc->step_ma;
	if (desc->max_ma && *ipeak_ma > desc->max_ma)
		*ipeak_ma = desc->max_ma;

	return 0;
}

int aw882xx_dev_set_boost_voltage_uv(struct aw_device *aw_dev, unsigned int vout_uv)
{
	struct aw_vout_desc *desc = NULL;
	unsigned int shift = 0;
	unsigned int width = 0;
	unsigned int field_mask = 0;
	unsigned int max_code = 0;
	unsigned int code_limit = 0;
	unsigned int rel_code = 0;
	unsigned int code = 0;
	unsigned int applied_uv = 0;
	int ret = 0;

	if (aw_dev == NULL)
		return -EINVAL;

	if (aw_dev->ops.aw_i2c_write_bits == NULL)
		return -EINVAL;

	desc = &aw_dev->vout_desc;
	if (desc->mask == 0 || desc->reg == 0)
		return -EINVAL;

	if (desc->min_uv == 0 || desc->step_uv == 0)
		return -EINVAL;

	shift = desc->shift ? desc->shift : aw_dev_get_shift_from_mask(desc->mask);
	width = aw_dev_get_width_from_mask(desc->mask, shift);
	if (width == 0 || width >= 32)
		return -EINVAL;

	field_mask = aw_dev_get_field_mask(desc->mask);
	if (field_mask == 0)
		return -EINVAL;

	max_code = (1u << width) - 1u;
	if (desc->base_code > max_code)
		return -EINVAL;

	if (desc->max_uv > desc->min_uv) {
		unsigned long long diff_uv = (unsigned long long)(desc->max_uv - desc->min_uv);
		code_limit = (unsigned int)(diff_uv / desc->step_uv);
		if (desc->base_code + code_limit > max_code)
			code_limit = max_code - desc->base_code;
	} else {
		code_limit = max_code - desc->base_code;
	}

	if (desc->max_uv > desc->min_uv)
		vout_uv = aw_dev_clamp_unsigned(vout_uv, desc->min_uv, desc->max_uv);
	else {
		unsigned long long max_uv = (unsigned long long)desc->min_uv +
			(unsigned long long)desc->step_uv * code_limit;
		if (vout_uv < desc->min_uv)
			vout_uv = desc->min_uv;
		else if ((unsigned long long)vout_uv > max_uv)
			vout_uv = (unsigned int)max_uv;
	}

	rel_code = (unsigned int)(((unsigned long long)(vout_uv - desc->min_uv) + (desc->step_uv / 2)) / desc->step_uv);
	if (rel_code > code_limit)
		rel_code = code_limit;

	code = desc->base_code + rel_code;
	if (code > max_code)
		code = max_code;

	applied_uv = desc->min_uv + rel_code * desc->step_uv;

	ret = aw_dev->ops.aw_i2c_write_bits(aw_dev, desc->reg, desc->mask, code << shift);
	if (ret < 0)
		return ret;

	aw_dev_info(aw_dev->dev_index, "boost voltage %u uV (code %u)", applied_uv, code);

	return 0;
}

int aw882xx_dev_get_boost_voltage_uv(struct aw_device *aw_dev, unsigned int *vout_uv)
{
	struct aw_vout_desc *desc = NULL;
	unsigned int shift = 0;
	unsigned int width = 0;
	unsigned int field_mask = 0;
	unsigned int reg_val = 0;
	unsigned int code = 0;
	unsigned int rel_code = 0;
	unsigned int code_limit = 0;
	unsigned int max_code = 0;
	unsigned long long voltage = 0;
	int ret = 0;

	if (aw_dev == NULL || vout_uv == NULL)
		return -EINVAL;

	if (aw_dev->ops.aw_i2c_read == NULL)
		return -EINVAL;

	desc = &aw_dev->vout_desc;
	if (desc->mask == 0 || desc->reg == 0)
		return -EINVAL;

	if (desc->min_uv == 0 || desc->step_uv == 0)
		return -EINVAL;

	shift = desc->shift ? desc->shift : aw_dev_get_shift_from_mask(desc->mask);
	width = aw_dev_get_width_from_mask(desc->mask, shift);
	if (width == 0 || width >= 32)
		return -EINVAL;

	field_mask = aw_dev_get_field_mask(desc->mask);
	if (field_mask == 0)
		return -EINVAL;

	max_code = (1u << width) - 1u;
	if (desc->base_code > max_code)
		return -EINVAL;

	ret = aw_dev->ops.aw_i2c_read(aw_dev, desc->reg, &reg_val);
	if (ret < 0)
		return ret;

	code = (reg_val & field_mask) >> shift;

	if (code < desc->base_code)
		code = desc->base_code;

	if (desc->max_uv > desc->min_uv) {
		unsigned long long diff_uv = (unsigned long long)(desc->max_uv - desc->min_uv);
		code_limit = (unsigned int)(diff_uv / desc->step_uv);
		if (desc->base_code + code_limit < desc->base_code)
			code_limit = 0;
	} else {
		code_limit = max_code - desc->base_code;
	}

	if (code_limit > max_code - desc->base_code)
		code_limit = max_code - desc->base_code;

	if (code > desc->base_code + code_limit)
		code = desc->base_code + code_limit;

	rel_code = code - desc->base_code;

	voltage = (unsigned long long)desc->min_uv +
		(unsigned long long)rel_code * desc->step_uv;
	if (desc->max_uv > desc->min_uv && voltage > desc->max_uv)
		voltage = desc->max_uv;

	*vout_uv = (unsigned int)voltage;

	return 0;
}

static struct aw_sec_data_desc *aw_dev_get_prof_data_byname(struct aw_device *aw_dev, char *prof_name, int data_type)
{
	struct aw_sec_data_desc *sec_data = NULL;
	struct aw_prof_desc *prof_desc = NULL;
	struct aw_prof_info *prof_info = aw_dev->prof_info;
	int i = 0;

	if (data_type >= AW_PROFILE_DATA_TYPE_MAX) {
		aw_dev_err(aw_dev->dev_index, "unsupport data type id [%d]", data_type);
		return NULL;
	}

	for (i = 0; i < prof_info->count; i++) {
		if (strncmp(prof_name, prof_info->prof_desc[i].name, AW_PROF_NAME_MAX) == 0) {
			prof_desc = &aw_dev->prof_info->prof_desc[i];
			sec_data = &prof_desc->sec_desc[data_type];
			aw_dev_dbg(aw_dev->dev_index, "get prof[%s] data len[%d]",
								prof_desc->name, sec_data->len);
			return sec_data;
		}
	}

	aw_dev_err(aw_dev->dev_index, "not found prof_name[%s]", prof_name);
	return NULL;
}

static int aw_dev_check_profile_name(struct aw_device *aw_dev, const char *prof_name)
{
	int i =0;
	struct aw_prof_info *prof_info = aw_dev->prof_info;

	for (i = 0; i < prof_info->count; i++) {
		if (strncmp(prof_name, prof_info->prof_desc[i].name, AW_PROF_NAME_MAX) == 0){
			return 0;
		}
	}
	aw_dev_err(aw_dev->dev_index, "not found prof_name[%s]", prof_name);
	return -EINVAL;
}

int aw882xx_dev_set_profile_name(struct aw_device *aw_dev, const char *prof_name)
{
	if (aw_dev_check_profile_name(aw_dev, prof_name)) {
		return -EINVAL;
	} else {
		strncpy(aw_dev->set_prof_name, prof_name, AW_PROF_NAME_MAX - 1);
		aw_dev_info(aw_dev->dev_index, "set prof_name[%s]", aw_dev->set_prof_name);
	}

	return 0;
}

char *aw882xx_dev_get_profile_name(struct aw_device *aw_dev)
{
	return aw_dev->set_prof_name;
}

static int aw_dev_prof_init(struct aw_device *aw_dev, struct aw_init_info *init_info)
{
	int i =0;
	const char *first_prof_name = NULL;
	/*find profile*/
	for (i = 0; i < init_info->mix_chip_count; i++) {
		if (init_info->prof_info[i].chip_id == aw_dev->chip_id) {
			aw_dev->prof_info = &init_info->prof_info[i];
			first_prof_name = init_info->prof_info[i].prof_desc[0].name;
			strncpy(aw_dev->first_prof_name, first_prof_name, AW_PROF_NAME_MAX - 1);
			aw_dev_info(aw_dev->dev_index, "first prof_name[%s]", aw_dev->first_prof_name);
			return 0;
		}
	}

	aw_dev_err(aw_dev->dev_index, "no supported profile");
	return -EINVAL;
}

/*****************************awinic device*************************************/
/*pwd enable update reg*/
static int aw_dev_reg_fw_update(struct aw_device *aw_dev)
{
	int ret = -1;
	int i = 0;
	unsigned int reg_addr = 0;
	unsigned int reg_val = 0;
	unsigned int read_val;
	unsigned int init_volume = 0;
	unsigned int efcheck_val = 0;
	struct aw_int_desc *int_desc = &aw_dev->int_desc;
	struct aw_profctrl_desc *profctrl_desc = &aw_dev->profctrl_desc;
	struct aw_bstctrl_desc *bstctrl_desc = &aw_dev->bstctrl_desc;
	struct aw_sec_data_desc *reg_data = NULL;
	uint16_t *data = NULL;
	int data_len;

	reg_data = aw_dev_get_prof_data_byname(aw_dev, aw_dev->set_prof_name, AW_PROFILE_DATA_TYPE_REG);
	if (reg_data == NULL) {
		return -EINVAL;
	}

	data = (uint16_t *)reg_data->data;
	data_len = reg_data->len >> 1;

	for (i = 0; i < data_len; i += 2) {
		reg_addr = data[i];
		reg_val = data[i + 1];

		if (reg_addr == int_desc->mask_reg) {
			int_desc->int_mask = reg_val;
			reg_val = int_desc->mask_default;
		}

		if (aw_dev->bstcfg_enable) {
			if (reg_addr == profctrl_desc->reg) {
				profctrl_desc->cfg_prof_mode =
					reg_val & (~profctrl_desc->mask);
			}

			if (reg_addr == bstctrl_desc->reg) {
				bstctrl_desc->cfg_bst_type =
					reg_val & (~bstctrl_desc->mask);
			}
		}

		if (reg_addr == aw_dev->efcheck_desc.reg) {
			efcheck_val = reg_val & (~aw_dev->efcheck_desc.mask);
			if (efcheck_val == aw_dev->efcheck_desc.or_val)
				aw_dev->efuse_check = AW_EF_OR_CHECK;
			else
				aw_dev->efuse_check = AW_EF_AND_CHECK;

			aw_dev_info(aw_dev->dev_index, "efuse check: %d", aw_dev->efuse_check);
		}

		/*keep amppd status*/
		if (reg_addr == aw_dev->amppd_desc.reg) {
			aw_dev->amppd_st = reg_val & (~aw_dev->amppd_desc.mask);
			aw_dev_info(aw_dev->dev_index, "amppd_st=0x%04x", aw_dev->amppd_st);
			aw_dev->ops.aw_i2c_read(aw_dev,
			(unsigned char)reg_addr,
			(unsigned int *)&read_val);
			read_val &= (~aw_dev->amppd_desc.mask);
			reg_val &= aw_dev->amppd_desc.mask;
			reg_val |= read_val;
		}

		/*keep pwd status*/
		if (reg_addr == aw_dev->pwd_desc.reg) {
			aw_dev->ops.aw_i2c_read(aw_dev,
			(unsigned char)reg_addr,
			(unsigned int *)&read_val);
			read_val &= (~aw_dev->pwd_desc.mask);
			reg_val &= aw_dev->pwd_desc.mask;
			reg_val |= read_val;
		}
		/*keep mute status*/
		if (reg_addr == aw_dev->mute_desc.reg) {
			/*get bin value*/
			aw_dev->mute_st = reg_val & (~aw_dev->mute_desc.mask);
			aw_dev_info(aw_dev->dev_index, "mute_st=0x%04x", aw_dev->mute_st);
			aw_dev->ops.aw_i2c_read(aw_dev,
			(unsigned char)reg_addr,
			(unsigned int *)&read_val);
			read_val &= (~aw_dev->mute_desc.mask);
			reg_val &= aw_dev->mute_desc.mask;
			reg_val |= read_val;
		}
		if(reg_addr == aw_dev->dither_desc.reg) {
			aw_dev->dither_st = reg_val & (~aw_dev->dither_desc.mask);
			aw_dev_info(aw_dev->dev_index, "dither_st=0x%04x", aw_dev->dither_st);
		}
		if (reg_addr == aw_dev->vcalb_desc.vcalb_reg) {
			continue;
		}

		aw_dev_info(aw_dev->dev_index, "reg=0x%04x, val = 0x%04x",
			(uint16_t)reg_addr, (uint16_t)reg_val);
		ret = aw_dev->ops.aw_i2c_write(aw_dev,
			(unsigned char)reg_addr,
			(unsigned int)reg_val);
		if (ret < 0) {
			break;
		}
	}

	aw_dev->ops.aw_i2c_read(aw_dev, profctrl_desc->reg, &reg_val);
	reg_val = (reg_val & (~profctrl_desc->mask));
	if (reg_val == profctrl_desc->spk_mode) {
		aw_dev->spk_mode = AW_SPK_MODE;
	} else {
		aw_dev->spk_mode = AW_NOT_SPK_MODE;
	}
	aw_dev_info(aw_dev->dev_index, "reg0x%02x=0x%04x, spk_mode=%d", profctrl_desc->reg,
							reg_val, aw_dev->spk_mode);

	ret = aw_dev_set_vcalb(aw_dev);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev_index, "can't set vcalb");
		return ret;
	}

	aw_dev->ops.aw_get_volume(aw_dev, &init_volume);
	aw_dev->volume_desc.init_volume = init_volume;

	/*keep min volume*/
	if(aw_dev->fade_en) {
		aw_dev->ops.aw_set_volume(aw_dev, aw_dev->volume_desc.mute_volume);
	}

	aw_dev_info(aw_dev->dev_index, "load prof [%s] done", aw_dev->set_prof_name);

	return ret;
}

static void aw_dev_fade_in(struct aw_device *aw_dev)
{
	int i = 0;
	int fade_step = aw_dev->vol_step;
	struct aw_volume_desc *desc = &aw_dev->volume_desc;
	int fade_in_vol = desc->ctl_volume;

	if(!aw_dev->fade_en) {
		return;
	}

#ifdef AW_VOLUME
	if (fade_step == 0 || g_fade_in_time == 0) {
		aw882xx_dev_set_volume(aw_dev, fade_in_vol);
		return;
	}
	/*volume up*/
	for (i = desc->mute_volume; i >= fade_in_vol; i -= fade_step) {
		aw882xx_dev_set_volume(aw_dev, i);
		AW_MS_DELAY(g_fade_in_time);
	}
	if (i != fade_in_vol)
		aw882xx_dev_set_volume(aw_dev, fade_in_vol);
#endif

}

static void aw_dev_fade_out(struct aw_device *aw_dev)
{
	int i = 0;
	int fade_step = aw_dev->vol_step;
	struct aw_volume_desc *desc = &aw_dev->volume_desc;

	if(!aw_dev->fade_en) {
		return;
	}

#ifdef AW_VOLUME
	if (fade_step == 0 || g_fade_out_time == 0) {
		aw882xx_dev_set_volume(aw_dev, desc->mute_volume);
		return;
	}

	for (i = desc->ctl_volume; i <= desc->mute_volume; i += fade_step) {
		if (i > desc->mute_volume) {
			i = desc->mute_volume;
		}
		aw882xx_dev_set_volume(aw_dev, i);
		AW_MS_DELAY(g_fade_out_time);
	}
	if (i != desc->mute_volume) {
		aw882xx_dev_set_volume(aw_dev, desc->mute_volume);
		AW_MS_DELAY(g_fade_out_time);
	}
#endif

}

#ifdef AW_VOLUME
int aw882xx_dev_get_volume(struct aw_device *aw_dev, uint32_t *volume)
{
	int ret = 0;
	struct aw_volume_desc *desc = &aw_dev->volume_desc;
	unsigned int hw_vol = 0;

	ret = aw_dev->ops.aw_get_volume(aw_dev, &hw_vol);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev_index, "read volume failed");
		return ret;
	}

	*volume =  hw_vol - desc->init_volume;
	return ret;
}

int aw882xx_dev_set_volume(struct aw_device *aw_dev, uint32_t volume)
{
	int ret = 0;
	struct aw_volume_desc *desc = &aw_dev->volume_desc;
	unsigned int hw_vol = 0;

	if (volume > desc->mute_volume) {
		aw_dev_err(aw_dev->dev_index, "unsupported volume:%d", volume);
		return -EINVAL;
	}

	hw_vol = volume + desc->init_volume;

	ret = aw_dev->ops.aw_set_volume(aw_dev, hw_vol);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev_index, "write volume failed");
		return ret;
	}

	return ret;
}
#endif

static void aw_dev_pwd(struct aw_device *aw_dev, bool pwd)
{
	struct aw_pwd_desc *pwd_desc = &aw_dev->pwd_desc;

	aw_dev_dbg(aw_dev->dev_index, "enter, pwd: %d", pwd);

	if (pwd) {
		aw_dev->ops.aw_i2c_write_bits(aw_dev, pwd_desc->reg,
				pwd_desc->mask,
				pwd_desc->enable);
	} else {
		aw_dev->ops.aw_i2c_write_bits(aw_dev, pwd_desc->reg,
				pwd_desc->mask,
				pwd_desc->disable);
	}
	aw_dev_info(aw_dev->dev_index, "done");
}

static void aw_dev_amppd(struct aw_device *aw_dev, bool amppd)
{
	struct aw_amppd_desc *amppd_desc = &aw_dev->amppd_desc;

	aw_dev_dbg(aw_dev->dev_index, "enter, amppd: %d", amppd);

	if (amppd) {
		aw_dev->ops.aw_i2c_write_bits(aw_dev, amppd_desc->reg,
				amppd_desc->mask,
				amppd_desc->enable);
	} else {
		aw_dev->ops.aw_i2c_write_bits(aw_dev, amppd_desc->reg,
				amppd_desc->mask,
				amppd_desc->disable);
	}
	aw_dev_info(aw_dev->dev_index, "done");
}

static void aw_dev_mute(struct aw_device *aw_dev, bool mute)
{
	struct aw_mute_desc *mute_desc = &aw_dev->mute_desc;

	aw_dev_dbg(aw_dev->dev_index, "enter, mute: %d", mute);

	if (mute) {
		aw_dev_fade_out(aw_dev);
		aw_dev->ops.aw_i2c_write_bits(aw_dev, mute_desc->reg,
				mute_desc->mask,
				mute_desc->enable);
	} else {
		aw_dev->ops.aw_i2c_write_bits(aw_dev, mute_desc->reg,
				mute_desc->mask,
				mute_desc->disable);
		aw_dev_fade_in(aw_dev);
	}
	aw_dev_info(aw_dev->dev_index, "done");
}

static void aw_dev_uls_hmute(struct aw_device *aw_dev, bool uls_hmute)
{
	struct aw_uls_hmute_desc *uls_hmute_desc = &aw_dev->uls_hmute_desc;

	aw_dev_dbg(aw_dev->dev_index, "enter, uls_hmute: %d", uls_hmute);

	if (uls_hmute_desc->reg == AW_REG_NONE)
		return;

	if (uls_hmute) {
		aw_dev->ops.aw_i2c_write_bits(aw_dev, uls_hmute_desc->reg,
				uls_hmute_desc->mask,
				uls_hmute_desc->enable);
	} else {
		aw_dev->ops.aw_i2c_write_bits(aw_dev, uls_hmute_desc->reg,
				uls_hmute_desc->mask,
				uls_hmute_desc->disable);
	}
	aw_dev_info(aw_dev->dev_index, "done");
}

static void aw_dev_set_dither(struct aw_device *aw_dev, bool dither)
{
	struct aw_dither_desc *dither_desc = &aw_dev->dither_desc;

	aw_dev_dbg(aw_dev->dev_index, "enter, dither: %d", dither);

	if (dither_desc->reg == AW_REG_NONE)
		return;

	if (dither) {
		aw_dev->ops.aw_i2c_write_bits(aw_dev, dither_desc->reg,
				dither_desc->mask,
				dither_desc->enable);
	} else {
		aw_dev->ops.aw_i2c_write_bits(aw_dev, dither_desc->reg,
				dither_desc->mask,
				dither_desc->disable);
	}

	aw_dev_info(aw_dev->dev_index, "done");
}

static int aw_dev_get_icalk(struct aw_device *aw_dev, int16_t *icalk)
{
	int ret = -1;
	unsigned int reg_val = 0;
	uint16_t reg_icalk = 0;
	uint16_t reg_icalkl = 0;
	struct aw_vcalb_desc *desc = &aw_dev->vcalb_desc;

	if (desc->icalkl_reg == AW_REG_NONE) {
		ret = aw_dev->ops.aw_i2c_read(aw_dev, desc->icalk_reg, &reg_val);
		reg_icalk = (uint16_t)reg_val & (~desc->icalk_reg_mask);
	} else {
		ret = aw_dev->ops.aw_i2c_read(aw_dev, desc->icalk_reg, &reg_val);
		reg_icalk = (uint16_t)reg_val & (~desc->icalk_reg_mask);
		ret = aw_dev->ops.aw_i2c_read(aw_dev, desc->icalkl_reg, &reg_val);
		reg_icalkl = (uint16_t)reg_val & (~desc->icalkl_reg_mask);
		if (aw_dev->efuse_check == AW_EF_OR_CHECK)
			reg_icalk = (reg_icalk >> desc->icalk_shift) | (reg_icalkl >> desc->icalkl_shift);
		else
			reg_icalk = (reg_icalk >> desc->icalk_shift) & (reg_icalkl >> desc->icalkl_shift);

	}

	if (reg_icalk & (~desc->icalk_sign_mask)) {
		reg_icalk = reg_icalk | (~desc->icalk_neg_mask);
	}

	*icalk = (int16_t)reg_icalk;

	return ret;
}

static int aw_dev_get_vcalk(struct aw_device *aw_dev, int16_t *vcalk)
{
	int ret = -1;
	unsigned int reg_val = 0;
	uint16_t reg_vcalk = 0;
	uint16_t reg_vcalkl = 0;
	struct aw_vcalb_desc *desc = &aw_dev->vcalb_desc;

	if (desc->vcalkl_reg == AW_REG_NONE) {
		ret = aw_dev->ops.aw_i2c_read(aw_dev, desc->vcalk_reg, &reg_val);
		reg_vcalk = (uint16_t)reg_val & (~desc->vcalk_reg_mask);
	} else {
		ret = aw_dev->ops.aw_i2c_read(aw_dev, desc->vcalk_reg, &reg_val);
		reg_vcalk = (uint16_t)reg_val & (~desc->vcalk_reg_mask);
		ret = aw_dev->ops.aw_i2c_read(aw_dev, desc->vcalkl_reg, &reg_val);
		reg_vcalkl = (uint16_t)reg_val & (~desc->vcalkl_reg_mask);
		if (aw_dev->efuse_check == AW_EF_OR_CHECK)
			reg_vcalk = (reg_vcalk >> desc->vcalk_shift) | (reg_vcalkl >> desc->vcalkl_shift);
		else
			reg_vcalk = (reg_vcalk >> desc->vcalk_shift) & (reg_vcalkl >> desc->vcalkl_shift);
	}

	if (reg_vcalk & (~desc->vcalk_sign_mask)) {
		reg_vcalk = reg_vcalk | (~desc->vcalk_neg_mask);
	}
	*vcalk = (int16_t)reg_vcalk;

	return ret;
}

static int aw_dev_set_vcalb(struct aw_device *aw_dev)
{
	int ret = -1;
	unsigned int reg_val;
	int vcalb;
	int icalk;
	int vcalk;
	int16_t icalk_val = 0;
	int16_t vcalk_val = 0;

	struct aw_vcalb_desc *desc = &aw_dev->vcalb_desc;

	if (desc->icalk_reg == AW_REG_NONE || desc->vcalb_reg == AW_REG_NONE) {
		aw_dev_info(aw_dev->dev_index, "REG None !");
		return 0;
	}

	ret = aw_dev_get_icalk(aw_dev, &icalk_val);
	if (ret < 0) {
		return ret;
	}

	ret = aw_dev_get_vcalk(aw_dev, &vcalk_val);
	if (ret < 0) {
		return ret;
	}

	icalk = desc->cabl_base_value + desc->icalk_value_factor * icalk_val;
	vcalk = desc->cabl_base_value + desc->vcalk_value_factor * vcalk_val;
	if (!vcalk) {
		aw_dev_err(aw_dev->dev_index, "vcalk is 0");
		return -EINVAL;
	}

	vcalb = desc->vcal_factor * icalk / vcalk;

	reg_val = (unsigned int)vcalb;
	aw_dev_dbg(aw_dev->dev_index, "icalk=%d, vcalk=%d, vcalb=%d, reg_val=%d",
			icalk, vcalk, vcalb, reg_val);

	ret =  aw_dev->ops.aw_i2c_write(aw_dev, desc->vcalb_reg, reg_val);

	aw_dev_info(aw_dev->dev_index, "done");

	return ret;
}

int aw882xx_dev_get_int_status(struct aw_device *aw_dev, uint16_t *int_status)
{
	int ret = -1;
	unsigned int reg_val = 0;

	ret = aw_dev->ops.aw_i2c_read(aw_dev, aw_dev->int_desc.st_reg, &reg_val);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev_index, "read interrupt reg fail, ret=%d", ret);
	} else {
		*int_status = reg_val;
	}

	aw_dev_dbg(aw_dev->dev_index, "read interrupt reg = 0x%04x", *int_status);
	return ret;
}

void aw882xx_dev_clear_int_status(struct aw_device *aw_dev)
{
	uint16_t int_status = 0;

	/*read int status and clear*/
	aw882xx_dev_get_int_status(aw_dev, &int_status);
	/*make suer int status is clear*/
	aw882xx_dev_get_int_status(aw_dev, &int_status);
	aw_dev_info(aw_dev->dev_index, "done");
}

int aw882xx_dev_set_intmask(struct aw_device *aw_dev, bool flag)
{
	int ret = -1;
	struct aw_int_desc *desc = &aw_dev->int_desc;

	if (flag) {
		ret = aw_dev->ops.aw_i2c_write(aw_dev, desc->mask_reg,
					desc->int_mask);
	} else {
		ret = aw_dev->ops.aw_i2c_write(aw_dev, desc->mask_reg,
					desc->mask_default);
	}
	aw_dev_info(aw_dev->dev_index, "done");
	return ret;
}

static int aw_dev_mode1_pll_check(struct aw_device *aw_dev)
{
	int ret = -1;
	unsigned char i;
	unsigned int reg_val = 0;
	struct aw_sysst_desc *desc = &aw_dev->sysst_desc;

	for (i = 0; i < AW_DEV_SYSST_CHECK_MAX; i++) {
		aw_dev->ops.aw_i2c_read(aw_dev, desc->reg, &reg_val);
		if (reg_val & desc->pll_check) {
			ret = 0;
			break;
		} else {
			aw_dev_dbg(aw_dev->dev_index, "check pll lock fail, cnt=%d, reg_val=0x%04x",
					i, reg_val);
			AW_MS_DELAY(AW_2_MS);
		}
	}
	if (ret < 0) {
		aw_dev_err(aw_dev->dev_index, "check fail");
	} else {
		aw_dev_info(aw_dev->dev_index, "done");
	}

	return ret;
}

static int aw_dev_mode2_pll_check(struct aw_device *aw_dev)
{
	int ret = -1;
	unsigned int reg_val = 0;
	struct aw_cco_mux_desc *cco_mux_desc = &aw_dev->cco_mux_desc;

	aw_dev->ops.aw_i2c_read(aw_dev, cco_mux_desc->reg, &reg_val);
	reg_val &= (~cco_mux_desc->mask);
	aw_dev_dbg(aw_dev->dev_index, "REG_PLLCTRL1_bit14 = 0x%04x", reg_val);
	if (reg_val == cco_mux_desc->divided_val) {
		aw_dev_dbg(aw_dev->dev_index, "CCO_MUX is already divided");
		return ret;
	}

	/* change mode2 */
	aw_dev->ops.aw_i2c_write_bits(aw_dev, cco_mux_desc->reg,
				cco_mux_desc->mask, cco_mux_desc->bypass_val);
	ret = aw_dev_mode1_pll_check(aw_dev);

	/* change mode1 */
	aw_dev->ops.aw_i2c_write_bits(aw_dev, cco_mux_desc->reg,
				cco_mux_desc->mask, cco_mux_desc->divided_val);
	if (ret == 0) {
		AW_MS_DELAY(AW_2_MS);
		ret = aw_dev_mode1_pll_check(aw_dev);
	}

	return ret;
}

static int aw_dev_syspll_check(struct aw_device *aw_dev)
{
	int ret = -1;

	ret = aw_dev_mode1_pll_check(aw_dev);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev_index,
			"mode1 check iis failed try switch to mode2 check");

		ret= aw_dev_mode2_pll_check(aw_dev);
		if (ret < 0) {
			aw_dev_err(aw_dev->dev_index, "mode2 check iis failed");
		}
	}

	return ret;
}

static int aw_dev_sysst_check(struct aw_device *aw_dev)
{
	int ret = -1;
	unsigned char i;
	unsigned int reg_val = 0;
	struct aw_sysst_desc *desc = &aw_dev->sysst_desc;

	for (i = 0; i < AW_DEV_SYSST_CHECK_MAX; i++) {
		aw_dev->ops.aw_i2c_read(aw_dev, desc->reg, &reg_val);
		if (((reg_val & (~desc->mask)) & desc->st_check) == desc->st_check) {
			ret = 0;
			break;
		} else {
			aw_dev_err(aw_dev->dev_index, "SYSST mismatch cnt=%d val=0x%04x", i, reg_val);
			AW_MS_DELAY(AW_2_MS);
		}
	}
	if (ret < 0) {
		aw_dev_err(aw_dev->dev_index, "SYSST check failed after %d tries (last=0x%04x)",
			AW_DEV_SYSST_CHECK_MAX, reg_val);
	} else {
		aw_dev_info(aw_dev->dev_index, "done");
	}

	return ret;
}

#ifdef AW_FADE
int aw882xx_dev_get_fade_vol_step(struct aw_device *aw_dev)
{
	aw_dev_dbg(aw_dev->dev_index, "enter");
	return aw_dev->vol_step;
}

void aw882xx_dev_set_fade_vol_step(struct aw_device *aw_dev, unsigned int step)
{
	aw_dev_dbg(aw_dev->dev_index, "enter");
	aw_dev->vol_step = step;
}

void aw882xx_dev_get_fade_time(unsigned int *time, bool fade_in)
{
	if (fade_in) {
		*time = g_fade_in_time;
	} else {
		*time = g_fade_out_time;
	}
}

void aw882xx_dev_set_fade_time(unsigned int time, bool fade_in)
{
	if (fade_in) {
		g_fade_in_time = time;
	} else {
		g_fade_out_time = time;
	}
}
#endif

#ifdef AW_IRQ
void aw882xx_dev_interrupt_clear(struct aw_device *aw_dev)
{
	unsigned int reg_val = 0;

	aw_dev->ops.aw_i2c_read(aw_dev, aw_dev->sysst_desc.reg, &reg_val);
	aw_dev_info(aw_dev->dev_index,"reg SYSST=0x%x", reg_val);
	aw_dev->ops.aw_i2c_read(aw_dev, aw_dev->int_desc.st_reg, &reg_val);
	aw_dev_info(aw_dev->dev_index,"reg SYSINT=0x%x", reg_val);
	aw_dev->ops.aw_i2c_read(aw_dev, aw_dev->int_desc.mask_reg, &reg_val);
	aw_dev_info(aw_dev->dev_index,"reg SYSINTM=0x%x", reg_val);
}
#endif

int aw882xx_dev_status(struct aw_device *aw_dev)
{
	return aw_dev->status;
}

void aw882xx_dev_soft_reset(struct aw_device *aw_dev)
{
	struct aw_soft_rst *reset = &aw_dev->soft_rst;

	aw_dev->ops.aw_i2c_write(aw_dev, reset->reg, reset->reg_value);
	aw_dev_info(aw_dev->dev_index, "soft reset done");
}

int aw882xx_device_irq_reinit(struct aw_device *aw_dev)
{
	int ret = -1;

	/*reg re load*/
	ret = aw_dev_reg_fw_update(aw_dev);
	if (ret < 0) {
		return ret;
	}

	/*update vcalb*/
	aw_dev_set_vcalb(aw_dev);

	return 0;
}

static int aw_device_init(struct aw_device *aw_dev)
{
	int ret =- 1;

	if (aw_dev == NULL) {
		aw_pr_err("pointer is NULL");
		return -ENOMEM;
	}

	aw882xx_dev_soft_reset(aw_dev);

	strncpy(aw_dev->cur_prof_name, aw_dev->first_prof_name, AW_PROF_NAME_MAX - 1);
	strncpy(aw_dev->set_prof_name, aw_dev->first_prof_name, AW_PROF_NAME_MAX - 1);

	ret = aw_dev_reg_fw_update(aw_dev);
	if (ret < 0) {
		return ret;
	}

	aw_dev->status = AW_DEV_PW_ON;

	aw882xx_device_stop(aw_dev);

	aw_dev_info(aw_dev->dev_index, "init done");
	return 0;
}

int aw882xx_dev_reg_update(struct aw_device *aw_dev, bool force)
{
	int ret = -1;

	if (force) {
		aw882xx_dev_soft_reset(aw_dev);
		ret = aw_dev_reg_fw_update(aw_dev);
		if (ret < 0) {
			return ret;
		}
	} else {
		if (strncmp(aw_dev->cur_prof_name, aw_dev->set_prof_name,  AW_PROF_NAME_MAX) != 0) {
			ret = aw_dev_reg_fw_update(aw_dev);
			if (ret < 0) {
				return ret;
			}
		}
	}

	aw_dev_info(aw_dev->dev_index, "cur_prof=%s", aw_dev->cur_prof_name);

	strncpy(aw_dev->cur_prof_name, aw_dev->set_prof_name, AW_PROF_NAME_MAX - 1);

	aw_dev_info(aw_dev->dev_index, "done");
	return 0;
}

int aw882xx_dev_prof_update(struct aw_device *aw_dev, bool force)
{
	int ret = -1;

	/*if power on need off -- load -- on*/
	if (aw_dev->status == AW_DEV_PW_ON) {
		aw882xx_device_stop(aw_dev);

		ret = aw882xx_dev_reg_update(aw_dev, force);
		if (ret) {
			aw_dev_err(aw_dev->dev_index, "fw update failed ");
			return ret;
		}

		ret = aw882xx_device_start(aw_dev);
		if (ret) {
			aw_dev_err(aw_dev->dev_index, "start failed ");
			return ret;
		}
	} else {
		/*if pa off , only update set_prof value*/
		aw_dev_info(aw_dev->dev_index, "set prof[%s] done !", aw_dev->set_prof_name);
	}

	aw_dev_info(aw_dev->dev_index, "update done !");
	return 0;
}

static void aw_dev_boost_type_set(struct aw_device *aw_dev)
{
	struct aw_profctrl_desc *profctrl_desc = &aw_dev->profctrl_desc;
	struct aw_bstctrl_desc *bstctrl_desc = &aw_dev->bstctrl_desc;

	aw_dev_dbg(aw_dev->dev_index, "enter");

	if (aw_dev->bstcfg_enable) {
		/*set spk mode*/
		aw_dev->ops.aw_i2c_write_bits(aw_dev, profctrl_desc->reg,
				profctrl_desc->mask, profctrl_desc->spk_mode);

		/*force boost*/
		aw_dev->ops.aw_i2c_write_bits(aw_dev, bstctrl_desc->reg,
				bstctrl_desc->mask, bstctrl_desc->frc_bst);

		aw_dev_dbg(aw_dev->dev_index, "boost type set done");
	}
}

static void aw_dev_boost_type_recover(struct aw_device *aw_dev)
{
	struct aw_profctrl_desc *profctrl_desc = &aw_dev->profctrl_desc;
	struct aw_bstctrl_desc *bstctrl_desc = &aw_dev->bstctrl_desc;

	aw_dev_dbg(aw_dev->dev_index, "enter");

	aw_dev_info(aw_dev->dev_index, "DEBUG: bstcfg_enable=%d, reg=0x%02x, mask=0x%04x",
	            aw_dev->bstcfg_enable, bstctrl_desc->reg, bstctrl_desc->mask);

	aw_dev_info(aw_dev->dev_index, "DEBUG: tsp_type=0x%04x, cfg_bst_type=0x%04x", 
	            bstctrl_desc->tsp_type, bstctrl_desc->cfg_bst_type);				


	if (aw_dev->bstcfg_enable) {
		/*set transprant*/
		aw_dev->ops.aw_i2c_write_bits(aw_dev, bstctrl_desc->reg,
				bstctrl_desc->mask, bstctrl_desc->tsp_type);

		AW_MS_DELAY(AW_5_MS);
		/*set cfg boost type*/
		aw_dev->ops.aw_i2c_write_bits(aw_dev, bstctrl_desc->reg,
				bstctrl_desc->mask, bstctrl_desc->cfg_bst_type);

		/*set cfg prof mode*/
		aw_dev->ops.aw_i2c_write_bits(aw_dev, profctrl_desc->reg,
				profctrl_desc->mask, profctrl_desc->cfg_prof_mode);

		aw_dev_dbg(aw_dev->dev_index, "boost type recover done");
	}
}

int aw882xx_dev_reg_dump(struct aw_device *aw_dev)
{
	int reg_num = aw_dev->ops.aw_get_reg_num();
	uint8_t i = 0;
	uint32_t reg_val = 0;

	for (i = 0; i < reg_num; i++) {
		if (aw_dev->ops.aw_check_rd_access(i)) {
			aw_dev->ops.aw_i2c_read(aw_dev, i, &reg_val);
			aw_dev_info(aw_dev->dev_index, "read: reg = 0x%02x, val = 0x%04x",
				i, reg_val);
		}
	}

	return 0;
}

int aw882xx_device_start(struct aw_device *aw_dev)
{
	int ret = -1;
	struct aw_dither_desc *dither_desc = &aw_dev->dither_desc;

	aw_dev_dbg(aw_dev->dev_index, "enter");

	if (aw_dev->status == AW_DEV_PW_ON) {
		aw_dev_info(aw_dev->dev_index, "already power on");
		return 0;
	}

	/*set froce boost*/
	aw_dev_boost_type_set(aw_dev);

	aw_dev_set_dither(aw_dev, false);

	/*power on*/
	aw_dev_pwd(aw_dev, false);
	AW_MS_DELAY(AW_2_MS);

	ret = aw_dev_syspll_check(aw_dev);
	if (ret < 0) {
		aw882xx_dev_reg_dump(aw_dev);
		aw_dev_pwd(aw_dev, true);
		aw_dev_dbg(aw_dev->dev_index, "pll check failed cannot start");
		return ret;
	}

	/*amppd on*/
	aw_dev_amppd(aw_dev, false);
	AW_MS_DELAY(AW_1_MS);

	/*check i2s status*/
	ret = aw_dev_sysst_check(aw_dev);
	if (ret < 0) {
		aw882xx_dev_reg_dump(aw_dev);
		/*close tx feedback*/
		if (aw_dev->ops.aw_i2s_enable) {
			aw_dev->ops.aw_i2s_enable(aw_dev, false);
		}
		/*clear interrupt*/
		aw882xx_dev_clear_int_status(aw_dev);
		/*close amppd*/
		aw_dev_amppd(aw_dev, true);
		/*power down*/
		aw_dev_pwd(aw_dev, true);
		return -EINVAL;
	}

	/*boost type recover*/
	aw_dev_boost_type_recover(aw_dev);

	/*enable tx feedback*/
	if (aw_dev->ops.aw_i2s_enable) {
		aw_dev->ops.aw_i2s_enable(aw_dev, true);
	}

	if (aw_dev->amppd_st) {
		aw_dev_amppd(aw_dev, true);
	}

	if (aw_dev->ops.aw_reg_force_set) {
		aw_dev->ops.aw_reg_force_set(aw_dev);
	}

	/*close uls hmute*/
	aw_dev_uls_hmute(aw_dev, false);

	if(aw_dev->dither_st == dither_desc->enable) {
		aw_dev_set_dither(aw_dev, true);
	}

	if (!aw_dev->mute_st) {
		/*close mute*/
		aw_dev_mute(aw_dev, false);
		
		/* CRITICAL: Wait for audio data to flow and boost to stabilize
		 * SmartPA needs actual audio signal (not just I2S clocks) to start boost.
		 * Give it time to detect audio and start boost/switch circuits.
		 */
		AW_MS_DELAY(50);
		
		// DEBUG: Dump all critical registers after unmute to diagnose no sound issue
		unsigned int reg_val = 0;
		if (aw_dev->ops.aw_i2c_read) {
			aw_dev->ops.aw_i2c_read(aw_dev, 0x01, &reg_val);  // SYSST
			aw_dev_info(aw_dev->dev_index, "DEBUG: SYSST(0x01) = 0x%04x (PLLS=%d,CLKS=%d)", 
			            reg_val, reg_val&1, (reg_val>>4)&1);

			aw_dev_info(aw_dev->dev_index, "DEBUG: SYSST(0x01) = 0x%04x (SWS=%d,BSTS=%d,UVLS=%d)", 
			            (reg_val>>4)&1, (reg_val>>8)&1, (reg_val>>9)&1, (reg_val>>13)&1);
			
			aw_dev->ops.aw_i2c_read(aw_dev, 0x04, &reg_val);  // SYSCTRL
			aw_dev_info(aw_dev->dev_index, "DEBUG: SYSCTRL(0x04) = 0x%04x (PWDN=%d,AMPPD=%d,I2SEN=%d)", 
			            reg_val, reg_val&1, (reg_val>>1)&1, (reg_val>>6)&1);
			aw_dev_info(aw_dev->dev_index, "DEBUG: SYSCTRL(0x04) = 0x%04x (HMUTE=%d,HDCCE=%d,ULS_HMUTE=%d)", 
			            reg_val, (reg_val>>8)&1, (reg_val>>10)&1, (reg_val>>14)&1);
			
			aw_dev->ops.aw_i2c_read(aw_dev, 0x05, &reg_val);  // SYSCTRL2 (volume)
			aw_dev_info(aw_dev->dev_index, "DEBUG: SYSCTRL2(0x05) = 0x%04x (VOL=%d)", reg_val, reg_val&0xFF);
			
			aw_dev->ops.aw_i2c_read(aw_dev, 0x60, &reg_val);  // BSTCTRL1
			aw_dev_info(aw_dev->dev_index, "DEBUG: BSTCTRL1(0x60) = 0x%04x (BST_MODE=%d)", reg_val, reg_val&0x3);
			
			aw_dev->ops.aw_i2c_read(aw_dev, 0x61, &reg_val);  // BSTCTRL2
			aw_dev_info(aw_dev->dev_index, "DEBUG: BSTCTRL2(0x61) = 0x%04x", reg_val);
		}
	}

	/*clear inturrupt*/
	aw882xx_dev_clear_int_status(aw_dev);
	/*set inturrupt mask*/
	aw882xx_dev_set_intmask(aw_dev, true);

	if (aw_dev->ops.aw_monitor_start) {
		aw_dev->ops.aw_monitor_start((void *)aw_dev);
	}

	aw_dev->status = AW_DEV_PW_ON;
	aw_dev_info(aw_dev->dev_index, "done");
	return 0;
}

int aw882xx_device_stop(struct aw_device *aw_dev)
{
	aw_dev_dbg(aw_dev->dev_index, "enter");

	if (aw_dev->status == AW_DEV_PW_OFF) {
		aw_dev_dbg(aw_dev->dev_index, "already power off");
		return 0;
	}

	aw_dev->status = AW_DEV_PW_OFF;

	if (aw_dev->ops.aw_monitor_stop) {
		aw_dev->ops.aw_monitor_stop((void *)aw_dev);
	}

	/*clear interrupt*/
	aw882xx_dev_clear_int_status(aw_dev);

	/*set defaut int mask*/
	aw882xx_dev_set_intmask(aw_dev, false);

	/*set uls hmute*/
	aw_dev_uls_hmute(aw_dev, true);

	/*set mute*/
	aw_dev_mute(aw_dev, true);
	AW_MS_DELAY(AW_5_MS);

	/*close tx feedback*/
	if (aw_dev->ops.aw_i2s_enable) {
		aw_dev->ops.aw_i2s_enable(aw_dev, false);
	}

	AW_MS_DELAY(AW_1_MS);

	/*enable amppd*/
	aw_dev_amppd(aw_dev, true);

	/*set power down*/
	aw_dev_pwd(aw_dev, true);

	aw_dev_info(aw_dev->dev_index, "done");
	return 0;
}


int aw882xx_device_probe(struct aw_device *aw_dev, struct aw_init_info *init_info)
{
	int ret = -1;

	ret = aw_dev_prof_init(aw_dev, init_info);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev_index, "prof init failed");
		return ret;
	}

	aw882xx_dev_get_monitor_func(aw_dev);

	if(aw_dev->ops.aw_monitor_init) {
		ret = aw_dev->ops.aw_monitor_init((void *)aw_dev);
		if (ret < 0) {
			aw_dev_err(aw_dev->dev_index, "monitor init failed");
			return ret;
		}
	}

	ret = aw_device_init(aw_dev);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev_index, "dev init failed");
		return ret;
	}

	return 0;
}

