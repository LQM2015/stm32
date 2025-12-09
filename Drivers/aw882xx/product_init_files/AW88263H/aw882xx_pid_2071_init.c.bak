#include <stdio.h>
#include <stdlib.h>

#include "aw882xx.h"
#include "aw882xx_pid_2071_reg.h"
#include "aw882xx_base.h"

static int aw882xx_dev_i2c_write_bits(struct aw_device *aw_dev,
	unsigned char reg_addr, unsigned int mask, unsigned int reg_data)
{
	struct aw882xx *aw882xx = (struct aw882xx *)aw_dev->private_data;

	return aw882xx_i2c_write_bits(aw882xx, reg_addr, mask, reg_data);
}

static int aw882xx_dev_i2c_write(struct aw_device *aw_dev,
	unsigned char reg_addr, unsigned int reg_data)
{
	struct aw882xx *aw882xx = (struct aw882xx *)aw_dev->private_data;

	return aw882xx_i2c_write(aw882xx, reg_addr, reg_data);
}

static int aw882xx_dev_i2c_read(struct aw_device *aw_dev,
	unsigned char reg_addr, unsigned int *reg_data)
{
	struct aw882xx *aw882xx = (struct aw882xx *)aw_dev->private_data;

	return aw882xx_i2c_read(aw882xx, reg_addr, reg_data);
}

/* [9 : 6]: -6DB ; [5 : 0]: 0.125DB  real_value = value * 8 : 0.125db --> 1 */
static unsigned int aw_pid_2071_reg_val_to_db(unsigned int value)
{
	return ((value >> 6) * AW_PID_2071_VOL_STEP_DB + (value & 0x3f));
}

/* [9 : 6]: -6DB ; [5 : 0]: 0.125DB reg_value = value / step << 6 + value % step ; step = 6 * 8 */
static unsigned int aw_pid_2071_db_val_to_reg(unsigned int value)
{
	return (((value / AW_PID_2071_VOL_STEP_DB) << 6) + (value % AW_PID_2071_VOL_STEP_DB));
}


static int aw_pid_2071_set_volume(struct aw_device *aw_dev, unsigned int value)
{
	struct aw882xx *aw882xx = (struct aw882xx *)aw_dev->private_data;
	unsigned int reg_value = 0;
	unsigned int real_value = aw_pid_2071_db_val_to_reg(value);

	/* cal real value */
	aw882xx_i2c_read(aw882xx, AW_PID_2071_SYSCTRL2_REG, &reg_value);

	aw_dev_dbg(aw882xx->dev_index, "value %d , 0x%x", value, real_value);

	/* [15 : 6] volume */
	real_value = (real_value << 6) | (reg_value & 0x003f);

	/* write value */
	aw882xx_i2c_write(aw882xx, AW_PID_2071_SYSCTRL2_REG, real_value);
	return 0;
}

static int aw_pid_2071_get_volume(struct aw_device *aw_dev, unsigned int *value)
{
	unsigned int reg_value = 0;
	unsigned int real_value = 0;
	struct aw882xx *aw882xx = (struct aw882xx *)aw_dev->private_data;

	/* read value */
	aw882xx_i2c_read(aw882xx, AW_PID_2071_SYSCTRL2_REG, &reg_value);

	/* [15 : 6] volume */
	real_value = reg_value >> 6;

	real_value = aw_pid_2071_reg_val_to_db(real_value);
	*value = real_value;

	return 0;
}

static void aw_pid_2071_i2s_enable(struct aw_device *aw_dev, bool flag)
{
	struct aw882xx *aw882xx = (struct aw882xx *)aw_dev->private_data;
	aw_dev_dbg(aw882xx->dev_index, "enter");

	if (flag) {
		aw882xx_i2c_write_bits(aw882xx, AW_PID_2071_I2SCFG1_REG,
				AW_PID_2071_I2STXEN_MASK,
				AW_PID_2071_I2STXEN_ENABLE_VALUE);
	} else {
		aw882xx_i2c_write_bits(aw882xx, AW_PID_2071_I2SCFG1_REG,
				AW_PID_2071_I2STXEN_MASK,
				AW_PID_2071_I2STXEN_DISABLE_VALUE);
	}
}

static bool aw_pid_2071_check_rd_access(int reg)
{
	if (reg >= AW_PID_2071_REG_MAX) {
		return false;
	}
	if (aw_pid_2071_reg_access[reg] & AW_PID_2071_REG_RD_ACCESS) {
		return true;
	} else {
		return false;
	}
}

static bool aw_pid_2071_check_wr_access(int reg)
{
	if (reg >= AW_PID_2071_REG_MAX) {
		return false;
	}
	if (aw_pid_2071_reg_access[reg] & AW_PID_2071_REG_WR_ACCESS) {
		return true;
	} else {
		return false;
	}
}

static int aw_pid_2071_get_reg_num(void)
{
	return AW_PID_2071_REG_MAX;
}

static unsigned int aw_pid_2071_get_irq_type(struct aw_device *aw_dev,
					unsigned int value)
{
	unsigned int ret = INT_TYPE_NONE;

	/* UVL0 */
	if (value & (~AW_PID_2071_UVLI_MASK)) {
		aw_dev_info(aw_dev->dev_index, "UVLO: occur");
		ret |= INT_TYPE_UVLO;
	}

	/* BSTOCM */
	if (value & (~AW_PID_2071_BSTOCI_MASK)) {
		aw_dev_info(aw_dev->dev_index, "BSTOCI: occur");
		ret |= INT_TYPE_BSTOC;
	}

	/* OCDI */
	if (value & (~AW_PID_2071_OCDI_MASK)) {
		aw_dev_info(aw_dev->dev_index, "OCDI: occur");
		ret |= INT_TYPE_OCDI;
	}

	/* OTHI */
	if (value & (~AW_PID_2071_OTHI_MASK)) {
		aw_dev_info(aw_dev->dev_index, "OTHI: occur");
		ret |= INT_TYPE_OTHI;
	}

	return ret;
}


int aw882xx_pid_2071_dev_init(void *aw882xx_val)
{
	int ret = -1;
	struct aw_device *aw_pa = NULL;
	struct aw882xx *aw882xx = (struct aw882xx *)aw882xx_val;

	aw_pr_dbg("enter");

	if (aw882xx == NULL) {
		aw_pr_err("aw882xx is null");
		return -EINVAL;
	}

	if (aw882xx->chip_id != PID_2071_ID) {
		aw_dev_dbg(aw882xx->dev_index, "The id:0x%x does not match:0x%x",
					aw882xx->chip_id, PID_2071_ID);
		return -EINVAL;
	}

	aw_pa = calloc(1, sizeof(struct aw_device));
	if (aw_pa == NULL) {
		aw_dev_err(aw882xx->dev_index, "dev kalloc failed");
		return -ENOMEM;
	}

	/* call aw device init func */
	aw_pa->prof_info = NULL;
	aw_pa->bop_en = AW_BOP_DISABLE;
	aw_pa->vol_step = AW_PID_2071_VOL_STEP;
	aw_pa->chip_id = aw882xx->chip_id;
	aw_pa->dev_index = aw882xx->dev_index;
	aw_pa->private_data = (void *)aw882xx;
	aw_pa->bstcfg_enable = AW_BSTCFG_DISABLE;
	aw_pa->ops.aw_get_version = aw882xx_get_version;
	aw_pa->ops.aw_get_dev_num = aw882xx_get_dev_num;
	aw_pa->ops.aw_i2c_read = aw882xx_dev_i2c_read;
	aw_pa->ops.aw_i2c_write = aw882xx_dev_i2c_write;
	aw_pa->ops.aw_i2c_write_bits = aw882xx_dev_i2c_write_bits;
	aw_pa->ops.aw_get_volume = aw_pid_2071_get_volume;
	aw_pa->ops.aw_set_volume = aw_pid_2071_set_volume;
	aw_pa->ops.aw_reg_val_to_db = aw_pid_2071_reg_val_to_db;
	aw_pa->ops.aw_i2s_enable = aw_pid_2071_i2s_enable;
	aw_pa->ops.aw_check_rd_access = aw_pid_2071_check_rd_access;
	aw_pa->ops.aw_check_wr_access = aw_pid_2071_check_wr_access;
	aw_pa->ops.aw_get_reg_num = aw_pid_2071_get_reg_num;
	aw_pa->ops.aw_get_irq_type = aw_pid_2071_get_irq_type;

	aw_pa->int_desc.mask_reg = AW_PID_2071_SYSINTM_REG;
	aw_pa->int_desc.mask_default = AW_PID_2071_SYSINTM_DEFAULT;
	aw_pa->int_desc.int_mask = AW_PID_2071_SYSINTM_DEFAULT;
	aw_pa->int_desc.st_reg = AW_PID_2071_SYSINT_REG;
	aw_pa->int_desc.uvl_val = AW_PID_2071_UVLS_VDD_BELOW_2P8V_VALUE;
	aw_pa->int_desc.clock_val = AW_PID_2071_NOCLKS_CLOCK_OK_VALUE;
	aw_pa->int_desc.clks_val = AW_PID_2071_CLKS_STABLE_VALUE;

	aw_pa->pwd_desc.reg = AW_PID_2071_SYSCTRL_REG;
	aw_pa->pwd_desc.mask = AW_PID_2071_PWDN_MASK;
	aw_pa->pwd_desc.enable = AW_PID_2071_PWDN_POWER_DOWN_VALUE;
	aw_pa->pwd_desc.disable = AW_PID_2071_PWDN_WORKING_VALUE;

	aw_pa->amppd_desc.reg = AW_PID_2071_SYSCTRL_REG;
	aw_pa->amppd_desc.mask = AW_PID_2071_AMPPD_MASK;
	aw_pa->amppd_desc.enable = AW_PID_2071_AMPPD_POWER_DOWN_VALUE;
	aw_pa->amppd_desc.disable = AW_PID_2071_AMPPD_WORKING_VALUE;

	aw_pa->mute_desc.reg = AW_PID_2071_SYSCTRL2_REG;
	aw_pa->mute_desc.mask = AW_PID_2071_HMUTE_MASK;
	aw_pa->mute_desc.enable = AW_PID_2071_HMUTE_ENABLE_VALUE;
	aw_pa->mute_desc.disable = AW_PID_2071_HMUTE_DISABLE_VALUE;

	aw_pa->uls_hmute_desc.reg = AW_REG_NONE;

	aw_pa->vcalb_desc.vcalb_reg = AW_PID_2071_VTMCTRL3_REG;
	aw_pa->vcalb_desc.vcal_factor = AW_PID_2071_VCAL_FACTOR;
	aw_pa->vcalb_desc.cabl_base_value = AW_PID_2071_CABL_BASE_VALUE;

	aw_pa->vcalb_desc.icalk_value_factor = AW_PID_2071_ICABLK_FACTOR;
	aw_pa->vcalb_desc.icalk_reg = AW_PID_2071_EFRH_REG;
	aw_pa->vcalb_desc.icalk_reg_mask = AW_PID_2071_EF_VSN_OFFSET_MASK;
	aw_pa->vcalb_desc.icalk_shift = AW_PID_2071_ICALK_SHIFT;
	aw_pa->vcalb_desc.icalkl_reg = AW_PID_2071_EFRM1_REG;
	aw_pa->vcalb_desc.icalkl_reg_mask = AW_PID_2071_EF_ISN_OFFSET_MASK;
	aw_pa->vcalb_desc.icalkl_shift = AW_PID_2071_ICALKL_SHIFT;
	aw_pa->vcalb_desc.icalk_sign_mask = AW_PID_2071_EF_ISN_GESLP_SIGN_MASK;
	aw_pa->vcalb_desc.icalk_neg_mask = AW_PID_2071_EF_ISN_GESLP_NEG;

	aw_pa->vcalb_desc.vcalk_reg = AW_PID_2071_EFRH_REG;
	aw_pa->vcalb_desc.vcalk_reg_mask = AW_PID_2071_EF_VSN_GESLP_MASK;
	aw_pa->vcalb_desc.vcalkl_reg = AW_REG_NONE;
	aw_pa->vcalb_desc.vcalk_sign_mask = AW_PID_2071_EF_VSN_GESLP_SIGN_MASK;
	aw_pa->vcalb_desc.vcalk_neg_mask = AW_PID_2071_EF_VSN_GESLP_NEG;
	aw_pa->vcalb_desc.vcalk_value_factor = AW_PID_2071_VCABLK_FACTOR;

	aw_pa->sysst_desc.reg = AW_PID_2071_SYSST_REG;
	aw_pa->sysst_desc.mask = AW_PID_2071_SYSST_CHECK_MASK;
	aw_pa->sysst_desc.st_check = AW_PID_2071_SYSST_CHECK;
	aw_pa->sysst_desc.pll_check = AW_PID_2071_PLLS_LOCKED_VALUE;

	aw_pa->profctrl_desc.reg = AW_PID_2071_SYSCTRL_REG;
	aw_pa->profctrl_desc.mask = AW_PID_2071_RCV_MODE_MASK;
	aw_pa->profctrl_desc.spk_mode = AW_PID_2071_RCV_MODE_SPEAKER_VALUE;

	aw_pa->cco_mux_desc.reg = AW_PID_2071_PLLCTRL1_REG;
	aw_pa->cco_mux_desc.mask = AW_PID_2071_CCO_MUX_MASK;
	aw_pa->cco_mux_desc.divided_val = AW_PID_2071_CCO_MUX_DIVIDED_VALUE;
	aw_pa->cco_mux_desc.bypass_val = AW_PID_2071_CCO_MUX_BYPASS_VALUE;

	aw_pa->voltage_desc.reg = AW_PID_2071_VBAT_REG;
	aw_pa->voltage_desc.int_bit = AW_PID_2071_MONITOR_INT_10BIT;
	aw_pa->voltage_desc.vbat_range = AW_PID_2071_MONITOR_VBAT_RANGE;

	aw_pa->temp_desc.reg = AW_PID_2071_TEMP_REG;
	aw_pa->temp_desc.neg_mask = AW_PID_2071_MONITOR_TEMP_NEG_MASK;
	aw_pa->temp_desc.sign_mask = AW_PID_2071_MONITOR_TEMP_SIGN_MASK;

	aw_pa->ipeak_desc.reg = AW_PID_2071_SYSCTRL2_REG;
	aw_pa->ipeak_desc.mask = AW_PID_2071_BST_IPEAK_MASK;

	aw_pa->volume_desc.reg = AW_PID_2071_SYSCTRL2_REG;
	aw_pa->volume_desc.mask = AW_PID_2071_VOL_MASK;
	aw_pa->volume_desc.shift = AW_PID_2071_VOL_START_BIT;
	aw_pa->volume_desc.mute_volume = AW_PID_2071_MUTE_VOL;
	aw_pa->volume_desc.ctl_volume = AW_PID_2071_VOL_DEFAULT_VALUE;

	aw_pa->bop_desc.reg = AW_REG_NONE;

	aw_pa->soft_rst.reg = AW882XX_SOFT_RESET_REG;
	aw_pa->soft_rst.reg_value = AW882XX_SOFT_RESET_VALUE;

	aw_pa->efcheck_desc.reg = AW_REG_NONE;
	aw_pa->efuse_check = AW_EF_OR_CHECK;

	aw_pa->dither_desc.reg = AW_PID_2071_DBGCTRL_REG;
	aw_pa->dither_desc.mask = AW_PID_2071_DITHER_MASK;
	aw_pa->dither_desc.enable = AW_PID_2071_DITHER_ENABLE_VALUE;
	aw_pa->dither_desc.disable = AW_PID_2071_DITHER_DISABLE_VALUE;

	aw882xx->aw_pa = aw_pa;

	ret = aw882xx_device_probe(aw_pa, aw882xx->init_info);

	return ret;
}

