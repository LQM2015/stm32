#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* 初始化外部SDRAM并将链接在SDRAM的代码/常量段从FLASH搬运到SDRAM。
 * 完成后会刷新数据/指令缓存，保证从SDRAM取指正确。
 */
void SDRAM_InitAndLoadSections(void);

#ifdef __cplusplus
}
#endif