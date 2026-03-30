////////////////////////////////////////////////
//
//  Standard Keyboard define: KeyDef.h
//
//  DirectInput 键盘扫描码定义文件
//  本文件定义了所有标准键盘按键对应的 DirectInput 扫描码（DIK_* 的等价宏）。
//  这些值是键盘硬件产生的原始扫描码，与 Windows 虚拟键码（VK_*）不同。
//  可直接用于 KEYSTATUSFLAG[] 数组的下标索引，以检测某个键是否被按下。
//
//  使用方式：
//    if (KEYSTATUSFLAG[KEY_A] & 0x80)  // 检测 A 键是否按下
//
////////////////////////////////////////////////

// ========================
// 功能键与特殊键
// ========================
#define KEY_ESC             0x01    // Escape 键
#define KEY_BACK            0x0E    // Backspace 退格键
#define KEY_TAB             0x0F    // Tab 制表键
#define KEY_ENTER           0x1C    // 主键盘 Enter 回车键
#define KEY_SPACE           0x39    // 空格键
#define KEY_CAPITAL         0x3A    // Caps Lock 大写锁定键

// ========================
// 主键盘数字行（1~0）
// ========================
#define KEY_1               0x02
#define KEY_2               0x03
#define KEY_3               0x04
#define KEY_4               0x05
#define KEY_5               0x06
#define KEY_6               0x07
#define KEY_7               0x08
#define KEY_8               0x09
#define KEY_9               0x0A
#define KEY_0               0x0B

// ========================
// 主键盘符号键
// ========================
#define KEY_MINUS           0x0C    // -  主键盘减号
#define KEY_EQUALS          0x0D    // =  等号
#define KEY_LBRACKET        0x1A    // [  左方括号
#define KEY_RBRACKET        0x1B    // ]  右方括号
#define KEY_SEMICOLON       0x27    // ;  分号
#define KEY_APOSTROPHE      0x28    // '  单引号
#define KEY_GRAVE           0x29    // `  反引号（重音符）
#define KEY_BACKSLASH       0x2B    // \  反斜杠
#define KEY_COMMA           0x33    // ,  主键盘逗号
#define KEY_PERIOD          0x34    // .  主键盘句点
#define KEY_SLASH           0x35    // /  主键盘正斜杠

// ========================
// 字母键（A~Z）
// ========================
#define KEY_A               0x1E
#define KEY_B               0x30
#define KEY_C               0x2E
#define KEY_D               0x20
#define KEY_E               0x12
#define KEY_F               0x21
#define KEY_G               0x22
#define KEY_H               0x23
#define KEY_I               0x17
#define KEY_J               0x24
#define KEY_K               0x25
#define KEY_L               0x26
#define KEY_M               0x32
#define KEY_N               0x31
#define KEY_O               0x18
#define KEY_P               0x19
#define KEY_Q               0x10
#define KEY_R               0x13
#define KEY_S               0x1F
#define KEY_T               0x14
#define KEY_U               0x16
#define KEY_V               0x2F
#define KEY_W               0x11
#define KEY_X               0x2D
#define KEY_Y               0x15
#define KEY_Z               0x2C

// ========================
// 功能键（F1~F15）
// ========================
#define KEY_F1              0x3B
#define KEY_F2              0x3C
#define KEY_F3              0x3D
#define KEY_F4              0x3E
#define KEY_F5              0x3F
#define KEY_F6              0x40
#define KEY_F7              0x41
#define KEY_F8              0x42
#define KEY_F9              0x43
#define KEY_F10             0x44
#define KEY_F11             0x57
#define KEY_F12             0x58
#define KEY_F13             0x64    // （NEC PC98 专用）
#define KEY_F14             0x65    // （NEC PC98 专用）
#define KEY_F15             0x66    // （NEC PC98 专用）

// ========================
// 修饰键（Shift / Ctrl / Alt）
// ========================
#define KEY_LSHIFT          0x2A    // 左 Shift
#define KEY_RSHIFT          0x36    // 右 Shift
#define KEY_LCTRL           0x1D    // 左 Ctrl
#define KEY_RCTRL           0x9D    // 右 Ctrl
#define KEY_LALT            0x38    // 左 Alt
#define KEY_RALT            0xB8    // 右 Alt（AltGr）

// ========================
// 锁定键
// ========================
#define KEY_NUMLOCK         0x45    // Num Lock 数字锁定键
#define KEY_SCROLL          0x46    // Scroll Lock 滚动锁定键

// ========================
// 小键盘数字键（Num Pad）
// ========================
#define KEY_NUMPAD0         0x52
#define KEY_NUMPAD1         0x4F
#define KEY_NUMPAD2         0x50
#define KEY_NUMPAD3         0x51
#define KEY_NUMPAD4         0x4B
#define KEY_NUMPAD5         0x4C
#define KEY_NUMPAD6         0x4D
#define KEY_NUMPAD7         0x47
#define KEY_NUMPAD8         0x48
#define KEY_NUMPAD9         0x49

// ========================
// 小键盘运算符键
// ========================
#define KEY_MULTIPLY        0x37    // *  小键盘乘号
#define KEY_SUBTRACT        0x4A    // -  小键盘减号
#define KEY_ADD             0x4E    // +  小键盘加号
#define KEY_DECIMAL         0x53    // .  小键盘小数点
#define KEY_DIVIDE          0xB5    // /  小键盘除号
#define KEY_NUMPADENTER     0x9C    // 小键盘 Enter 键
#define KEY_NUMPADCOMMA     0xB3    // ,  小键盘逗号（NEC PC98）
#define KEY_NUMPADEQUALS    0x8D    // =  小键盘等号（NEC PC98）

// ========================
// 方向键与编辑键（独立区域）
// ========================
#define KEY_HOME            0xC7    // Home 键
#define KEY_UP              0xC8    // ↑ 上方向键
#define KEY_PRIOR           0xC9    // Page Up 向上翻页
#define KEY_LEFT            0xCB    // ← 左方向键
#define KEY_RIGHT           0xCD    // → 右方向键
#define KEY_END             0xCF    // End 键
#define KEY_DOWN            0xD0    // ↓ 下方向键
#define KEY_NEXT            0xD1    // Page Down 向下翻页
#define KEY_INSERT          0xD2    // Insert 插入键
#define KEY_DELETE          0xD3    // Delete 删除键

// ========================
// Windows 系统键
// ========================
#define KEY_LWIN            0xDB    // 左 Windows 键
#define KEY_RWIN            0xDC    // 右 Windows 键
#define KEY_APPS            0xDD    // 应用程序菜单键（Context Menu）
#define KEY_SYSRQ           0xB7    // Print Screen / SysRq

// ========================
// 日文键盘专用键
// ========================
#define KEY_KANA            0x70    // 假名切换键（Japanese）
#define KEY_CONVERT         0x79    // 转换键（Japanese）
#define KEY_NOCONVERT       0x7B    // 无转换键（Japanese）
#define KEY_YEN             0x7D    // 日元符号键（Japanese）
#define KEY_CIRCUMFLEX      0x90    // 抑扬符 ^（Japanese）
#define KEY_AT              0x91    // @ 键（NEC PC98）
#define KEY_COLON           0x92    // : 冒号键（NEC PC98）
#define KEY_UNDERLINE       0x93    // _ 下划线键（NEC PC98）
#define KEY_KANJI           0x94    // 汉字切换键（Japanese）
#define KEY_STOP            0x95    // 停止键（NEC PC98）
#define KEY_AX              0x96    // AX 键（Japan AX）
#define KEY_UNLABELED       0x97    // 无标签键（J3100）

/*--------------------------------------*/
