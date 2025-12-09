#include <stdio.h>
#include <stdlib.h>

#include "aw882xx.h"
#include "aw882xx_pid_1852_reg.h"
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


/* [7 : 4]: -6DB ; [3 : 0]: 0.5DB  real_value = value * 2 : 0.5db --> 1 */
static unsigned int aw_pid_1852_reg_val_to_db(unsigned int value)
{
	return ((value >> 4) * AW_PID_1852_VOL_STEP_DB + (value & 0x0f));
}

/* [7 : 4]: -6DB ; [3 : 0]: -0.5DB reg_value = value / step << 4 + value % step ; step = 6 * 2 */
static unsigned int aw_pid_1852_db_val_to_reg(unsigned int value)
{
	return (((value / AW_PID_1852_VOL_STEP_DB) << 4) + (value % AW_PID_1852_VOL_STEP_DB));
}

static int aw_pid_1852_set_volume(struct aw_device *aw_dev, unsigned int value)
{
	struct aw882xx *aw882xx = (struct aw882xx *)aw_dev->private_data;
	unsigned int reg_value = 0;
	unsigned int real_value = aw_pid_1852_db_val_to_reg(value);

	/* cal real value */
	aw882xx_i2c_read(aw882xx, AW_PID_1852_HAGCCFG4_REG, &reg_value);

	aw_dev_dbg(aw882xx->dev_index, "value %d , 0x%x", value, real_value);

	/* 15 : 8] volume */
	real_value = (real_value << 8) | (reg_value & 0x00ff);

	/* write value */
	aw882xx_i2c_write(aw882xx, AW_PID_1852_HAGCCFG4_REG, real_value);
	return 0;
}

static int aw_pid_1852_get_volume(struct aw_device *aw_dev, unsigned int *value)
{
	unsigned int reg_value = 0;
	unsigned int real_value = 0;
	struct aw882xx *aw882xx = (struct aw882xx *)aw_dev->private_data;

	/* read value */
	aw882xx_i2c_read(aw882xx, AW_PID_1852_HAGCCFG4_REG, &reg_value);

	/* [15 : 8] volume */
	real_value = reg_value >> 8;

	real_value = aw_pid_1852_reg_val_to_db(real_value);
	*value = real_value;

	return 0;
}

static void aw_pid_1852_i2s_enable(struct aw_device *aw_dev, bool flag)
{
	struct aw882xx *aw882xx = (struct aw882xx *)aw_dev->private_data;

	aw_dev_dbg(aw882xx->dev_index, "enter");

	if (flag) {
		aw882xx_i2c_write_bits(aw882xx, AW_PID_1852_I2SCFG1_REG,
				AW_PID_1852_I2STXEN_MASK,
				AW_PID_1852_I2STXEN_ENABLE_VALUE);
	} else {
		aw882xx_i2c_write_bits(aw882xx, AW_PID_1852_I2SCFG1_REG,
				AW_PID_1852_I2STXEN_MASK,
				AW_PID_1852_I2STXEN_DISABLE_VALUE);
	}
}

static bool aw_pid_1852_check_rd_access(int reg)
{
	if (reg >= AW_PID_1852_REG_MAX) {
		return false;
	}
	if (aw_pid_1852_reg_access[reg] & AW_PID_1852_REG_RD_ACCESS) {
		return true;
	} else {
		return false;
	}
}

static bool aw_pid_1852_check_wr_access(int reg)
{
	if (reg >= AW_PID_1852_REG_MAX) {
		return false;
	}
	if (aw_pid_1852_reg_access[reg] & AW_PID_1852_REG_WR_ACCESS) {
		return true;
	} else {
		return false;
	}
}

static int aw_pid_1852_get_reg_num(void)
{
	return AW_PID_1852_REG_MAX;
}

static unsigned int aw_pid_1852_get_irq_type(struct aw_device *aw_dev,
				unsigned int value)
{
	unsigned int ret = INT_TYPE_NONE;
	/* UVL0 */
	if (value & (~AW_PID_1852_UVLI_MASK)) {
		aw_dev_info(aw_dev->dev_index, "UVLO: occur");
		ret |= INT_TYPE_UVLO;
	}

	/* BSTOCM */
	if (value & (~AW_PID_1852_BSTOCI_MASK)) {
		aw_dev_info(aw_dev->dev_index, "BSTOCI: occur");
		ret |= INT_TYPE_BSTOC;
	}

	/* OCDI */
	if (value & (~AW_PID_1852_OCDI_MASK)) {
		aw_dev_info(aw_dev->dev_index, "OCDI: occur");
		ret |= INT_TYPE_OCDI;
	}

	/* OTHI */
	if (value & (~AW_PID_1852_OTHI_MASK)) {
		aw_dev_info(aw_dev->dev_index, "OTHI: occur");
		ret |= INT_TYPE_OTHI;
	}

	return ret;
}

int aw882xx_pid_1852_dev_init(void *aw882xx_val)
{
	int ret = -1;
	struct aw_device *aw_pa = NULL;
	struct aw882xx *aw882xx = (struct aw882xx *)aw882xx_val;

	aw_pr_dbg("enter");

	if (aw882xx == NULL) {
		aw_pr_err("aw882xx is null");
		return -EINVAL;
	}

	if (aw882xx->chip_id != PID_1852_ID) {
		aw_dev_dbg(aw882xx->dev_index, "The id:0x%x does not match:0x%x",
					aw882xx->chip_id, PID_1852_ID);
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
	aw_pa->vol_step = AW_PID_1852_VOL_STEP;
	aw_pa->chip_id = aw882xx->chip_id;
	aw_pa->dev_index = aw882xx->dev_index;
	aw_pa->private_data = (void *)aw882xx;
	aw_pa->bstcfg_enable = AW_BSTCFG_DISABLE;
	aw_pa->ops.aw_get_version = aw882xx_get_version;
	aw_pa->ops.aw_get_dev_num = aw882xx_get_dev_num;
	aw_pa->ops.aw_i2c_read = aw882xx_dev_i2c_read;
	aw_pa->ops.aw_i2c_write = aw882xx_dev_i2c_write;
	aw_pa->ops.aw_i2c_write_bits = aw882xx_dev_i2c_write_bits;
	aw_pa->ops.aw_get_volume = aw_pid_1852_get_volume;
	aw_pa->ops.aw_set_volume = aw_pid_1852_set_volume;
	aw_pa->ops.aw_reg_val_to_db = aw_pid_1852_reg_val_to_db;
	aw_pa->ops.aw_i2s_enable = aw_pid_1852_i2s_enable;
	aw_pa->ops.aw_check_rd_access = aw_pid_1852_check_rd_access;
	aw_pa->ops.aw_check_wr_access = aw_pid_1852_check_wr_access;
	aw_pa->ops.aw_get_reg_num = aw_pid_1852_get_reg_num;
	aw_pa->ops.aw_get_irq_type = aw_pid_1852_get_irq_type;

	aw_pa->int_desc.mask_reg = AW_PID_1852_SYSINTM_REG;
	aw_pa->int_desc.mask_default = AW_PID_1852_SYSINTM_DEFAULT;
	aw_pa->int_desc.int_mask = AW_PID_1852_SYSINTM_DEFAULT;
	aw_pa->int_desc.st_reg = AW_PID_1852_SYSINT_REG;
	aw_pa->int_desc.uvl_val = AW_PID_1852_UVLS_VDD_BELOW_2P8V_VALUE;
	aw_pa->int_desc.clock_val = AW_PID_1852_NOCLKS_TRIG_VALUE;
	aw_pa->int_desc.clks_val = AW_PID_1852_CLKS_TRIG_VALUE;

	aw_pa->pwd_desc.reg = AW_PID_1852_SYSCTRL_REG;
	aw_pa->pwd_desc.mask = AW_PID_1852_PWDN_MASK;
	aw_pa->pwd_desc.enable = AW_PID_1852_PWDN_POWER_DOWN_VALUE;
	aw_pa->pwd_desc.disable = AW_PID_1852_PWDN_NORMAL_WORKING_VALUE;

	aw_pa->amppd_desc.reg = AW_PID_1852_SYSCTRL_REG;
	aw_pa->amppd_desc.mask = AW_PID_1852_AMPPD_MASK;
	aw_pa->amppd_desc.enable = AW_PID_1852_AMPPD_POWER_DOWN_VALUE;
	aw_pa->amppd_desc.disable = AW_PID_1852_AMPPD_NORMAL_WORKING_VALUE;

	aw_pa->mute_desc.reg = AW_PID_1852_SYSCTRL2_REG;
	aw_pa->mute_desc.mask = AW_PID_1852_HMUTE_MASK;
	aw_pa->mute_desc.enable = AW_PID_1852_HMUTE_ENABLE_VALUE;
	aw_pa->mute_desc.disable = AW_PID_1852_HMUTE_DISABLE_VALUE;

	aw_pa->uls_hmute_desc.reg = AW_REG_NONE;

	aw_pa->vcalb_desc.vcalb_reg = AW_PID_1852_VTMCTRL3_REG;
	aw_pa->vcalb_desc.vcal_factor = AW_PID_1852_VCAL_FACTOR;
	aw_pa->vcalb_desc.cabl_base_value = AW_PID_1852_CABL_BASE_VALUE;

	aw_pa->vcalb_desc.icalk_value_factor = AW_PID_1852_ICABLK_FACTOR;
	aw_pa->vcalb_desc.icalk_reg = AW_PID_1852_EFRM1_REG;
	aw_pa->vcalb_desc.icalkl_reg = AW_REG_NONE;
	aw_pa->vcalb_desc.icalk_reg_mask = AW_PID_1852_EF_ISN_GESLP_MASK;
	aw_pa->vcalb_desc.icalk_sign_mask = AW_PID_1852_EF_ISN_GESLP_SIGN_MASK;
	aw_pa->vcalb_desc.icalk_neg_mask = AW_PID_1852_EF_ISN_GESLP_NEG;

	aw_pa->vcalb_desc.vcalk_reg = AW_PID_1852_EFRH_REG;
	aw_pa->vcalb_desc.vcalk_reg_mask = AW_PID_1852_EF_VSN_GESLP_MASK;
	aw_pa->vcalb_desc.vcalkl_reg = AW_REG_NONE;
	aw_pa->vcalb_desc.vcalk_sign_mask = AW_PID_1852_EF_VSN_GESLP_SIGN_MASK;
	aw_pa->vcalb_desc.vcalk_neg_mask = AW_PID_1852_EF_VSN_GESLP_NEG;
	aw_pa->vcalb_desc.vcalk_value_factor = AW_PID_1852_VCABLK_FACTOR;

	aw_pa->sysst_desc.reg = AW_PID_1852_SYSST_REG;
	aw_pa->sysst_desc.mask = AW_PID_1852_SYSST_CHECK_MASK;
	aw_pa->sysst_desc.st_check = AW_PID_1852_SYSST_CHECK;
	aw_pa->sysst_desc.pll_check = AW_PID_1852_PLLS_LOCKED_VALUE;

	aw_pa->profctrl_desc.reg = AW_PID_1852_SYSCTRL_REG;
	aw_pa->profctrl_desc.mask = AW_PID_1852_RCV_MODE_MASK;
	aw_pa->profctrl_desc.spk_mode = AW_PID_1852_RCV_MODE_SPEAKER_MODE;

	aw_pa->cco_mux_desc.reg = AW_PID_1852_PLLCTRL1_REG;
	aw_pa->cco_mux_desc.mask = AW_PID_1852_I2S_CCO_MUX_MASK;
	aw_pa->cco_mux_desc.divided_val = AW_PID_1852_I2S_CCO_MUX_8_16_32KHZ_VALUE;
	aw_pa->cco_mux_desc.bypass_val = AW_PID_1852_I2S_CCO_MUX_EXC_8_16_32KHZ_VALUE;

	aw_pa->voltage_desc.reg = AW_PID_1852_VBAT_REG;
	aw_pa->voltage_desc.int_bit = AW_PID_1852_MONITOR_INT_10BIT;
	aw_pa->voltage_desc.vbat_range = AW_PID_1852_MONITOR_VBAT_RANGE;

	aw_pa->temp_desc.reg = AW_PID_1852_TEMP_REG;
	aw_pa->temp_desc.neg_mask = AW_PID_1852_MONITOR_TEMP_NEG_MASK;
	aw_pa->temp_desc.sign_mask = AW_PID_1852_MONITOR_TEMP_SIGN_MASK;

	aw_pa->ipeak_desc.reg = AW_PID_1852_SYSCTRL2_REG;
	aw_pa->ipeak_desc.mask = AW_PID_1852_BST_IPEAK_MASK;

	aw_pa->volume_desc.reg = AW_PID_1852_HAGCCFG4_REG;
	aw_pa->volume_desc.mask = AW_PID_1852_VOL_MASK;
	aw_pa->volume_desc.shift = AW_PID_1852_VOL_START_BIT;
	aw_pa->volume_desc.mute_volume = AW_PID_1852_MUTE_VOL;
	aw_pa->volume_desc.ctl_volume = AW_PID_1852_VOL_DEFAULT_VALUE;

	aw_pa->bop_desc.reg = AW_REG_NONE;

	aw_pa->soft_rst.reg = AW882XX_SOFT_RESET_REG;
	aw_pa->soft_rst.reg_value = AW882XX_SOFT_RESET_VALUE;

	aw_pa->efcheck_desc.reg = AW_REG_NONE;

	aw_pa->dither_desc.reg = AW_PID_1852_TESTCTRL2_REG;
	aw_pa->dither_desc.mask = AW_PID_1852_DITHER_MASK;
	aw_pa->dither_desc.enable = AW_PID_1852_DITHER_ENABLE_VALUE;
	aw_pa->dither_desc.disable = AW_PID_1852_DITHER_DISABLE_VALUE;

	aw882xx->aw_pa = aw_pa;

	ret = aw882xx_device_probe(aw_pa, aw882xx->init_info);

	return ret;
}

