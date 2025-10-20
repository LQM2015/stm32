/* FatFs 功能测试示例
 * 
 * 本文件提供了各种FatFs功能的测试代码示例
 * 使用前请确保SD卡已正确初始化
 */

#include "ff.h"
#include <stdio.h>
#include <string.h>

/* 测试结果输出宏 */
#define TEST_PASS(name) printf("[PASS] %s\n", name)
#define TEST_FAIL(name, res) printf("[FAIL] %s - Error: %d\n", name, res)

/* 全局变量 */
static FATFS fs;
static FIL fil;
static DIR dir;
static FILINFO fno;

/*---------------------------------------------------------------------------*/
/* 测试1：基本文件读写                                                        */
/*---------------------------------------------------------------------------*/
void test_basic_file_operations(void)
{
    FRESULT res;
    UINT bw, br;
    const char *write_data = "Hello, FatFs!\n";
    char read_buffer[32];
    
    printf("\n=== 测试1: 基本文件读写 ===\n");
    
    /* 写入文件 */
    res = f_open(&fil, "SD:/test.txt", FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK) {
        TEST_FAIL("打开文件写入", res);
        return;
    }
    
    res = f_write(&fil, write_data, strlen(write_data), &bw);
    if (res != FR_OK || bw != strlen(write_data)) {
        TEST_FAIL("写入数据", res);
        f_close(&fil);
        return;
    }
    
    f_close(&fil);
    TEST_PASS("文件写入");
    
    /* 读取文件 */
    res = f_open(&fil, "SD:/test.txt", FA_READ);
    if (res != FR_OK) {
        TEST_FAIL("打开文件读取", res);
        return;
    }
    
    res = f_read(&fil, read_buffer, sizeof(read_buffer) - 1, &br);
    if (res != FR_OK) {
        TEST_FAIL("读取数据", res);
        f_close(&fil);
        return;
    }
    
    read_buffer[br] = '\0';
    f_close(&fil);
    
    if (strcmp(read_buffer, write_data) == 0) {
        TEST_PASS("文件读取");
        printf("  读取内容: %s", read_buffer);
    } else {
        TEST_FAIL("数据校验", -1);
    }
}

/*---------------------------------------------------------------------------*/
/* 测试2：长文件名和UTF-8编码                                                 */
/*---------------------------------------------------------------------------*/
void test_long_filename_utf8(void)
{
    FRESULT res;
    const char *chinese_filename = "SD:/测试文件_中文名称.txt";
    const char *long_filename = "SD:/this_is_a_very_long_filename_with_more_than_8_characters.txt";
    
    printf("\n=== 测试2: 长文件名和UTF-8 ===\n");
    
    /* 测试中文文件名 */
    res = f_open(&fil, chinese_filename, FA_CREATE_ALWAYS | FA_WRITE);
    if (res == FR_OK) {
        f_puts("中文内容测试\n", &fil);
        f_close(&fil);
        TEST_PASS("中文文件名创建");
        
        /* 验证读取 */
        res = f_stat(chinese_filename, &fno);
        if (res == FR_OK) {
            TEST_PASS("中文文件名访问");
        } else {
            TEST_FAIL("中文文件名访问", res);
        }
    } else {
        TEST_FAIL("中文文件名创建", res);
    }
    
    /* 测试长文件名 */
    res = f_open(&fil, long_filename, FA_CREATE_ALWAYS | FA_WRITE);
    if (res == FR_OK) {
        f_close(&fil);
        TEST_PASS("长文件名创建");
        f_unlink(long_filename);  // 清理
    } else {
        TEST_FAIL("长文件名创建", res);
    }
}

/*---------------------------------------------------------------------------*/
/* 测试3：格式化printf输出                                                   */
/*---------------------------------------------------------------------------*/
void test_formatted_output(void)
{
    FRESULT res;
    int int_val = 12345;
    long long ll_val = 1234567890123LL;
    float float_val = 3.14159f;
    
    printf("\n=== 测试3: 格式化输出 ===\n");
    
    res = f_open(&fil, "SD:/printf_test.txt", FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK) {
        TEST_FAIL("打开文件", res);
        return;
    }
    
    /* 测试各种格式化输出 */
    f_printf(&fil, "整数: %d\n", int_val);
    f_printf(&fil, "长整数: %lld\n", ll_val);
    f_printf(&fil, "十六进制: 0x%X\n", int_val);
    f_printf(&fil, "浮点数: %.3f\n", float_val);
    f_printf(&fil, "字符串: %s\n", "Hello World");
    
    f_close(&fil);
    TEST_PASS("格式化输出");
}

/*---------------------------------------------------------------------------*/
/* 测试4：目录操作                                                           */
/*---------------------------------------------------------------------------*/
void test_directory_operations(void)
{
    FRESULT res;
    char cwd[64];
    
    printf("\n=== 测试4: 目录操作 ===\n");
    
    /* 创建目录 */
    res = f_mkdir("SD:/testdir");
    if (res == FR_OK || res == FR_EXIST) {
        TEST_PASS("创建目录");
    } else {
        TEST_FAIL("创建目录", res);
        return;
    }
    
    /* 改变目录 */
    res = f_chdir("SD:/testdir");
    if (res == FR_OK) {
        TEST_PASS("改变目录");
    } else {
        TEST_FAIL("改变目录", res);
        return;
    }
    
    /* 获取当前目录 */
    res = f_getcwd(cwd, sizeof(cwd));
    if (res == FR_OK) {
        TEST_PASS("获取当前目录");
        printf("  当前目录: %s\n", cwd);
    } else {
        TEST_FAIL("获取当前目录", res);
    }
    
    /* 回到根目录 */
    f_chdir("SD:/");
    
    /* 在子目录创建文件 */
    res = f_open(&fil, "SD:/testdir/subfile.txt", FA_CREATE_ALWAYS | FA_WRITE);
    if (res == FR_OK) {
        f_puts("子目录文件\n", &fil);
        f_close(&fil);
        TEST_PASS("子目录文件创建");
    } else {
        TEST_FAIL("子目录文件创建", res);
    }
}

/*---------------------------------------------------------------------------*/
/* 测试5：目录搜索                                                           */
/*---------------------------------------------------------------------------*/
void test_directory_search(void)
{
    FRESULT res;
    int file_count = 0;
    
    printf("\n=== 测试5: 目录搜索 ===\n");
    
    /* 搜索所有.txt文件 */
    res = f_findfirst(&dir, &fno, "SD:", "*.txt");
    if (res == FR_OK) {
        printf("  搜索结果:\n");
        while (res == FR_OK && fno.fname[0]) {
            printf("    %s (%lu bytes)\n", fno.fname, (unsigned long)fno.fsize);
            file_count++;
            res = f_findnext(&dir, &fno);
        }
        f_closedir(&dir);
        TEST_PASS("目录搜索");
        printf("  找到 %d 个文件\n", file_count);
    } else {
        TEST_FAIL("目录搜索", res);
    }
}

/*---------------------------------------------------------------------------*/
/* 测试6：文件属性和时间戳                                                    */
/*---------------------------------------------------------------------------*/
void test_file_attributes(void)
{
    FRESULT res;
    
    printf("\n=== 测试6: 文件属性 ===\n");
    
    /* 创建测试文件 */
    res = f_open(&fil, "SD:/attr_test.txt", FA_CREATE_ALWAYS | FA_WRITE);
    if (res == FR_OK) {
        f_puts("属性测试\n", &fil);
        f_close(&fil);
    } else {
        TEST_FAIL("创建文件", res);
        return;
    }
    
    /* 读取文件信息 */
    res = f_stat("SD:/attr_test.txt", &fno);
    if (res == FR_OK) {
        printf("  文件名: %s\n", fno.fname);
        printf("  大小: %lu bytes\n", (unsigned long)fno.fsize);
        printf("  属性: 0x%02X\n", fno.fattrib);
        printf("  日期: %04d-%02d-%02d\n", 
               1980 + (fno.fdate >> 9), 
               (fno.fdate >> 5) & 0x0F,
               fno.fdate & 0x1F);
        printf("  时间: %02d:%02d:%02d\n",
               fno.ftime >> 11,
               (fno.ftime >> 5) & 0x3F,
               (fno.ftime & 0x1F) * 2);
        TEST_PASS("读取文件信息");
    } else {
        TEST_FAIL("读取文件信息", res);
        return;
    }
    
    /* 修改文件属性为只读 */
    res = f_chmod("SD:/attr_test.txt", AM_RDO, AM_RDO);
    if (res == FR_OK) {
        TEST_PASS("设置只读属性");
        
        /* 取消只读 */
        f_chmod("SD:/attr_test.txt", 0, AM_RDO);
    } else {
        TEST_FAIL("设置只读属性", res);
    }
}

/*---------------------------------------------------------------------------*/
/* 测试7：磁盘空间查询                                                       */
/*---------------------------------------------------------------------------*/
void test_disk_space(void)
{
    FRESULT res;
    FATFS *fs_ptr;
    DWORD fre_clust, fre_sect, tot_sect;
    char label[12];
    DWORD vsn;
    
    printf("\n=== 测试7: 磁盘信息 ===\n");
    
    /* 获取空闲空间 */
    res = f_getfree("SD:", &fre_clust, &fs_ptr);
    if (res == FR_OK) {
        tot_sect = (fs_ptr->n_fatent - 2) * fs_ptr->csize;
        fre_sect = fre_clust * fs_ptr->csize;
        
        printf("  文件系统类型: FAT%d\n", fs_ptr->fs_type);
        printf("  总空间: %lu KB (%lu MB)\n", 
               tot_sect / 2, tot_sect / 2048);
        printf("  可用空间: %lu KB (%lu MB)\n", 
               fre_sect / 2, fre_sect / 2048);
        printf("  已用: %.1f%%\n", 
               100.0 * (tot_sect - fre_sect) / tot_sect);
        TEST_PASS("获取磁盘空间");
    } else {
        TEST_FAIL("获取磁盘空间", res);
    }
    
    /* 获取卷标 */
    res = f_getlabel("SD:", label, &vsn);
    if (res == FR_OK) {
        printf("  卷标: %s\n", label[0] ? label : "(无)");
        printf("  序列号: %08lX\n", vsn);
        TEST_PASS("获取卷标");
    } else {
        TEST_FAIL("获取卷标", res);
    }
}

/*---------------------------------------------------------------------------*/
/* 测试8：文件扩展和快速定位                                                  */
/*---------------------------------------------------------------------------*/
void test_expand_and_seek(void)
{
    FRESULT res;
    DWORD file_size = 1024 * 100;  // 100KB
    FSIZE_t pos;
    
    printf("\n=== 测试8: 文件扩展和定位 ===\n");
    
    /* 创建并扩展文件 */
    res = f_open(&fil, "SD:/expand_test.dat", FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK) {
        TEST_FAIL("创建文件", res);
        return;
    }
    
    /* 预分配空间 */
    res = f_expand(&fil, file_size, 1);  // 预分配100KB
    if (res == FR_OK) {
        TEST_PASS("文件空间预分配");
    } else {
        TEST_FAIL("文件空间预分配", res);
    }
    
    /* 测试定位 */
    res = f_lseek(&fil, 50000);  // 定位到50KB位置
    if (res == FR_OK) {
        pos = f_tell(&fil);
        if (pos == 50000) {
            TEST_PASS("文件定位");
            printf("  当前位置: %lu\n", (unsigned long)pos);
        } else {
            TEST_FAIL("文件定位验证", -1);
        }
    } else {
        TEST_FAIL("文件定位", res);
    }
    
    f_close(&fil);
    
    /* 清理 */
    f_unlink("SD:/expand_test.dat");
}

/*---------------------------------------------------------------------------*/
/* 测试9：字符串I/O函数                                                      */
/*---------------------------------------------------------------------------*/
void test_string_io(void)
{
    FRESULT res;
    char line_buffer[128];
    
    printf("\n=== 测试9: 字符串I/O ===\n");
    
    /* 写入多行 */
    res = f_open(&fil, "SD:/string_io.txt", FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK) {
        TEST_FAIL("创建文件", res);
        return;
    }
    
    f_puts("第一行\n", &fil);
    f_putc('A', &fil);
    f_putc('\n', &fil);
    f_printf(&fil, "第%d行，数值: %d\n", 3, 123);
    f_close(&fil);
    TEST_PASS("字符串写入");
    
    /* 逐行读取 */
    res = f_open(&fil, "SD:/string_io.txt", FA_READ);
    if (res == FR_OK) {
        printf("  读取内容:\n");
        int line = 1;
        while (f_gets(line_buffer, sizeof(line_buffer), &fil)) {
            printf("    行%d: %s", line++, line_buffer);
        }
        f_close(&fil);
        TEST_PASS("字符串读取");
    } else {
        TEST_FAIL("打开文件读取", res);
    }
}

/*---------------------------------------------------------------------------*/
/* 测试10：文件重命名和移动                                                  */
/*---------------------------------------------------------------------------*/
void test_rename_move(void)
{
    FRESULT res;
    
    printf("\n=== 测试10: 重命名和移动 ===\n");
    
    /* 创建测试文件 */
    res = f_open(&fil, "SD:/old_name.txt", FA_CREATE_ALWAYS | FA_WRITE);
    if (res == FR_OK) {
        f_puts("重命名测试\n", &fil);
        f_close(&fil);
    } else {
        TEST_FAIL("创建文件", res);
        return;
    }
    
    /* 重命名 */
    res = f_rename("SD:/old_name.txt", "SD:/new_name.txt");
    if (res == FR_OK) {
        TEST_PASS("文件重命名");
        
        /* 验证新文件存在 */
        res = f_stat("SD:/new_name.txt", &fno);
        if (res == FR_OK) {
            printf("  新文件存在: %s\n", fno.fname);
        }
    } else {
        TEST_FAIL("文件重命名", res);
    }
    
    /* 移动到子目录 */
    res = f_rename("SD:/new_name.txt", "SD:/testdir/moved_file.txt");
    if (res == FR_OK) {
        TEST_PASS("文件移动");
    } else {
        TEST_FAIL("文件移动", res);
    }
}

/*---------------------------------------------------------------------------*/
/* 主测试函数                                                               */
/*---------------------------------------------------------------------------*/
void run_all_fatfs_tests(void)
{
    FRESULT res;
    
    printf("\n");
    printf("=========================================\n");
    printf("   FatFs 完整功能测试\n");
    printf("=========================================\n");
    
    /* 挂载文件系统 */
    printf("\n挂载SD卡...\n");
    res = f_mount(&fs, "SD:", 1);
    if (res != FR_OK) {
        printf("挂载失败! 错误代码: %d\n", res);
        printf("请检查:\n");
        printf("  1. SD卡是否正确插入\n");
        printf("  2. SDMMC接口是否初始化\n");
        printf("  3. SD卡是否已格式化为FAT32\n");
        return;
    }
    printf("挂载成功!\n");
    
    /* 运行所有测试 */
    test_basic_file_operations();
    test_long_filename_utf8();
    test_formatted_output();
    test_directory_operations();
    test_directory_search();
    test_file_attributes();
    test_disk_space();
    test_expand_and_seek();
    test_string_io();
    test_rename_move();
    
    /* 卸载文件系统 */
    printf("\n卸载SD卡...\n");
    f_unmount("SD:");
    
    printf("\n");
    printf("=========================================\n");
    printf("   测试完成!\n");
    printf("=========================================\n");
}

/* 单独测试某个功能 */
void run_single_test(int test_num)
{
    FRESULT res;
    
    res = f_mount(&fs, "SD:", 1);
    if (res != FR_OK) {
        printf("挂载失败!\n");
        return;
    }
    
    switch (test_num) {
        case 1: test_basic_file_operations(); break;
        case 2: test_long_filename_utf8(); break;
        case 3: test_formatted_output(); break;
        case 4: test_directory_operations(); break;
        case 5: test_directory_search(); break;
        case 6: test_file_attributes(); break;
        case 7: test_disk_space(); break;
        case 8: test_expand_and_seek(); break;
        case 9: test_string_io(); break;
        case 10: test_rename_move(); break;
        default: printf("无效的测试编号\n"); break;
    }
    
    f_unmount("SD:");
}
