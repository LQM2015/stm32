/**
 * @file    main_flashloader.c
 * @brief   Flash Loader main函数 - 用于满足链接器要求
 * @author  Assistant
 * @date    2025-09-29
 */

/**
 * @brief  Flash Loader的main函数
 * @note   Flash Loader实际上不使用main函数，但链接器需要这个符号
 * @retval None
 */
int main(void)
{
    // Flash Loader不应该执行到这里
    // 实际的入口点是通过函数指针调用的Init/Write/SectorErase等函数
    while(1)
    {
        // 死循环，防止意外执行
    }
}