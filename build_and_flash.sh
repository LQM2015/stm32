#!/bin/bash
# ========================================
# STM32H750 Bootloader 编译和烧录脚本
# ========================================

PROJECT_NAME="H750XBH6"
BOOTLOADER_DIR="Debug_Bootloader"
EXTFLASH_DIR="Debug_ExtFlash"
PROGRAMMER="STM32_Programmer_CLI"

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "========================================"
echo "STM32H750 Bootloader 自动编译脚本"
echo "========================================"
echo

show_menu() {
    echo "请选择操作:"
    echo "1. 编译 Bootloader"
    echo "2. 编译应用程序 (外部Flash)"
    echo "3. 编译全部 (Bootloader + 应用程序)"
    echo "4. 烧录 Bootloader"
    echo "5. 烧录应用程序"
    echo "6. 烧录全部"
    echo "7. 清理编译输出"
    echo "8. 退出"
    echo
}

build_bootloader() {
    echo
    echo "[1/1] 正在编译 Bootloader..."
    echo "----------------------------------------"
    # 这里需要根据实际的编译工具调整命令
    # 示例: 使用 make 或 STM32CubeIDE 命令行工具
    echo "请在 STM32CubeIDE 中手动编译 Bootloader 配置"
    echo "或者配置 makefile 后取消注释下面的命令:"
    # make -C $BOOTLOADER_DIR clean all
    echo "----------------------------------------"
    read -p "按回车继续..."
}

build_application() {
    echo
    echo "[1/1] 正在编译应用程序..."
    echo "----------------------------------------"
    echo "请在 STM32CubeIDE 中手动编译 ExtFlash 配置"
    echo "或者配置 makefile 后取消注释下面的命令:"
    # make -C $EXTFLASH_DIR clean all
    echo "----------------------------------------"
    read -p "按回车继续..."
}

build_all() {
    echo
    echo "[1/2] 正在编译 Bootloader..."
    echo "----------------------------------------"
    echo "请在 STM32CubeIDE 中先编译 Bootloader 配置"
    read -p "按回车继续..."
    echo
    echo "[2/2] 正在编译应用程序..."
    echo "----------------------------------------"
    echo "然后编译 ExtFlash 配置"
    read -p "按回车继续..."
}

flash_bootloader() {
    echo
    echo "[1/1] 正在烧录 Bootloader 到内部 Flash..."
    echo "----------------------------------------"
    
    if [ ! -f "$BOOTLOADER_DIR/$PROJECT_NAME.hex" ]; then
        echo -e "${RED}错误: 找不到 Bootloader hex 文件${NC}"
        echo "请先编译 Bootloader"
        read -p "按回车继续..."
        return
    fi

    echo "正在连接调试器..."
    $PROGRAMMER -c port=SWD -w $BOOTLOADER_DIR/$PROJECT_NAME.hex -v -s

    if [ $? -eq 0 ]; then
        echo
        echo -e "${GREEN}✓ Bootloader 烧录成功！${NC}"
    else
        echo
        echo -e "${RED}✗ Bootloader 烧录失败！${NC}"
        echo "错误代码: $?"
    fi
    echo "----------------------------------------"
    read -p "按回车继续..."
}

flash_application() {
    echo
    echo "[1/1] 正在烧录应用程序到外部 Flash..."
    echo "----------------------------------------"
    
    if [ ! -f "$EXTFLASH_DIR/$PROJECT_NAME.hex" ]; then
        echo -e "${RED}错误: 找不到应用程序 hex 文件${NC}"
        echo "请先编译应用程序"
        read -p "按回车继续..."
        return
    fi

    echo -e "${YELLOW}警告: 烧录外部 Flash 需要配置 External Loader${NC}"
    echo "请确保已安装 W25Q256 External Loader"
    echo
    read -p "是否继续? (y/N): " confirm
    if [[ ! $confirm =~ ^[Yy]$ ]]; then
        return
    fi

    echo "正在连接调试器并加载 External Loader..."
    # 注意: 需要指定正确的 External Loader 路径
    # $PROGRAMMER -c port=SWD -el "path/to/W25Q256.stldr"
    $PROGRAMMER -c port=SWD -w $EXTFLASH_DIR/$PROJECT_NAME.hex 0x90000000 -v

    if [ $? -eq 0 ]; then
        echo
        echo -e "${GREEN}✓ 应用程序烧录成功！${NC}"
    else
        echo
        echo -e "${RED}✗ 应用程序烧录失败！${NC}"
        echo "错误代码: $?"
        echo
        echo "可能的原因:"
        echo "1. 未配置 External Loader"
        echo "2. QSPI Flash 硬件连接问题"
        echo "3. Bootloader 未烧录"
    fi
    echo "----------------------------------------"
    read -p "按回车继续..."
}

flash_all() {
    echo
    echo "[1/2] 正在烧录 Bootloader..."
    echo "----------------------------------------"
    flash_bootloader
    echo
    echo "[2/2] 正在烧录应用程序..."
    echo "----------------------------------------"
    flash_application
}

clean() {
    echo
    echo "正在清理编译输出..."
    echo "----------------------------------------"
    if [ -d "$BOOTLOADER_DIR" ]; then
        echo "清理 Bootloader 目录..."
        rm -rf "$BOOTLOADER_DIR"
    fi
    if [ -d "$EXTFLASH_DIR" ]; then
        echo "清理应用程序目录..."
        rm -rf "$EXTFLASH_DIR"
    fi
    echo -e "${GREEN}✓ 清理完成${NC}"
    echo "----------------------------------------"
    read -p "按回车继续..."
}

# 主循环
while true; do
    clear
    echo "========================================"
    echo "STM32H750 Bootloader 自动编译脚本"
    echo "========================================"
    echo
    show_menu
    read -p "请输入选项 (1-8): " choice

    case $choice in
        1) build_bootloader ;;
        2) build_application ;;
        3) build_all ;;
        4) flash_bootloader ;;
        5) flash_application ;;
        6) flash_all ;;
        7) clean ;;
        8) 
            echo
            echo "感谢使用！再见！"
            exit 0
            ;;
        *)
            echo -e "${RED}无效选项，请重新选择${NC}"
            sleep 2
            ;;
    esac
done
