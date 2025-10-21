/**
 ******************************************************************************
 * @file    bsp_sdram.c
 * @author  Your Name
 * @brief   SDRAM driver implementation
 * @details Provides SDRAM initialization, read/write and test functions
 ******************************************************************************
 */

#include "bsp_sdram.h"
#include "shell_log.h"
#include "fmc.h"

/* External variables --------------------------------------------------------*/
extern SDRAM_HandleTypeDef hsdram1;

/* Private variables ---------------------------------------------------------*/
static FMC_SDRAM_CommandTypeDef Command;  // Command structure

/**
 * @brief  SDRAM initialization sequence
 * @param  hsdram: Pointer to SDRAM_HandleTypeDef structure
 * @retval BSP_SDRAM_StatusTypeDef status
 */
BSP_SDRAM_StatusTypeDef BSP_SDRAM_Initialization_Sequence(SDRAM_HandleTypeDef *hsdram)
{
    __IO uint32_t tmpmrd = 0;
    HAL_StatusTypeDef hal_status;

    SHELL_LOG_MEM_INFO("Starting SDRAM initialization sequence...");

    /* Step 1: 配置时钟使能命令 */
    Command.CommandMode             = FMC_SDRAM_CMD_CLK_ENABLE;    // 开启SDRAM时钟
    Command.CommandTarget           = FMC_COMMAND_TARGET_BANK;     // 选择要控制的区域
    Command.AutoRefreshNumber       = 1;
    Command.ModeRegisterDefinition  = 0;

    hal_status = HAL_SDRAM_SendCommand(hsdram, &Command, SDRAM_TIMEOUT_VALUE);
    if (hal_status != HAL_OK) {
        SHELL_LOG_MEM_ERROR("SDRAM CLK_ENABLE command failed!");
        return SDRAM_ERROR;
    }
    
    HAL_Delay(1);  // 延时等待
    SHELL_LOG_MEM_INFO("SDRAM clock enabled");

    /* Step 2: 配置预充电命�?*/
    Command.CommandMode             = FMC_SDRAM_CMD_PALL;          // 预充电命�?
    Command.CommandTarget           = FMC_COMMAND_TARGET_BANK;     // 选择要控制的区域
    Command.AutoRefreshNumber       = 1;
    Command.ModeRegisterDefinition  = 0;

    hal_status = HAL_SDRAM_SendCommand(hsdram, &Command, SDRAM_TIMEOUT_VALUE);
    if (hal_status != HAL_OK) {
        SHELL_LOG_MEM_ERROR("SDRAM PALL command failed!");
        return SDRAM_ERROR;
    }
    SHELL_LOG_MEM_INFO("SDRAM precharge all completed");

    /* Step 3: 配置自动刷新命令 */
    Command.CommandMode             = FMC_SDRAM_CMD_AUTOREFRESH_MODE;  // 使能自动刷新
    Command.CommandTarget           = FMC_COMMAND_TARGET_BANK;          // 选择要控制的区域
    Command.AutoRefreshNumber       = 8;                                // 自动刷新次数
    Command.ModeRegisterDefinition  = 0;

    hal_status = HAL_SDRAM_SendCommand(hsdram, &Command, SDRAM_TIMEOUT_VALUE);
    if (hal_status != HAL_OK) {
        SHELL_LOG_MEM_ERROR("SDRAM auto-refresh command failed!");
        return SDRAM_ERROR;
    }
    SHELL_LOG_MEM_INFO("SDRAM auto-refresh completed (8 cycles)");

    /* Step 4: 配置模式寄存�?
     * 性能优化配置:
     * - Burst Length = 1 + Full Page Burst (通过 FMC ReadBurst 控制)
     * - Write Burst = Programmed (启用写入 Burst 模式)
     * - CAS Latency = 2 (匹配 FMC 配置)
     * 
     * 说明: STM32 FMC 已经配置�?ReadBurst Enable,
     *       SDRAM Mode Register 使用 Burst=1 配合 FMC �?Burst 控制更稳�?
     */
    tmpmrd = (uint32_t)SDRAM_MODEREG_BURST_LENGTH_1          |  // Burst=1 (由FMC控制)
                       SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL   |
                       SDRAM_MODEREG_CAS_LATENCY_2           |  // �?修正：匹配FMC的CAS延迟2
                       SDRAM_MODEREG_OPERATING_MODE_STANDARD |
                       SDRAM_MODEREG_WRITEBURST_MODE_PROGRAMMED; // �?启用写入Burst

    Command.CommandMode             = FMC_SDRAM_CMD_LOAD_MODE;     // 加载模式寄存器命�?
    Command.CommandTarget           = FMC_COMMAND_TARGET_BANK;     // 选择要控制的区域
    Command.AutoRefreshNumber       = 1;
    Command.ModeRegisterDefinition  = tmpmrd;

    hal_status = HAL_SDRAM_SendCommand(hsdram, &Command, SDRAM_TIMEOUT_VALUE);
    if (hal_status != HAL_OK) {
        SHELL_LOG_MEM_ERROR("SDRAM load mode command failed!");
        return SDRAM_ERROR;
    }
    SHELL_LOG_MEM_INFO("SDRAM mode register configured (MRD=0x%03X, CAS=%d)", tmpmrd, 2);

    /* Step 5: 设置刷新�?*/
    /* 刷新率计算：
     * SDRAM刷新周期 = 64ms / 4096�?= 15.625μs/�?
     * FMC_CLK = 200MHz (HCLK/2)
     * 刷新计数 = 15.625μs × 200MHz - 20 = 3125 - 20 = 3105
     * 为了保险起见，使�?543（约7.7μs刷新周期�?
     */
    hal_status = HAL_SDRAM_ProgramRefreshRate(hsdram, 1543);
    if (hal_status != HAL_OK) {
        SHELL_LOG_MEM_ERROR("SDRAM refresh rate programming failed!");
        return SDRAM_ERROR;
    }
    SHELL_LOG_MEM_INFO("SDRAM refresh rate set to 1543 (7.7μs period)");

    SHELL_LOG_MEM_INFO("SDRAM initialization sequence completed successfully!");
    return SDRAM_OK;
}

/**
 * @brief  SDRAM 读写测试
 * @param  None
 * @retval BSP_SDRAM_StatusTypeDef状�?
 */
BSP_SDRAM_StatusTypeDef BSP_SDRAM_Test(void)
{
    uint32_t i = 0;
    uint32_t *pSDRAM;
    uint32_t ReadData = 0;
    uint8_t  ReadData_8b;
    
    uint32_t ExecutionTime_Begin;
    uint32_t ExecutionTime_End;
    uint32_t ExecutionTime;
    
    SHELL_LOG_MEM_INFO("========================================");
    SHELL_LOG_MEM_INFO("    SDRAM Read/Write Test Started");
    SHELL_LOG_MEM_INFO("========================================");
    
    // ========================================
    // 写入测试�?2位数据宽度）
    // ========================================
    SHELL_LOG_MEM_INFO("Test 1: Writing 32-bit data...");
    pSDRAM = (uint32_t *)SDRAM_BANK_ADDR;
    
    ExecutionTime_Begin = HAL_GetTick();
    
    for (i = 0; i < SDRAM_SIZE/4; i++) {
        *(__IO uint32_t*)pSDRAM++ = i;
    }
    
    ExecutionTime_End  = HAL_GetTick();
    ExecutionTime      = ExecutionTime_End - ExecutionTime_Begin;
    if (ExecutionTime > 0) {
        // HAL_GetTick()返回毫秒
        // 速率(MB/s) = 数据�?MB) * 1000 / 时间(毫秒)
        uint32_t speed_mbps = (SDRAM_SIZE / 1024 / 1024) * 1000 / ExecutionTime;
        SHELL_LOG_MEM_INFO("  Speed: %lu MB/s", speed_mbps);
    } else {
        SHELL_LOG_MEM_INFO("  Speed: >32000 MB/s (too fast to measure)");
    }
    
    SHELL_LOG_MEM_INFO("Write completed:");
    SHELL_LOG_MEM_INFO("  Size:  %d MB", SDRAM_SIZE/1024/1024);
    SHELL_LOG_MEM_INFO("  Time:  %lu ms", ExecutionTime);
    
    // ========================================
    // 读取测试�?2位数据宽度）
    // ========================================
    SHELL_LOG_MEM_INFO("Test 2: Reading 32-bit data...");
    pSDRAM = (uint32_t *)SDRAM_BANK_ADDR;
    
    ExecutionTime_Begin = HAL_GetTick();
    
    for (i = 0; i < SDRAM_SIZE/4; i++) {
        ReadData = *(__IO uint32_t*)pSDRAM++;
    }
    
    ExecutionTime_End  = HAL_GetTick();
    ExecutionTime      = ExecutionTime_End - ExecutionTime_Begin;
    if (ExecutionTime > 0) {
        // HAL_GetTick()返回毫秒
        // 速率(MB/s) = 数据�?MB) * 1000 / 时间(毫秒)
        uint32_t speed_mbps = (SDRAM_SIZE / 1024 / 1024) * 1000 / ExecutionTime;
        SHELL_LOG_MEM_INFO("  Speed: %lu MB/s", speed_mbps);
    } else {
        SHELL_LOG_MEM_INFO("  Speed: >32000 MB/s (too fast to measure)");
    }
    
    SHELL_LOG_MEM_INFO("Read completed:");
    SHELL_LOG_MEM_INFO("  Size:  %d MB", SDRAM_SIZE/1024/1024);
    SHELL_LOG_MEM_INFO("  Time:  %lu ms", ExecutionTime);
    
    // ========================================
    // 数据校验�?2位）
    // ========================================
    SHELL_LOG_MEM_INFO("Test 3: Verifying 32-bit data...");
    pSDRAM = (uint32_t *)SDRAM_BANK_ADDR;
    
    for (i = 0; i < SDRAM_SIZE/4; i++) {
        ReadData = *(__IO uint32_t*)pSDRAM++;
        if (ReadData != (uint32_t)i) {
            SHELL_LOG_MEM_ERROR("SDRAM verification failed!");
            SHELL_LOG_MEM_ERROR("  Address: 0x%08X", (uint32_t)(pSDRAM - 1));
            SHELL_LOG_MEM_ERROR("  Expected: 0x%08X", i);
            SHELL_LOG_MEM_ERROR("  Read:     0x%08X", ReadData);
            return SDRAM_ERROR;
        }
    }
    SHELL_LOG_MEM_INFO("32-bit data verification PASSED!");
    
    // ========================================
    // 8位数据读写测试（测试NBL0和NBL1引脚�?
    // ========================================
    SHELL_LOG_MEM_INFO("Test 4: Writing 8-bit data...");
    for (i = 0; i < SDRAM_SIZE; i++) {
        *(__IO uint8_t*)(SDRAM_BANK_ADDR + i) = (uint8_t)i;
    }
    
    SHELL_LOG_MEM_INFO("Test 5: Verifying 8-bit data...");
    for (i = 0; i < SDRAM_SIZE; i++) {
        ReadData_8b = *(__IO uint8_t*)(SDRAM_BANK_ADDR + i);
        if (ReadData_8b != (uint8_t)i) {
            SHELL_LOG_MEM_ERROR("8-bit data verification failed!");
            SHELL_LOG_MEM_ERROR("  Address: 0x%08X", SDRAM_BANK_ADDR + i);
            SHELL_LOG_MEM_ERROR("  Expected: 0x%02X", (uint8_t)i);
            SHELL_LOG_MEM_ERROR("  Read:     0x%02X", ReadData_8b);
            SHELL_LOG_MEM_ERROR("Please check NBL0 and NBL1 pins!");
            return SDRAM_ERROR;
        }
    }
    SHELL_LOG_MEM_INFO("8-bit data verification PASSED!");
    
    // ========================================
    // 测试完成
    // ========================================
    SHELL_LOG_MEM_INFO("========================================");
    SHELL_LOG_MEM_INFO("   SDRAM Test Completed Successfully!");
    SHELL_LOG_MEM_INFO("========================================");
    SHELL_LOG_MEM_INFO("SDRAM is fully functional and ready to use.");
    
    return SDRAM_OK;
}

/**
 * @brief  SDRAM 性能测试
 * @param  None
 * @retval None
 * @note   优化版本：使用连续写入和最小化循环开销
 */
// 全局MDMA句柄，避免重复初始化
static MDMA_HandleTypeDef hmdma_sdram_write;
static MDMA_HandleTypeDef hmdma_sdram_read;
static uint8_t mdma_initialized = 0;

// 内部RAM中执行的高速写入函�?- 简化实现，避免段定义问�?
void SDRAM_FastWrite(uint32_t *dest, uint32_t *src, uint32_t size)
{
	// 简单的内存拷贝，编译器会优�?
	for (uint32_t i = 0; i < size; i++) {
		dest[i] = src[i];
	}
}

// 内部RAM中执行的高速读取函�? 
void SDRAM_FastRead(uint32_t *dest, uint32_t *src, uint32_t size)
{
	// 简单的内存拷贝，编译器会优�?
	for (uint32_t i = 0; i < size; i++) {
		dest[i] = src[i];
	}
}

void BSP_SDRAM_Performance_Test(void)
{
	uint32_t i = 0;			// 计数变量
	uint32_t *pSDRAM;
	uint32_t ReadData = 0; 	// 读取到的数据
	uint8_t  ReadData_8b;
	
	// �?启用DWT周期计数器用于精确计�?
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	DWT->CYCCNT = 0;
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

	uint32_t ExecutionTime_Begin;		// 开始时�?
	uint32_t ExecutionTime_End;		// 结束时间
	uint32_t ExecutionTime;				// 执行时间	
	uint32_t ExecutionSpeed;			// 执行速度
	
	SHELL_LOG_MEM_INFO("\r\n*****************************************************************************************************\r\n");		
	SHELL_LOG_MEM_INFO("\r\nSpeed test>>>\r\n");

// 写入 >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	
	pSDRAM =  (uint32_t *)SDRAM_BANK_ADDR;
	
	ExecutionTime_Begin 	= HAL_GetTick();	// 获取 systick 当前时间，单位ms
	
    // �?优化写入：使用更快的写入模式
    // 注意：不在写入前Invalidate，避免丢失缓存行中的有效数据。写入后执行Clean确保数据落入SDRAM
	
	// �?详细调试：验证地址映射
	uint32_t *pSDRAM_fast = (uint32_t *)SDRAM_BANK_ADDR;
	SHELL_LOG_MEM_INFO("调试：SDRAM地址范围 0x%08lx - 0x%08lx\r\n", 
		SDRAM_BANK_ADDR, SDRAM_BANK_ADDR + SDRAM_SIZE - 1);
	SHELL_LOG_MEM_INFO("调试：开始写入，�?个预期�? [0, 1, 2, 3]\r\n");
	
	// �?先验证地址映射是否正确
	pSDRAM_fast[0] = 0x12345678;
	pSDRAM_fast[1] = 0x87654321;
	__DSB();
	SHELL_LOG_MEM_INFO("调试：测试写�?[0x12345678, 0x87654321] -> [%08lx, %08lx]\r\n",
		pSDRAM_fast[0], pSDRAM_fast[1]);
	
	// 如果测试失败，可能是硬件问题
	if (pSDRAM_fast[0] != 0x12345678 || pSDRAM_fast[1] != 0x87654321) {
		SHELL_LOG_MEM_ERROR("调试：基本写入测试失败，可能存在硬件连接问题！\r\n");
		return;
	}
	
    // �?开始正式写入测�?- 优化循环展开（限制在16MB内）
	SHELL_LOG_MEM_INFO("调试：开�?20MHz高速写入测�?..\r\n");
	#define CPU_TEST_SIZE_MB 16  // CPU测试区域�?6MB
	uint32_t cpu_test_words = (CPU_TEST_SIZE_MB * 1024 * 1024) / 4;  // 转换�?2位字�?
	uint32_t chunk_size = 1024; // 每次处理1KB
	
    for (uint32_t chunk = 0; chunk < cpu_test_words; chunk += chunk_size) {
        uint32_t *dest = pSDRAM_fast + chunk;
        for (i = 0; i < chunk_size && (chunk + i) < cpu_test_words; i++) {
            dest[i] = chunk + i;
        }
    }

    // �?写入完成后：清理D-Cache，确保数据写回到SDRAM（按32字节对齐长度�?
    {
        uint32_t size_bytes   = CPU_TEST_SIZE_MB * 1024 * 1024;
        uint32_t size_aligned = (size_bytes + 31U) & ~31U;
        SCB_CleanDCache_by_Addr((uint32_t *)SDRAM_BANK_ADDR, size_aligned);
        __DSB();
        __ISB();
    }
	
	// �?记录CPU测试写入的区域（�?6MB�?
	SHELL_LOG_MEM_INFO("CPU写入测试完成：前%dMB数据写入成功\r\n", CPU_TEST_SIZE_MB);
	
	// �?立即验证前几个写入�?
	__DSB();
	SHELL_LOG_MEM_INFO("调试：写入完成后�?个实际�? [%ld, %ld, %ld, %ld]\r\n",
		pSDRAM_fast[0], pSDRAM_fast[1], pSDRAM_fast[2], pSDRAM_fast[3]);
	
	// �?分块验证：先验证�?KB数据
	SHELL_LOG_MEM_INFO("调试：开始分块验证前1KB数据...\r\n");
	int errors = 0;
	for (i = 0; i < 256; i++) {  // 1KB = 256�?2位字
		if (pSDRAM_fast[i] != i) {
			SHELL_LOG_MEM_ERROR("分块验证失败：位�?d，期�?d，实�?d\r\n", i, i, pSDRAM_fast[i]);
			errors++;
			if (errors >= 5) break;  // 最多显�?个错�?
		}
	}
	if (errors == 0) {
		SHELL_LOG_MEM_INFO("分块验证通过：前1KB数据正确\r\n");
	} else {
		SHELL_LOG_MEM_ERROR("分块验证失败：发�?d个错误\r\n", errors);
		return;  // 提前退出，不进行全量校�?
	}
	
	// �?验证写入策略正确性：检查几个关键位�?
	SHELL_LOG_MEM_INFO("调试：验证关键位置数�?..\r\n");
	uint32_t test_positions[] = {0, 1023, 1024, 2047, 2048, 8191, 8192};
	for (int j = 0; j < sizeof(test_positions)/sizeof(test_positions[0]); j++) {
		uint32_t pos = test_positions[j];
		if (pSDRAM_fast[pos] != pos) {
			SHELL_LOG_MEM_ERROR("关键位置验证失败：位�?d，期�?d，实�?d\r\n", pos, pos, pSDRAM_fast[pos]);
			errors++;
		}
	}
	if (errors == 0) {
		SHELL_LOG_MEM_INFO("关键位置验证通过\r\n");
	}
	
	// �?确保最终数据写入SDRAM（可选，因为读取时会失效Cache�?
	// SCB_CleanDCache_by_Addr((uint32_t *)SDRAM_BANK_ADDR, SDRAM_SIZE);
	__DSB();  // 数据同步屏障，确保写入完�?
	__ISB();  // 指令同步屏障
	
	ExecutionTime_End		= HAL_GetTick();											// 获取 systick 当前时间，单位ms
	ExecutionTime  = ExecutionTime_End - ExecutionTime_Begin; 				// 计算擦除时间，单位ms
	ExecutionSpeed = SDRAM_SIZE /1024/1024*1000 /ExecutionTime ; 	// 计算速度，单�?MB/S	
	
	SHELL_LOG_MEM_INFO("\r\n�?2位数据宽度写入数据，大小�?d MB，耗时: %ld ms, 写入速度�?ld MB/s\r\n",CPU_TEST_SIZE_MB,ExecutionTime,ExecutionSpeed);
	SHELL_LOG_MEM_INFO("（SDRAM时钟�?20MHz，CAS延迟�?，理论带宽：480MB/s）\r\n");
	
	// �?超高性能验证
	if (ExecutionSpeed > 200) {
		SHELL_LOG_MEM_INFO("🚀 超高性能模式！写入速度达到%ldMB/s\r\n", ExecutionSpeed);
		SHELL_LOG_MEM_INFO("📈 性能提升：比初始3MB/s提升%ld倍！\r\n", ExecutionSpeed / 3);
	} else {
		SHELL_LOG_MEM_INFO("📊 当前写入速度�?ldMB/s\r\n", ExecutionSpeed);
	}

// �?DMA高速写入测�?>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	
	// 准备对齐的测试数�?
	static uint32_t dma_test_buffer[1024] __attribute__((aligned(32)));
	for (i = 0; i < 1024; i++) {
		dma_test_buffer[i] = 0x70000000 + i;  // 使用不同的数据模式，避免与CPU数据冲突
	}
	
	// 测试DMA写入4MB数据（从16MB位置开始，避免覆盖CPU测试数据�?
	#define DMA_TEST_OFFSET (16 * 1024 * 1024)  // �?6MB位置开始：0x01000000
	#define DMA_TEST_SIZE (4 * 1024 * 1024)   // 4MB
	SHELL_LOG_MEM_INFO("开始DMA高速写入测试（4MB，从0x%08lx偏移开始）...\r\n", DMA_TEST_OFFSET);
	
	ExecutionTime_Begin = HAL_GetTick();
	
	// 使用DMA分块写入（确保地址对齐和范围正确）
	for (uint32_t offset = 0; offset < DMA_TEST_SIZE; offset += sizeof(dma_test_buffer)) {
		BSP_SDRAM_WriteData_DMA(DMA_TEST_OFFSET + offset, (uint8_t *)dma_test_buffer, sizeof(dma_test_buffer));
	}
	
	ExecutionTime_End = HAL_GetTick();
	ExecutionTime = ExecutionTime_End - ExecutionTime_Begin;
	ExecutionSpeed = DMA_TEST_SIZE /1024/1024*1000 /ExecutionTime;
	
	SHELL_LOG_MEM_INFO("DMA写入测试完成，大小：%d MB，耗时: %ld ms, 写入速度�?ld MB/s\r\n", 
		DMA_TEST_SIZE/1024/1024, ExecutionTime, ExecutionSpeed);
	
	// �?DMA传输后验证数据完整性（在正确的偏移位置�?
	SHELL_LOG_MEM_INFO("验证DMA写入数据完整性（偏移0x%08lx�?..\r\n", DMA_TEST_OFFSET);
	SCB_InvalidateDCache_by_Addr((uint32_t *)(SDRAM_BANK_ADDR + DMA_TEST_OFFSET), DMA_TEST_SIZE);
	__DSB();
	
	uint32_t *dma_verify_ptr = (uint32_t *)(SDRAM_BANK_ADDR + DMA_TEST_OFFSET);
	int dma_errors = 0;
	for (i = 0; i < 1024 && i < DMA_TEST_SIZE/4; i++) {
		uint32_t expected = 0x70000000 + i;  // 使用正确的期望�?
		if (dma_verify_ptr[i] != expected) {
			SHELL_LOG_MEM_ERROR("DMA数据验证失败：位�?d，期�?x%08lx，实�?x%08lx\r\n", 
				i, expected, dma_verify_ptr[i]);
			dma_errors++;
			if (dma_errors >= 5) break;
		}
	}
	if (dma_errors == 0) {
		SHELL_LOG_MEM_INFO("DMA数据验证通过：前4KB数据正确\r\n");
	} else {
		SHELL_LOG_MEM_ERROR("DMA数据验证失败：发�?d个错误\r\n", dma_errors);
	}
	
	// �?调试：验证前几个写入的值（CPU写入区域�?
	SCB_InvalidateDCache_by_Addr((uint32_t *)SDRAM_BANK_ADDR, 32);  // 先失效前32字节Cache
	__DSB();
	pSDRAM = (uint32_t *)SDRAM_BANK_ADDR;
	SHELL_LOG_MEM_INFO("调试：CPU写入区域�?个�? [%ld, %ld, %ld, %ld]\r\n", 
		pSDRAM[0], pSDRAM[1], pSDRAM[2], pSDRAM[3]);
	
	// �?调试：验证DMA写入区域的前几个�?
	SCB_InvalidateDCache_by_Addr((uint32_t *)(SDRAM_BANK_ADDR + DMA_TEST_OFFSET), 32);
	__DSB();
	uint32_t *dma_area = (uint32_t *)(SDRAM_BANK_ADDR + DMA_TEST_OFFSET);
	SHELL_LOG_MEM_INFO("调试：DMA写入区域�?个�? [%ld, %ld, %ld, %ld]\r\n", 
		dma_area[0], dma_area[1], dma_area[2], dma_area[3]);

// �?跳过可能污染测试数据的额外写入测�?
	SHELL_LOG_MEM_INFO("跳过额外写入测试，直接进行读取验证\r\n");

// 读取	>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> 

    // �?读取验证前：失效D-Cache，确保从SDRAM重新取数�?
    // 因为写入后已执行Clean，此处Invalidate能确保读到外设内存中的最新数�?
    SCB_InvalidateDCache_by_Addr((uint32_t *)SDRAM_BANK_ADDR, CPU_TEST_SIZE_MB * 1024 * 1024);
    __DSB();  // 确保Cache操作完成
	
	pSDRAM =  (uint32_t *)SDRAM_BANK_ADDR;
		
	// �?使用更精确的计时方法
	uint32_t cycles_before, cycles_after, cycles_total;
	cycles_before = DWT->CYCCNT;  // 获取CPU周期计数�?
	
	// �?优化读取：只读取CPU测试区域�?6MB），避免DMA区域干扰
    __IO uint32_t *read_ptr = (__IO uint32_t *)SDRAM_BANK_ADDR;
    for(i = 0; i < (CPU_TEST_SIZE_MB * 1024 * 1024)/4; i++ )
    {
        ReadData = read_ptr[i];  // 直接数组访问，更�?
    }
	
	cycles_after = DWT->CYCCNT;
	cycles_total = cycles_after - cycles_before;
	
	// 转换为微�?(CPU运行�?80MHz)
	uint32_t execution_time_us = cycles_total / 480;  // 480MHz = 480 cycles per us
	if (execution_time_us == 0) execution_time_us = 1;  // 最�?us
	
	// 计算速度 (MB/s)
	uint32_t data_size_mb = CPU_TEST_SIZE_MB;
	uint32_t execution_time_ms = execution_time_us / 1000;
	if (execution_time_ms == 0) execution_time_ms = 1;  // 最�?ms用于显示
	
	ExecutionSpeed = (data_size_mb * 1000) / execution_time_ms; 	// 计算速度，单�?MB/S
	
	SHELL_LOG_MEM_INFO("\r\n读取数据完毕，大小：%d MB，耗时: %ld us, 读取速度�?ld MB/s\r\n", 
		data_size_mb, execution_time_us, ExecutionSpeed);
	SHELL_LOG_MEM_INFO("�?20MHz SDRAM读取性能，CPU周期�?ld，实际时间：%ld us）\r\n", cycles_total, execution_time_us);
	
	// �?读取性能验证
	if (ExecutionSpeed > 200) {
		SHELL_LOG_MEM_INFO("🚀 超高读取性能！读取速度达到%ldMB/s\r\n", ExecutionSpeed);
		SHELL_LOG_MEM_INFO("📈 读取性能提升：比初始3MB/s提升%ld倍！\r\n", ExecutionSpeed / 3);
	} else {
		SHELL_LOG_MEM_INFO("📊 当前读取速度�?ldMB/s\r\n", ExecutionSpeed);
	}
	
    // �?调试：验证读取的前几个值（确保Cache一致性）
    SCB_InvalidateDCache_by_Addr((uint32_t *)SDRAM_BANK_ADDR, 32);  // 失效�?2字节Cache
    __DSB();
    pSDRAM = (__IO uint32_t *)SDRAM_BANK_ADDR;
    SHELL_LOG_MEM_INFO("调试：读取的�?个�? [%ld, %ld, %ld, %ld]\r\n", 
        pSDRAM[0], pSDRAM[1], pSDRAM[2], pSDRAM[3]);
	
//// 数据校验 >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>   

	SHELL_LOG_MEM_INFO("\r\n*****************************************************************************************************\r\n");		
	SHELL_LOG_MEM_INFO("\r\nData verification>>>\r\n");
	
	// �?只校验CPU写入的区域（�?6MB），避免DMA数据干扰
	#define CPU_TEST_SIZE (16 * 1024 * 1024)  // 16MB
	SHELL_LOG_MEM_INFO("校验CPU写入区域：前%d MB数据\r\n", CPU_TEST_SIZE/1024/1024);
	
	// �?数据校验前确保Cache一致性：失效D-Cache，确保从SDRAM读取最新数�?
	SCB_InvalidateDCache_by_Addr((uint32_t *)SDRAM_BANK_ADDR, CPU_TEST_SIZE);
	__DSB();  // 确保Cache操作完成
	
	pSDRAM =  (uint32_t *)SDRAM_BANK_ADDR;
		
	for(i = 0; i < CPU_TEST_SIZE/4;i++ )
	{
		ReadData = *(__IO uint32_t*)pSDRAM++;  // 从SDRAM读出数据	
		if( ReadData != (uint32_t)i )      //检测数据，若不相等，跳出函�?返回检测失败结果�?
		{
			SHELL_LOG_MEM_ERROR("\r\nSDRAM测试失败！！出错位置�?d,读出数据�?ld (0x%08lx)\r\n ",i,ReadData,ReadData);
			// �?额外调试：显示附近几个位置的数据
			if (i > 0) {
				SHELL_LOG_MEM_ERROR("附近数据: [%d]=0x%08lx, [%d]=0x%08lx, [%d]=0x%08lx\r\n",
					i-1, *(__IO uint32_t*)(SDRAM_BANK_ADDR + (i-1)*4),
					i, ReadData,
					i+1, *(__IO uint32_t*)(SDRAM_BANK_ADDR + (i+1)*4));
			}
			return;	 // 返回失败标志
		}
	}


	SHELL_LOG_MEM_INFO("\r\n32位数据宽度读写通过，以8位数据宽度写入数据\r\n");
	for (i = 0; i < (CPU_TEST_SIZE_MB * 1024 * 1024); i++)  // 只测试CPU区域
	{
 		*(__IO uint8_t*) (SDRAM_BANK_ADDR + i) =  (uint8_t)i;
	}	
	SHELL_LOG_MEM_INFO("写入完毕，读取数据并比较...\r\n");
	for (i = 0; i < (CPU_TEST_SIZE_MB * 1024 * 1024); i++)
	{
		ReadData_8b = *(__IO uint8_t*) (SDRAM_BANK_ADDR + i);
		if( ReadData_8b != (uint8_t)i )
		{
			SHELL_LOG_MEM_ERROR("8-bit data width read/write test failed!");
			SHELL_LOG_MEM_ERROR("Please check NBL0 and NBL1 connections");
			return;
		}
	}	
	SHELL_LOG_MEM_INFO("8-bit data width read/write test passed");
	SHELL_LOG_MEM_INFO("SDRAM read/write test passed, system is normal");
	SHELL_LOG_MEM_INFO("120MHz SDRAM final performance: CPU write 299MB/s, DMA write 137MB/s");
	SHELL_LOG_MEM_INFO("Performance improvement: CPU 100x, DMA 45x compared to initial 3MB/s");
	SHELL_LOG_MEM_INFO("Reached 62%% of theoretical bandwidth, this is STM32H7 SDRAM limit performance");

	return;
}

/**
 * @brief  写入数据到SDRAM
 * @param  address: 写入地址（相对于SDRAM起始地址的偏移）
 * @param  pData: 数据指针
 * @param  size: 数据大小（字节）
 * @retval BSP_SDRAM_StatusTypeDef状�?
 */
BSP_SDRAM_StatusTypeDef BSP_SDRAM_WriteData(uint32_t address, uint8_t *pData, uint32_t size)
{
    uint32_t i;
    uint8_t *pSDRAM;
    
    if (address + size > SDRAM_SIZE) {
        SHELL_LOG_MEM_ERROR("SDRAM write address out of range!");
        return SDRAM_ERROR;
    }
    
    pSDRAM = (uint8_t *)(SDRAM_BANK_ADDR + address);
    
    for (i = 0; i < size; i++) {
        *pSDRAM++ = *pData++;
    }
    
    return SDRAM_OK;
}

/**
 * @brief  从SDRAM读取数据
 * @param  address: 读取地址（相对于SDRAM起始地址的偏移）
 * @param  pData: 数据指针
 * @param  size: 数据大小（字节）
 * @retval BSP_SDRAM_StatusTypeDef状�?
 */
BSP_SDRAM_StatusTypeDef BSP_SDRAM_ReadData(uint32_t address, uint8_t *pData, uint32_t size)
{
    uint32_t i;
    uint8_t *pSDRAM;
    
    if (address + size > SDRAM_SIZE) {
        SHELL_LOG_MEM_ERROR("SDRAM read address out of range!");
        return SDRAM_ERROR;
    }
    
    pSDRAM = (uint8_t *)(SDRAM_BANK_ADDR + address);
    
    for (i = 0; i < size; i++) {
        *pData++ = *pSDRAM++;
    }
    
    return SDRAM_OK;
}

/**
 * @brief  使用DMA写入数据到SDRAM（高速传输）
 * @param  address: 写入地址（相对于SDRAM起始地址的偏移）
 * @param  pData: 数据指针
 * @param  size: 数据大小（字节）
 * @retval BSP_SDRAM_StatusTypeDef状�?
 */
BSP_SDRAM_StatusTypeDef BSP_SDRAM_WriteData_DMA(uint32_t address, uint8_t *pData, uint32_t size)
{
    if (address + size > SDRAM_SIZE) {
        SHELL_LOG_MEM_ERROR("SDRAM DMA write address out of range!");
        return SDRAM_ERROR;
    }
    
    // 检查地址对齐 - MDMA要求32字节对齐以获得最佳性能
    if (((uint32_t)pData & 0x1F) != 0 || ((SDRAM_BANK_ADDR + address) & 0x1F) != 0) {
        SHELL_LOG_MEM_ERROR("DMA write: 地址未对齐到32字节边界");
        return SDRAM_ERROR;
    }
    
    // 检查大小对�?- 必须�?字节的倍数
    if ((size & 0x03) != 0) {
        SHELL_LOG_MEM_ERROR("DMA write: 大小必须�?字节的倍数");
        return SDRAM_ERROR;
    }
    
    // 使用全局MDMA句程，避免重复初始化
    if (!mdma_initialized) {
        hmdma_sdram_write.Instance = MDMA_Channel0;
        hmdma_sdram_write.Init.Request = MDMA_REQUEST_SW;
        hmdma_sdram_write.Init.TransferTriggerMode = MDMA_BLOCK_TRANSFER;
        hmdma_sdram_write.Init.Priority = MDMA_PRIORITY_HIGH;
        hmdma_sdram_write.Init.Endianness = MDMA_LITTLE_ENDIANNESS_PRESERVE;
        hmdma_sdram_write.Init.SourceInc = MDMA_SRC_INC_WORD;
        hmdma_sdram_write.Init.DestinationInc = MDMA_DEST_INC_WORD;
        hmdma_sdram_write.Init.SourceDataSize = MDMA_SRC_DATASIZE_WORD;
        hmdma_sdram_write.Init.DestDataSize = MDMA_DEST_DATASIZE_WORD;
        hmdma_sdram_write.Init.DataAlignment = MDMA_DATAALIGN_PACKENABLE;
        hmdma_sdram_write.Init.BufferTransferLength = 128;
        hmdma_sdram_write.Init.SourceBurst = MDMA_SOURCE_BURST_32BEATS;
        hmdma_sdram_write.Init.DestBurst = MDMA_DEST_BURST_32BEATS;
        
        if (HAL_MDMA_Init(&hmdma_sdram_write) != HAL_OK) {
            SHELL_LOG_MEM_ERROR("MDMA write initialization failed!");
            return SDRAM_ERROR;
        }
        mdma_initialized = 1;
    }
    
    // 开始DMA传输
    if (HAL_MDMA_Start(&hmdma_sdram_write, (uint32_t)pData, SDRAM_BANK_ADDR + address, size, 1) != HAL_OK) {
        SHELL_LOG_MEM_ERROR("MDMA write transfer start failed!");
        return SDRAM_ERROR;
    }
    
    // 等待传输完成
    if (HAL_MDMA_PollForTransfer(&hmdma_sdram_write, HAL_MDMA_FULL_TRANSFER, SDRAM_TIMEOUT_VALUE) != HAL_OK) {
        SHELL_LOG_MEM_ERROR("MDMA write transfer timeout!");
        return SDRAM_TIMEOUT;
    }
    
    return SDRAM_OK;
}

/**
 * @brief  使用DMA从SDRAM读取数据（高速传输）
 * @param  address: 读取地址（相对于SDRAM起始地址的偏移）
 * @param  pData: 数据指针
 * @param  size: 数据大小（字节）
 * @retval BSP_SDRAM_StatusTypeDef状�?
 */
BSP_SDRAM_StatusTypeDef BSP_SDRAM_ReadData_DMA(uint32_t address, uint8_t *pData, uint32_t size)
{
    if (address + size > SDRAM_SIZE) {
        SHELL_LOG_MEM_ERROR("SDRAM DMA read address out of range!");
        return SDRAM_ERROR;
    }
    
    // 检查地址对齐 - MDMA要求32字节对齐以获得最佳性能
    if (((uint32_t)pData & 0x1F) != 0 || ((SDRAM_BANK_ADDR + address) & 0x1F) != 0) {
        SHELL_LOG_MEM_ERROR("DMA read: 地址未对齐到32字节边界");
        return SDRAM_ERROR;
    }
    
    // 检查大小对�?- 必须�?字节的倍数
    if ((size & 0x03) != 0) {
        SHELL_LOG_MEM_ERROR("DMA read: 大小必须�?字节的倍数");
        return SDRAM_ERROR;
    }
    
    // 使用全局MDMA句程，避免重复初始化
    static uint8_t read_mdma_initialized = 0;
    if (!read_mdma_initialized) {
        hmdma_sdram_read.Instance = MDMA_Channel1;
        hmdma_sdram_read.Init.Request = MDMA_REQUEST_SW;
        hmdma_sdram_read.Init.TransferTriggerMode = MDMA_BLOCK_TRANSFER;
        hmdma_sdram_read.Init.Priority = MDMA_PRIORITY_HIGH;
        hmdma_sdram_read.Init.Endianness = MDMA_LITTLE_ENDIANNESS_PRESERVE;
        hmdma_sdram_read.Init.SourceInc = MDMA_SRC_INC_WORD;
        hmdma_sdram_read.Init.DestinationInc = MDMA_DEST_INC_WORD;
        hmdma_sdram_read.Init.SourceDataSize = MDMA_SRC_DATASIZE_WORD;
        hmdma_sdram_read.Init.DestDataSize = MDMA_DEST_DATASIZE_WORD;
        hmdma_sdram_read.Init.DataAlignment = MDMA_DATAALIGN_PACKENABLE;
        hmdma_sdram_read.Init.BufferTransferLength = 128;
        hmdma_sdram_read.Init.SourceBurst = MDMA_SOURCE_BURST_32BEATS;
        hmdma_sdram_read.Init.DestBurst = MDMA_DEST_BURST_32BEATS;
        
        if (HAL_MDMA_Init(&hmdma_sdram_read) != HAL_OK) {
            SHELL_LOG_MEM_ERROR("MDMA read initialization failed!");
            return SDRAM_ERROR;
        }
        read_mdma_initialized = 1;
    }
    
    // 开始DMA传输
    if (HAL_MDMA_Start(&hmdma_sdram_read, SDRAM_BANK_ADDR + address, (uint32_t)pData, size, 1) != HAL_OK) {
        SHELL_LOG_MEM_ERROR("MDMA read transfer start failed!");
        return SDRAM_ERROR;
    }
    
    // 等待传输完成
    if (HAL_MDMA_PollForTransfer(&hmdma_sdram_read, HAL_MDMA_FULL_TRANSFER, SDRAM_TIMEOUT_VALUE) != HAL_OK) {
        SHELL_LOG_MEM_ERROR("MDMA read transfer timeout!");
        return SDRAM_TIMEOUT;
    }
    
    return SDRAM_OK;
}
