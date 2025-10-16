/**
 ******************************************************************************
 * @file    bsp_sdram.c
 * @author  Your Name
 * @brief   SDRAM驱动实现
 * @details 提供SDRAM初始化、读写和测试功能
 ******************************************************************************
 */

#include "bsp_sdram.h"
#include "debug.h"
#include "fmc.h"

/* External variables --------------------------------------------------------*/
extern SDRAM_HandleTypeDef hsdram1;

/* Private variables ---------------------------------------------------------*/
static FMC_SDRAM_CommandTypeDef Command;  // 命令结构体

/**
 * @brief  SDRAM 初始化序列
 * @param  hsdram: SDRAM_HandleTypeDef结构体变量指针
 * @retval BSP_SDRAM_StatusTypeDef状态
 */
BSP_SDRAM_StatusTypeDef BSP_SDRAM_Initialization_Sequence(SDRAM_HandleTypeDef *hsdram)
{
    __IO uint32_t tmpmrd = 0;
    HAL_StatusTypeDef hal_status;

    DEBUG_INFO("Starting SDRAM initialization sequence...");

    /* Step 1: 配置时钟使能命令 */
    Command.CommandMode             = FMC_SDRAM_CMD_CLK_ENABLE;    // 开启SDRAM时钟
    Command.CommandTarget           = FMC_COMMAND_TARGET_BANK;     // 选择要控制的区域
    Command.AutoRefreshNumber       = 1;
    Command.ModeRegisterDefinition  = 0;

    hal_status = HAL_SDRAM_SendCommand(hsdram, &Command, SDRAM_TIMEOUT_VALUE);
    if (hal_status != HAL_OK) {
        DEBUG_ERROR("SDRAM CLK_ENABLE command failed!");
        return SDRAM_ERROR;
    }
    
    HAL_Delay(1);  // 延时等待
    DEBUG_INFO("SDRAM clock enabled");

    /* Step 2: 配置预充电命令 */
    Command.CommandMode             = FMC_SDRAM_CMD_PALL;          // 预充电命令
    Command.CommandTarget           = FMC_COMMAND_TARGET_BANK;     // 选择要控制的区域
    Command.AutoRefreshNumber       = 1;
    Command.ModeRegisterDefinition  = 0;

    hal_status = HAL_SDRAM_SendCommand(hsdram, &Command, SDRAM_TIMEOUT_VALUE);
    if (hal_status != HAL_OK) {
        DEBUG_ERROR("SDRAM PALL command failed!");
        return SDRAM_ERROR;
    }
    DEBUG_INFO("SDRAM precharge all completed");

    /* Step 3: 配置自动刷新命令 */
    Command.CommandMode             = FMC_SDRAM_CMD_AUTOREFRESH_MODE;  // 使能自动刷新
    Command.CommandTarget           = FMC_COMMAND_TARGET_BANK;          // 选择要控制的区域
    Command.AutoRefreshNumber       = 8;                                // 自动刷新次数
    Command.ModeRegisterDefinition  = 0;

    hal_status = HAL_SDRAM_SendCommand(hsdram, &Command, SDRAM_TIMEOUT_VALUE);
    if (hal_status != HAL_OK) {
        DEBUG_ERROR("SDRAM auto-refresh command failed!");
        return SDRAM_ERROR;
    }
    DEBUG_INFO("SDRAM auto-refresh completed (8 cycles)");

    /* Step 4: 配置模式寄存器 
     * 性能优化配置:
     * - Burst Length = 1 + Full Page Burst (通过 FMC ReadBurst 控制)
     * - Write Burst = Programmed (启用写入 Burst 模式)
     * - CAS Latency = 2 (匹配 FMC 配置)
     * 
     * 说明: STM32 FMC 已经配置了 ReadBurst Enable,
     *       SDRAM Mode Register 使用 Burst=1 配合 FMC 的 Burst 控制更稳定
     */
    tmpmrd = (uint32_t)SDRAM_MODEREG_BURST_LENGTH_1          |  // Burst=1 (由FMC控制)
                       SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL   |
                       SDRAM_MODEREG_CAS_LATENCY_2           |  // ✅ 修正：匹配FMC的CAS延迟2
                       SDRAM_MODEREG_OPERATING_MODE_STANDARD |
                       SDRAM_MODEREG_WRITEBURST_MODE_PROGRAMMED; // ✅ 启用写入Burst

    Command.CommandMode             = FMC_SDRAM_CMD_LOAD_MODE;     // 加载模式寄存器命令
    Command.CommandTarget           = FMC_COMMAND_TARGET_BANK;     // 选择要控制的区域
    Command.AutoRefreshNumber       = 1;
    Command.ModeRegisterDefinition  = tmpmrd;

    hal_status = HAL_SDRAM_SendCommand(hsdram, &Command, SDRAM_TIMEOUT_VALUE);
    if (hal_status != HAL_OK) {
        DEBUG_ERROR("SDRAM load mode command failed!");
        return SDRAM_ERROR;
    }
    DEBUG_INFO("SDRAM mode register configured (MRD=0x%03X, CAS=%d)", tmpmrd, 2);

    /* Step 5: 设置刷新率 */
    /* 刷新率计算：
     * SDRAM刷新周期 = 64ms / 4096行 = 15.625μs/行
     * FMC_CLK = 200MHz (HCLK/2)
     * 刷新计数 = 15.625μs × 200MHz - 20 = 3125 - 20 = 3105
     * 为了保险起见，使用1543（约7.7μs刷新周期）
     */
    hal_status = HAL_SDRAM_ProgramRefreshRate(hsdram, 1543);
    if (hal_status != HAL_OK) {
        DEBUG_ERROR("SDRAM refresh rate programming failed!");
        return SDRAM_ERROR;
    }
    DEBUG_INFO("SDRAM refresh rate set to 1543 (7.7μs period)");

    DEBUG_INFO("SDRAM initialization sequence completed successfully!");
    return SDRAM_OK;
}

/**
 * @brief  SDRAM 读写测试
 * @param  None
 * @retval BSP_SDRAM_StatusTypeDef状态
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
    
    DEBUG_INFO("========================================");
    DEBUG_INFO("    SDRAM Read/Write Test Started");
    DEBUG_INFO("========================================");
    
    // ========================================
    // 写入测试（32位数据宽度）
    // ========================================
    DEBUG_INFO("Test 1: Writing 32-bit data...");
    pSDRAM = (uint32_t *)SDRAM_BANK_ADDR;
    
    ExecutionTime_Begin = HAL_GetTick();
    
    for (i = 0; i < SDRAM_SIZE/4; i++) {
        *(__IO uint32_t*)pSDRAM++ = i;
    }
    
    ExecutionTime_End  = HAL_GetTick();
    ExecutionTime      = ExecutionTime_End - ExecutionTime_Begin;
    if (ExecutionTime > 0) {
        // HAL_GetTick()返回毫秒
        // 速率(MB/s) = 数据量(MB) * 1000 / 时间(毫秒)
        uint32_t speed_mbps = (SDRAM_SIZE / 1024 / 1024) * 1000 / ExecutionTime;
        DEBUG_INFO("  Speed: %lu MB/s", speed_mbps);
    } else {
        DEBUG_INFO("  Speed: >32000 MB/s (too fast to measure)");
    }
    
    DEBUG_INFO("Write completed:");
    DEBUG_INFO("  Size:  %d MB", SDRAM_SIZE/1024/1024);
    DEBUG_INFO("  Time:  %lu ms", ExecutionTime);
    
    // ========================================
    // 读取测试（32位数据宽度）
    // ========================================
    DEBUG_INFO("Test 2: Reading 32-bit data...");
    pSDRAM = (uint32_t *)SDRAM_BANK_ADDR;
    
    ExecutionTime_Begin = HAL_GetTick();
    
    for (i = 0; i < SDRAM_SIZE/4; i++) {
        ReadData = *(__IO uint32_t*)pSDRAM++;
    }
    
    ExecutionTime_End  = HAL_GetTick();
    ExecutionTime      = ExecutionTime_End - ExecutionTime_Begin;
    if (ExecutionTime > 0) {
        // HAL_GetTick()返回毫秒
        // 速率(MB/s) = 数据量(MB) * 1000 / 时间(毫秒)
        uint32_t speed_mbps = (SDRAM_SIZE / 1024 / 1024) * 1000 / ExecutionTime;
        DEBUG_INFO("  Speed: %lu MB/s", speed_mbps);
    } else {
        DEBUG_INFO("  Speed: >32000 MB/s (too fast to measure)");
    }
    
    DEBUG_INFO("Read completed:");
    DEBUG_INFO("  Size:  %d MB", SDRAM_SIZE/1024/1024);
    DEBUG_INFO("  Time:  %lu ms", ExecutionTime);
    
    // ========================================
    // 数据校验（32位）
    // ========================================
    DEBUG_INFO("Test 3: Verifying 32-bit data...");
    pSDRAM = (uint32_t *)SDRAM_BANK_ADDR;
    
    for (i = 0; i < SDRAM_SIZE/4; i++) {
        ReadData = *(__IO uint32_t*)pSDRAM++;
        if (ReadData != (uint32_t)i) {
            DEBUG_ERROR("SDRAM verification failed!");
            DEBUG_ERROR("  Address: 0x%08X", (uint32_t)(pSDRAM - 1));
            DEBUG_ERROR("  Expected: 0x%08X", i);
            DEBUG_ERROR("  Read:     0x%08X", ReadData);
            return SDRAM_ERROR;
        }
    }
    DEBUG_INFO("32-bit data verification PASSED!");
    
    // ========================================
    // 8位数据读写测试（测试NBL0和NBL1引脚）
    // ========================================
    DEBUG_INFO("Test 4: Writing 8-bit data...");
    for (i = 0; i < SDRAM_SIZE; i++) {
        *(__IO uint8_t*)(SDRAM_BANK_ADDR + i) = (uint8_t)i;
    }
    
    DEBUG_INFO("Test 5: Verifying 8-bit data...");
    for (i = 0; i < SDRAM_SIZE; i++) {
        ReadData_8b = *(__IO uint8_t*)(SDRAM_BANK_ADDR + i);
        if (ReadData_8b != (uint8_t)i) {
            DEBUG_ERROR("8-bit data verification failed!");
            DEBUG_ERROR("  Address: 0x%08X", SDRAM_BANK_ADDR + i);
            DEBUG_ERROR("  Expected: 0x%02X", (uint8_t)i);
            DEBUG_ERROR("  Read:     0x%02X", ReadData_8b);
            DEBUG_ERROR("Please check NBL0 and NBL1 pins!");
            return SDRAM_ERROR;
        }
    }
    DEBUG_INFO("8-bit data verification PASSED!");
    
    // ========================================
    // 测试完成
    // ========================================
    DEBUG_INFO("========================================");
    DEBUG_INFO("   SDRAM Test Completed Successfully!");
    DEBUG_INFO("========================================");
    DEBUG_INFO("SDRAM is fully functional and ready to use.");
    
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

// 内部RAM中执行的高速写入函数 - 简化实现，避免段定义问题
void SDRAM_FastWrite(uint32_t *dest, uint32_t *src, uint32_t size)
{
	// 简单的内存拷贝，编译器会优化
	for (uint32_t i = 0; i < size; i++) {
		dest[i] = src[i];
	}
}

// 内部RAM中执行的高速读取函数  
void SDRAM_FastRead(uint32_t *dest, uint32_t *src, uint32_t size)
{
	// 简单的内存拷贝，编译器会优化
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

	uint32_t ExecutionTime_Begin;		// 开始时间
	uint32_t ExecutionTime_End;		// 结束时间
	uint32_t ExecutionTime;				// 执行时间	
	uint32_t ExecutionSpeed;			// 执行速度
	
	DEBUG_INFO("\r\n*****************************************************************************************************\r\n");		
	DEBUG_INFO("\r\n进行速度测试>>>\r\n");

// 写入 >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	
	pSDRAM =  (uint32_t *)SDRAM_BANK_ADDR;
	
	ExecutionTime_Begin 	= HAL_GetTick();	// 获取 systick 当前时间，单位ms
	
	// ✅ 优化写入：使用更快的写入模式，减少Cache操作
	// 先失效Cache，确保直接写入SDRAM
	SCB_InvalidateDCache_by_Addr((uint32_t *)SDRAM_BANK_ADDR, SDRAM_SIZE);
	__DSB();
	
	// ✅ 详细调试：验证地址映射
	uint32_t *pSDRAM_fast = (uint32_t *)SDRAM_BANK_ADDR;
	DEBUG_INFO("调试：SDRAM地址范围 0x%08lx - 0x%08lx\r\n", 
		SDRAM_BANK_ADDR, SDRAM_BANK_ADDR + SDRAM_SIZE - 1);
	DEBUG_INFO("调试：开始写入，前4个预期值: [0, 1, 2, 3]\r\n");
	
	// ✅ 先验证地址映射是否正确
	pSDRAM_fast[0] = 0x12345678;
	pSDRAM_fast[1] = 0x87654321;
	__DSB();
	DEBUG_INFO("调试：测试写入 [0x12345678, 0x87654321] -> [%08lx, %08lx]\r\n",
		pSDRAM_fast[0], pSDRAM_fast[1]);
	
	// 如果测试失败，可能是硬件问题
	if (pSDRAM_fast[0] != 0x12345678 || pSDRAM_fast[1] != 0x87654321) {
		DEBUG_ERROR("调试：基本写入测试失败，可能存在硬件连接问题！\r\n");
		return;
	}
	
	// ✅ 开始正式写入测试
	for (i = 0; i < SDRAM_SIZE/4; i++)
	{
 		pSDRAM_fast[i] = i;		// 直接数组访问，更快
	}
	
	// ✅ 立即验证前几个写入值
	__DSB();
	DEBUG_INFO("调试：写入完成后前4个实际值: [%ld, %ld, %ld, %ld]\r\n",
		pSDRAM_fast[0], pSDRAM_fast[1], pSDRAM_fast[2], pSDRAM_fast[3]);
	
	// ✅ 分块验证：先验证前1KB数据
	DEBUG_INFO("调试：开始分块验证前1KB数据...\r\n");
	int errors = 0;
	for (i = 0; i < 256; i++) {  // 1KB = 256个32位字
		if (pSDRAM_fast[i] != i) {
			DEBUG_ERROR("分块验证失败：位置%d，期望%d，实际%d\r\n", i, i, pSDRAM_fast[i]);
			errors++;
			if (errors >= 5) break;  // 最多显示5个错误
		}
	}
	if (errors == 0) {
		DEBUG_INFO("分块验证通过：前1KB数据正确\r\n");
	} else {
		DEBUG_ERROR("分块验证失败：发现%d个错误\r\n", errors);
		return;  // 提前退出，不进行全量校验
	}
	
	// ✅ 确保最终数据写入SDRAM（可选，因为读取时会失效Cache）
	// SCB_CleanDCache_by_Addr((uint32_t *)SDRAM_BANK_ADDR, SDRAM_SIZE);
	__DSB();  // 数据同步屏障，确保写入完成
	__ISB();  // 指令同步屏障
	
	ExecutionTime_End		= HAL_GetTick();											// 获取 systick 当前时间，单位ms
	ExecutionTime  = ExecutionTime_End - ExecutionTime_Begin; 				// 计算擦除时间，单位ms
	ExecutionSpeed = SDRAM_SIZE /1024/1024*1000 /ExecutionTime ; 	// 计算速度，单位 MB/S	
	
	DEBUG_INFO("\r\n以32位数据宽度写入数据，大小：%d MB，耗时: %ld ms, 写入速度：%ld MB/s\r\n",SDRAM_SIZE/1024/1024,ExecutionTime,ExecutionSpeed);
	
	// ✅ 调试：验证写入的前几个值（确保Cache一致性）
	SCB_InvalidateDCache_by_Addr((uint32_t *)SDRAM_BANK_ADDR, 32);  // 先失效前32字节Cache
	__DSB();
	pSDRAM = (uint32_t *)SDRAM_BANK_ADDR;
	DEBUG_INFO("调试：前4个写入的值: [%ld, %ld, %ld, %ld]\r\n", 
		pSDRAM[0], pSDRAM[1], pSDRAM[2], pSDRAM[3]);

// ❌ 跳过可能污染测试数据的额外写入测试
	DEBUG_INFO("跳过额外写入测试，直接进行读取验证\r\n");

// 读取	>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> 

	// ✅ 确保Cache一致性：失效D-Cache，确保从SDRAM读取最新数据
	SCB_InvalidateDCache_by_Addr((uint32_t *)SDRAM_BANK_ADDR, SDRAM_SIZE);
	__DSB();  // 确保Cache操作完成
	
	pSDRAM =  (uint32_t *)SDRAM_BANK_ADDR;
		
	ExecutionTime_Begin 	= HAL_GetTick();	// 获取 systick 当前时间，单位ms
	
	for(i = 0; i < SDRAM_SIZE/4;i++ )
	{
		ReadData = *(__IO uint32_t*)pSDRAM++;  // 从SDRAM读出数据	
	}
	ExecutionTime_End		= HAL_GetTick();											// 获取 systick 当前时间，单位ms
	ExecutionTime  = ExecutionTime_End - ExecutionTime_Begin; 				// 计算擦除时间，单位ms
	ExecutionSpeed = SDRAM_SIZE /1024/1024*1000 /ExecutionTime ; 	// 计算速度，单位 MB/S
	
	DEBUG_INFO("\r\n读取数据完毕，大小：%d MB，耗时: %ld ms, 读取速度：%ld MB/s\r\n",SDRAM_SIZE/1024/1024,ExecutionTime,ExecutionSpeed);
	
	// ✅ 调试：验证读取的前几个值（确保Cache一致性）
	SCB_InvalidateDCache_by_Addr((uint32_t *)SDRAM_BANK_ADDR, 32);  // 先失效前32字节Cache
	__DSB();
	pSDRAM = (uint32_t *)SDRAM_BANK_ADDR;
	DEBUG_INFO("调试：读取的前4个值: [%ld, %ld, %ld, %ld]\r\n", 
		pSDRAM[0], pSDRAM[1], pSDRAM[2], pSDRAM[3]);
	
//// 数据校验 >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>   

	DEBUG_INFO("\r\n*****************************************************************************************************\r\n");		
	DEBUG_INFO("\r\n进行数据校验>>>\r\n");
	
	// ✅ 数据校验前确保Cache一致性：失效D-Cache，确保从SDRAM读取最新数据
	SCB_InvalidateDCache_by_Addr((uint32_t *)SDRAM_BANK_ADDR, SDRAM_SIZE);
	__DSB();  // 确保Cache操作完成
	
	pSDRAM =  (uint32_t *)SDRAM_BANK_ADDR;
		
	for(i = 0; i < SDRAM_SIZE/4;i++ )
	{
		ReadData = *(__IO uint32_t*)pSDRAM++;  // 从SDRAM读出数据	
		if( ReadData != (uint32_t)i )      //检测数据，若不相等，跳出函数,返回检测失败结果。
		{
			DEBUG_ERROR("\r\nSDRAM测试失败！！出错位置：%d,读出数据：%ld (0x%08lx)\r\n ",i,ReadData,ReadData);
			// ✅ 额外调试：显示附近几个位置的数据
			if (i > 0) {
				DEBUG_ERROR("附近数据: [%d]=0x%08lx, [%d]=0x%08lx, [%d]=0x%08lx\r\n",
					i-1, *(__IO uint32_t*)(SDRAM_BANK_ADDR + (i-1)*4),
					i, ReadData,
					i+1, *(__IO uint32_t*)(SDRAM_BANK_ADDR + (i+1)*4));
			}
			return;	 // 返回失败标志
		}
	}


	DEBUG_INFO("\r\n32位数据宽度读写通过，以8位数据宽度写入数据\r\n");
	for (i = 0; i < SDRAM_SIZE; i++)
	{
 		*(__IO uint8_t*) (SDRAM_BANK_ADDR + i) =  (uint8_t)i;
	}	
	DEBUG_INFO("写入完毕，读取数据并比较...\r\n");
	for (i = 0; i < SDRAM_SIZE; i++)
	{
		ReadData_8b = *(__IO uint8_t*) (SDRAM_BANK_ADDR + i);
		if( ReadData_8b != (uint8_t)i )      //检测数据，若不相等，跳出函数,返回检测失败结果。
		{
			DEBUG_ERROR("8位数据宽度读写测试失败！！");
			DEBUG_ERROR("请检查NBL0和NBL1的连接");
			return;	 // 返回失败标志
		}
	}	
	DEBUG_INFO("8位数据宽度读写通过");
	DEBUG_INFO("SDRAM读写测试通过，系统正常");

	return;	 // 返回成功标志
}

/**
 * @brief  写入数据到SDRAM
 * @param  address: 写入地址（相对于SDRAM起始地址的偏移）
 * @param  pData: 数据指针
 * @param  size: 数据大小（字节）
 * @retval BSP_SDRAM_StatusTypeDef状态
 */
BSP_SDRAM_StatusTypeDef BSP_SDRAM_WriteData(uint32_t address, uint8_t *pData, uint32_t size)
{
    uint32_t i;
    uint8_t *pSDRAM;
    
    if (address + size > SDRAM_SIZE) {
        DEBUG_ERROR("SDRAM write address out of range!");
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
 * @retval BSP_SDRAM_StatusTypeDef状态
 */
BSP_SDRAM_StatusTypeDef BSP_SDRAM_ReadData(uint32_t address, uint8_t *pData, uint32_t size)
{
    uint32_t i;
    uint8_t *pSDRAM;
    
    if (address + size > SDRAM_SIZE) {
        DEBUG_ERROR("SDRAM read address out of range!");
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
 * @retval BSP_SDRAM_StatusTypeDef状态
 */
BSP_SDRAM_StatusTypeDef BSP_SDRAM_WriteData_DMA(uint32_t address, uint8_t *pData, uint32_t size)
{
    if (address + size > SDRAM_SIZE) {
        DEBUG_ERROR("SDRAM DMA write address out of range!");
        return SDRAM_ERROR;
    }
    
    // 检查地址对齐 - MDMA要求32字节对齐以获得最佳性能
    if (((uint32_t)pData & 0x1F) != 0 || ((SDRAM_BANK_ADDR + address) & 0x1F) != 0) {
        DEBUG_ERROR("DMA write: 地址未对齐到32字节边界");
        return SDRAM_ERROR;
    }
    
    // 检查大小对齐 - 必须是4字节的倍数
    if ((size & 0x03) != 0) {
        DEBUG_ERROR("DMA write: 大小必须是4字节的倍数");
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
            DEBUG_ERROR("MDMA write initialization failed!");
            return SDRAM_ERROR;
        }
        mdma_initialized = 1;
    }
    
    // 开始DMA传输
    if (HAL_MDMA_Start(&hmdma_sdram_write, (uint32_t)pData, SDRAM_BANK_ADDR + address, size, 1) != HAL_OK) {
        DEBUG_ERROR("MDMA write transfer start failed!");
        return SDRAM_ERROR;
    }
    
    // 等待传输完成
    if (HAL_MDMA_PollForTransfer(&hmdma_sdram_write, HAL_MDMA_FULL_TRANSFER, SDRAM_TIMEOUT_VALUE) != HAL_OK) {
        DEBUG_ERROR("MDMA write transfer timeout!");
        return SDRAM_TIMEOUT;
    }
    
    return SDRAM_OK;
}

/**
 * @brief  使用DMA从SDRAM读取数据（高速传输）
 * @param  address: 读取地址（相对于SDRAM起始地址的偏移）
 * @param  pData: 数据指针
 * @param  size: 数据大小（字节）
 * @retval BSP_SDRAM_StatusTypeDef状态
 */
BSP_SDRAM_StatusTypeDef BSP_SDRAM_ReadData_DMA(uint32_t address, uint8_t *pData, uint32_t size)
{
    if (address + size > SDRAM_SIZE) {
        DEBUG_ERROR("SDRAM DMA read address out of range!");
        return SDRAM_ERROR;
    }
    
    // 检查地址对齐 - MDMA要求32字节对齐以获得最佳性能
    if (((uint32_t)pData & 0x1F) != 0 || ((SDRAM_BANK_ADDR + address) & 0x1F) != 0) {
        DEBUG_ERROR("DMA read: 地址未对齐到32字节边界");
        return SDRAM_ERROR;
    }
    
    // 检查大小对齐 - 必须是4字节的倍数
    if ((size & 0x03) != 0) {
        DEBUG_ERROR("DMA read: 大小必须是4字节的倍数");
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
            DEBUG_ERROR("MDMA read initialization failed!");
            return SDRAM_ERROR;
        }
        read_mdma_initialized = 1;
    }
    
    // 开始DMA传输
    if (HAL_MDMA_Start(&hmdma_sdram_read, SDRAM_BANK_ADDR + address, (uint32_t)pData, size, 1) != HAL_OK) {
        DEBUG_ERROR("MDMA read transfer start failed!");
        return SDRAM_ERROR;
    }
    
    // 等待传输完成
    if (HAL_MDMA_PollForTransfer(&hmdma_sdram_read, HAL_MDMA_FULL_TRANSFER, SDRAM_TIMEOUT_VALUE) != HAL_OK) {
        DEBUG_ERROR("MDMA read transfer timeout!");
        return SDRAM_TIMEOUT;
    }
    
    return SDRAM_OK;
}
