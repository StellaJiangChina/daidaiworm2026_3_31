/*  StdGraph.H  */
//
// 核心图形渲染头文件
//
// 提供渲染缓冲区管理、裁剪框控制、基础 2D 图形绘制（点/线/矩形/图像）
// 以及 PPM 图像文件加载功能。
//
// 渲染架构：
//   采用离屏缓冲区渲染策略——所有绘制操作写入 RenderBuffer（内存数组），
//   最终由 Refresh.H 中的 RefreshScreen() 负责将缓冲区内容拷贝到 DirectDraw 表面。
//
// 缓冲区布局（6页，每页 400×300 个 DWORD）：
//   Page 0 (MAINPAGE)  - 主渲染页，游戏每帧写入此页
//   Page 1 (ZBUFPAGE)  - Z 缓冲区（深度测试）
//   Page 2 (BLURPAGE)  - 运动模糊混合页（存储前一帧用于叠加）
//   Page 3 (BACKPAGE)  - 备份页（保存背景，用于水波、鼠标恢复等）
//   Page 4 (SFX1PAGE)  - 特效计算页1（水、火、烟雾等）
//   Page 5 (SFX2PAGE)  - 特效计算页2（备用）
//
// 颜色格式：BGRA 或 XRGB 32位，存储为 0x00RRGGBB
// 透明色键：0x00FF00FF（品红色），用于 CK（Color Key）系列绘图函数
//

#define _CRT_SECURE_NO_WARNINGS
// -------------------------------------
//			render buffer 
// -------------------------------------
#include <string.h>

// 渲染分辨率（内部渲染缓冲区尺寸，独立于屏幕分辨率）
// 实际显示时可缩放到 2x（800×600）或保持 1x（400×300）
#define		RENDER_WIDTH		400   // 渲染宽度（像素）
#define		RENDER_HEIGHT		300   // 渲染高度（像素）

// -------------------------------------

long		StatusFlag = FALSE;     // 是否显示状态栏（FPS、屏幕模式等信息），F12切换
long		MotionBlurFlag = 0;     // 运动模糊等级：0=关闭，1=轻度，2=重度（F11切换）

// -------------------------------------

/**
 * @brief 切换当前渲染目标页面
 * @param x 页面编号（0~5），切换后所有绘制操作将写入该页
 */
#define SetRenderPage(x)	lpRenderBuffer = (LPDWORD)RenderBuffer + RENDER_HEIGHT * RENDER_WIDTH * (x)

// 渲染缓冲区页面编号常量
#define	MAINPAGE	0   // 主页：每帧游戏内容绘制于此
#define	ZBUFPAGE	1   // Z缓冲页：用于深度测试（当前版本可能未完全启用）
#define	BLURPAGE	2   // 运动模糊页：存储前一帧图像，用于叠加混合
#define	BACKPAGE	3   // 备份页：保存背景快照（水波效果等需要从此读取原始背景）
#define	SFX1PAGE	4   // 特效计算页1：用于水波、火焰、烟雾等特效的中间计算
#define	SFX2PAGE	5   // 特效计算页2：备用特效或第二备份页

// 渲染缓冲区：6页连续内存（每页 400×300 个 DWORD = 480000 字节）
// 总大小：400 × 300 × 4 × 6 = 2,880,000 字节 ≈ 2.75 MB
DWORD		RenderBuffer[RENDER_HEIGHT * RENDER_WIDTH * 6];

// 当前渲染目标指针，默认指向第0页（MAINPAGE）
// 通过 SetRenderPage() 宏切换到不同页面
LPDWORD		lpRenderBuffer = (LPDWORD)RenderBuffer;

// -------------------------------------

// DirectDraw 接口指针
LPDIRECTDRAW			lpDD = NULL;           // DirectDraw 主对象
LPDIRECTDRAWSURFACE		lpDDSPrimary = NULL;   // 主显示表面（前缓冲区，显示给用户看）
LPDIRECTDRAWSURFACE		lpDDSBack = NULL;      // 后备缓冲区（绘制完成后翻转到前台）

// -------------------------------------
//			行起始偏移查找表 & 亮度表初始化
// -------------------------------------

// 行起始偏移表：LineStartOffset[y] = y * RENDER_WIDTH * 4（字节）
// 避免每次绘制像素时重新计算 y*宽度 的乘法，提高运行效率
DWORD		LineStartOffset[RENDER_HEIGHT]; // 每行在缓冲区中的字节偏移量

// 行像素偏移表：MultipleWidth[y] = y * RENDER_WIDTH（像素索引）
// 用于水波效果等需要直接按像素索引访问的场合
DWORD		MultipleWidth[RENDER_HEIGHT];

// 亮度映射表：Brightness[i] = i | (i<<8) | (i<<16)
// 将 0~255 的灰度值扩展为 XRGB 颜色（R=G=B=i），用于白噪声等特效
DWORD		Brightness[256];

// -------------------------------------

/**
 * @brief 初始化行起始偏移表、行像素倍数表和亮度映射表
 *
 * 程序启动时调用一次，预计算常用的查找表，避免运行时重复乘法运算：
 *   - Brightness[i]     = i * 0x10101  → 将灰度值 i 扩展为 0x00RRGGBB（R=G=B=i）
 *   - LineStartOffset[y]= y * 400 * 4  → 第 y 行的字节偏移（用于像素地址计算）
 *   - MultipleWidth[y]  = y * 400       → 第 y 行的 DWORD 索引（用于水波等特效）
 */
void SetLineStartOffset(void)
{
	long	y;

	// 构建灰度→颜色映射表（0x10101 = R、G、B三通道同步置位）
	for(y = 0; y < 256; y ++)
	{
		Brightness[y] = y * 0x10101;
	}

	// 构建每行字节偏移表和像素偏移表
	for(y = 0; y < RENDER_HEIGHT; y ++)
	{
		LineStartOffset[y] = y * RENDER_WIDTH * sizeof(DWORD); // 字节偏移
		MultipleWidth[y] = y * RENDER_WIDTH;                   // 像素偏移（DWORD单位）
	}
}

/*--------------------------------------*/
//			标准图形绘制函数
/*--------------------------------------*/

// 裁剪框边界（全局变量，决定所有绘制操作的可见区域）
// 默认值覆盖整个渲染缓冲区；可通过 SetClipBox() 限制绘制区域
long	XMIN = 0,					YMIN = 0;
long	XMAX = RENDER_WIDTH - 1,	YMAX = RENDER_HEIGHT - 1;

/**
 * @brief 设置当前绘制颜色（宏）
 * @param color 颜色值，格式为 0x00RRGGBB
 */
#define SetColor(color)			CURRENTCOLOR = (color)

DWORD	CURRENTCOLOR = 0xFFFFFF;    // 当前绘制颜色，默认白色

// 透明色键（Color Key）
// 品红色 0x00FF00FF = R:255 G:0 B:255，在自然图像中极少出现。
// 凡是像素颜色等于此值，在 CK 系列函数中会被视为"透明"而跳过绘制。
#define CK_VALUE	0x00FF00FF

// -------------------------------------

/**
 * @brief 将裁剪框重置为覆盖整个渲染区域
 *
 * 调用后 XMIN=0, YMIN=0, XMAX=399, YMAX=299，
 * 所有后续绘制操作均不受额外裁剪限制。
 */
void ResetClipBox(void)
{
	XMIN = 0;					YMIN = 0;
	XMAX = RENDER_WIDTH - 1;	YMAX = RENDER_HEIGHT - 1;
}

// -------------------------------------
/**
 * @brief 设置并约束裁剪框边界
 *
 * 该函数用于确保裁剪框的坐标顺序合法（左边界 <= 右边界，上边界 <= 下边界），
 * 并将其严格限制在渲染区域 [0, RENDER_WIDTH-1] × [0, RENDER_HEIGHT-1] 范围内，
 * 最后将处理后的最终边界值存储到全局变量 XMIN/YMIN/XMAX/YMAX 中。
 *
 * @param Left   裁剪框左边界 X 坐标（输入）
 * @param Top    裁剪框上边界 Y 坐标（输入）
 * @param Right  裁剪框右边界 X 坐标（输入）
 * @param Bottom 裁剪框下边界 Y 坐标（输入）
 */
void SetClipBox(long Left, long Top, long Right, long Bottom)
{
	long temp;

	// 1. 确保 Left <= Right（交换逻辑，对应第一段内联汇编）
	if (Right < Left)
	{
		temp = Left;
		Left = Right;
		Right = temp;
	}

	// 2. 确保 Top <= Bottom（交换逻辑，对应第二段内联汇编）
	if (Bottom < Top)
	{
		temp = Top;
		Top = Bottom;
		Bottom = temp;
	}

	// 3. 边界检查：将坐标限制在 [0, RENDER_WIDTH-1] 和 [0, RENDER_HEIGHT-1] 范围内
	if (Left < 0)
		Left = 0;
	if (Right >= RENDER_WIDTH)
		Right = RENDER_WIDTH - 1;
	if (Top < 0)
		Top = 0;
	if (Bottom >= RENDER_HEIGHT)
		Bottom = RENDER_HEIGHT - 1;

	// 4. 输出最终裁剪框边界
	XMIN = Left;
	YMIN = Top;
	XMAX = Right;
	YMAX = Bottom;
}

// -------------------------------------

/**
 * @brief 清零渲染缓冲区指定页面
 *
 * 将指定页面的整个帧缓冲区快速填充为 0（通常代表黑色或透明色）。
 *
 * @param Page 要清零的页面编号（从 0 开始）
 */
void ClearRenderBuffer(long Page)
{
	// 1. 计算目标页面的起始指针（对应原代码第一行）
	// 注意：使用 size_t 防止整数溢出
	DWORD* lpDst = (DWORD*)RenderBuffer + (size_t)RENDER_WIDTH * RENDER_HEIGHT * Page;

	// 2. 批量填充为 0（对应内联汇编的 REP STOSD）
	// 逻辑：将 lpDst 开始的 (宽×高) 个 DWORD（32位）全部设为 0
	// memset 是标准库函数，内部通常优化为类似 REP STOSD 的高效指令
	size_t byte_count = (size_t)RENDER_WIDTH * RENDER_HEIGHT * sizeof(DWORD);
	memset(lpDst, 0, byte_count);
}

// -------------------------------------

/**
 * @brief 将主渲染页（Page 0）内容备份到运动模糊页（Page 2）
 *
 * 使用 x86 REP MOVSD 高效内存拷贝，将当前帧图像复制到 BLURPAGE。
 * 在开启运动模糊时，每帧调用此函数保存"上一帧"，供后续混合使用。
 */
void BackupToMotionBlurBuffer(void)
{
	_asm {
		LEA		ESI, [RenderBuffer]                             // 源：主渲染页（Page 0）
		LEA		EDI, [ESI + RENDER_WIDTH * RENDER_HEIGHT * 4 * 2] // 目标：运动模糊页（Page 2）
		MOV		ECX, RENDER_WIDTH * RENDER_HEIGHT               // 拷贝像素数（DWORD）
		REP		MOVSD                                           // 快速批量复制
	}
}

// -------------------------------------

/**
 * @brief 将主渲染页（Page 0）内容备份到备份页（Page 3）
 *
 * 用于在进行水波渲染或鼠标绘制等需要"保护背景"的操作前，
 * 先将当前画面保存到 BACKPAGE，水波渲染时从此处读取原始背景像素。
 */
void BackupRenderBuffer(void)
{
	_asm {
		LEA		ESI, [RenderBuffer]                             // 源：主渲染页（Page 0）
		LEA		EDI, [ESI + RENDER_WIDTH * RENDER_HEIGHT * 4 * 3] // 目标：备份页（Page 3）
		MOV		ECX, RENDER_WIDTH * RENDER_HEIGHT
		REP		MOVSD
	}
}

// -------------------------------------

/**
 * @brief 无裁剪的像素绘制（内部快速版本）
 *
 * 不进行边界检查，直接将 CURRENTCOLOR 写入渲染缓冲区中 (X, Y) 位置。
 * 调用者须确保坐标在有效范围内，否则会越界写内存。
 * 命名前缀 "_" 表示"无裁剪"版本（高性能，但使用者负责边界安全）。
 *
 * @param X 像素横坐标
 * @param Y 像素纵坐标
 */
void _PSet(long X, long Y)
{
	// 像素地址 = 缓冲区基址 + 行字节偏移 + 列字节偏移
	*(unsigned long*)((BYTE*)lpRenderBuffer + LineStartOffset[Y] + X * 4) = CURRENTCOLOR;
}

// -------------------------------------

/**
 * @brief 带裁剪的安全像素绘制
 *
 * 先检查 (X, Y) 是否在裁剪框 [XMIN, XMAX] × [YMIN, YMAX] 内，
 * 超出范围则直接返回，不绘制。适合在不确定坐标合法性时使用。
 *
 * @param X 像素横坐标
 * @param Y 像素纵坐标
 */
void PSet(long X, long Y)
{
	if(X<XMIN || Y<YMIN || X>XMAX ||Y>YMAX) return;
	*(unsigned long*)((BYTE*)lpRenderBuffer + LineStartOffset[Y] + X * 4) = CURRENTCOLOR;
}

// -------------------------------------

/**
 * @brief 读取渲染缓冲区中指定位置的像素颜色
 *
 * 读取并返回 (X, Y) 处的像素值（XRGB 32位颜色）。
 * 超出裁剪框范围则返回 0（黑色）。
 *
 * @param X 像素横坐标
 * @param Y 像素纵坐标
 * @return 像素颜色值（0x00RRGGBB），坐标越界时返回 0
 */
DWORD Point(const long X, const long Y)
{
	if (X < XMIN || Y < YMIN || X > XMAX || Y > YMAX)
		return 0;

	// 计算像素地址：注意此处 lpRenderBuffer 是 LPDWORD，
	// 而 LineStartOffset 是字节偏移，故先转为 BYTE* 再加偏移
	DWORD* const pixelPtr = (DWORD*)(lpRenderBuffer + LineStartOffset[Y] + X * 4);

	return *pixelPtr;
}

// -------------------------------------

/**
 * @brief 无裁剪的直线绘制（Bresenham 算法）
 *
 * 使用 Bresenham 整数直线算法，从 (X1,Y1) 到 (X2,Y2) 绘制直线，
 * 不进行边界裁剪，调用者须保证两端点在有效范围内。
 *
 * @param X1,Y1 起点坐标
 * @param X2,Y2 终点坐标
 */
void _Line(long X1, long Y1, long X2, long Y2)
{
	long deltax, deltay, halfx, halfy, dotn;
	long x, y, dirx, diry, b;

	dirx = 1; diry = 1;
	if((deltax = X2 - X1) < 0)
	{
		deltax = -deltax;
		dirx = -1;
	}
	if((deltay = Y2 - Y1) < 0)
	{
		deltay = -deltay;
		diry = -1;
	}
	x = X1; y = Y1; b = 0;
	if(deltax < deltay)
	{
		halfy = deltay >> 1;
		dotn = deltay;
		do
		{
			_PSet(x, y);
			y += diry;
			b += deltax;
			if(b > halfy)
			{
				b -= deltay;
				x += dirx;
			}
		} while(dotn--);
    }
	else
	{
		halfx = deltax >> 1;
		dotn = deltax;
		do
		{
			_PSet(x, y);
			x += dirx;
			b += deltay;
			if(b > halfx)
			{
				b -= deltax;
				y += diry;
			}
		} while(dotn--);
	}
}

// -------------------------------------

/**
 * @brief 带裁剪的直线绘制（Bresenham 算法）
 *
 * 使用 Bresenham 整数直线算法绘制直线。
 * 逐像素绘制时通过 PSet() 进行裁剪框检查，确保不越界。
 * 性能略低于 _Line，但安全性更高。
 *
 * @param X1,Y1 起点坐标
 * @param X2,Y2 终点坐标
 */
void Line(long X1, long Y1, long X2, long Y2)
{
	long deltax, deltay, halfx, halfy, dotn;
	long x, y, dirx, diry, b;

	dirx = 1; diry = 1;
	if((deltax = X2 - X1) < 0)
	{
		deltax = -deltax;
		dirx = -1;
	}
	if((deltay = Y2 - Y1) < 0)
	{
		deltay = -deltay;
		diry = -1;
	}
	x = X1; y = Y1; b = 0;
	if(deltax < deltay)
	{
		halfy = deltay >> 1;
		dotn = deltay;
		do
		{
			PSet(x, y);
			y += diry;
			b += deltax;
			if(b > halfy)
			{
				b -= deltay;
				x += dirx;
			}
		} while(dotn--);
    }
	else
	{
		halfx = deltax >> 1;
		dotn = deltax;
		do
		{
			PSet(x, y);
			x += dirx;
			b += deltay;
			if(b > halfx)
			{
				b -= deltax;
				y += diry;
			}
		} while(dotn--);
	}
}

// -------------------------------------

/**
 * @brief 无裁剪的矩形填充
 *
 * 将 [Left, Right] × [Top, Bottom] 范围内所有像素填充为 CURRENTCOLOR。
 * 不做裁剪检查，调用者须确保坐标合法。
 *
 * @param Left,Top     矩形左上角坐标
 * @param Right,Bottom 矩形右下角坐标（闭区间，包含此行列）
 */
void _Box(long Left, long Top, long Right, long Bottom)
{
	// 1. 计算起始像素地址 (X=Left, Y=Top)
	DWORD* dest = (DWORD*)((BYTE*)lpRenderBuffer + LineStartOffset[Top] + Left * 4); // 每个像素4字节

	// 2. 计算矩形尺寸 (包含 Right/Bottom，即闭区间 [Left, Right] x [Top, Bottom])
	long width = Right - Left + 1;
	long height = Bottom - Top + 1;
	long pitch = RENDER_WIDTH * 4; // 每行字节跨度

	// 3. 逐行绘制矩形
	for (long y = 0; y < height; ++y) {
		// 填充当前行 (等价于 REP STOSD)
		DWORD* p = dest;
		for (long x = 0; x < width; ++x) {
			*p++ = CURRENTCOLOR;
		}
		// 移动到下一行起始位置
		dest = (DWORD*)((BYTE*)dest + pitch);
	}
}

/**
 * @brief 带裁剪的矩形填充
 *
 * 自动修正坐标顺序（确保 Left<=Right, Top<=Bottom），
 * 再将矩形裁剪到屏幕范围内，最后填充 CURRENTCOLOR。
 *
 * @param Left,Top     矩形左上角坐标（可大于Right/Bottom，函数自动交换）
 * @param Right,Bottom 矩形右下角坐标
 */
void Box(long Left, long Top, long Right, long Bottom)
{
	long temp;

	// 1. 坐标交换（确保 Left < Right, Top < Bottom）
	if (Right < Left)
	{
		temp = Left;
		Left = Right;
		Right = temp;
	}

	if (Bottom < Top)
	{
		temp = Top;
		Top = Bottom;
		Bottom = temp;
	}

	// 2. 边界裁剪（限制在屏幕范围内）
	if (Left < XMIN)        Left = XMIN;
	if (Right >= XMAX)      Right = XMAX - 1; // 注意：原汇编是 >= XMAX 时设为 XMAX，
	// 但根据绘制逻辑应为 XMAX-1，此处保持原样
	if (Top < YMIN)         Top = YMIN;
	if (Bottom >= YMAX)     Bottom = YMAX - 1;

	// 3. 绘制矩形（核心逻辑）
	const long width = Right - Left + 1;   // 矩形宽度（像素数）
	const long height = Bottom - Top + 1;   // 矩形高度（行数）

	// 计算起始像素地址：(Top, Left) 位置
	DWORD* dest = (DWORD*)((BYTE*)lpRenderBuffer + LineStartOffset[Top] + Left * 4);

	for (long y = 0; y < height; y++)
	{
		// 填充当前行
		DWORD* pixel = dest;
		for (long x = 0; x < width; x++)
		{
			*pixel++ = CURRENTCOLOR;
		}

		// 移动到下一行的同一列位置（加上一行的跨度）
		dest = (DWORD*)((BYTE*)dest + RENDER_WIDTH * 4);
	}
}

// -------------------------------------
/**
 * @brief 将图像绘制到渲染缓冲区（无透明处理）
 *
 * 将图像数据逐行拷贝到渲染缓冲区的 (X, Y) 位置，支持裁剪。
 * 图像格式：前两个 DWORD 为宽高，后续为按行存储的像素数据（XRGB 32位）。
 * 所有像素（包括品红色）均会被绘制，不做透明色跳过。
 *
 * @param X     目标左上角横坐标
 * @param Y     目标左上角纵坐标
 * @param Image 图像数据指针（格式：[宽][高][像素数据...]）
 */
// 不跳过CK颜色
void PutImage(long X, long Y, LPDWORD Image)
{
	long ImageW, ImageH;
	long PasteW, PasteH;
	long delta;

	// 1. 早期边界检查
	if (X > XMAX || Y > YMAX) return;

	// 2. 读取图像头信息
	ImageW = *Image++;
	ImageH = *Image++;

	if (X + ImageW <= XMIN || Y + ImageH <= YMIN) return;

	// 3. 初始化粘贴尺寸
	PasteW = ImageW;
	PasteH = ImageH;

	// 4. 左侧裁剪
	if (X < XMIN)
	{
		delta = XMIN - X;
		Image += delta;
		PasteW -= delta;
		X = XMIN;
	}

	// 5. 顶部裁剪
	if (Y < YMIN)
	{
		delta = YMIN - Y;
		Image += delta * ImageW;
		PasteH -= delta;
		Y = YMIN;
	}

	// 6. 右侧裁剪
	delta = X + PasteW - 1;
	if (delta > XMAX)
	{
		PasteW -= delta - XMAX;
	}

	// 7. 底部裁剪
	delta = Y + PasteH - 1;
	if (delta > YMAX)
	{
		PasteH -= delta - YMAX;
	}

	// --- 优化核心：高效内存复制 ---

	// 计算目标起始指针
	// 对应汇编: EDI = lpRenderBuffer + LineStartOffset[Y] + X*4
	DWORD* dst = (DWORD*)((BYTE*)lpRenderBuffer + LineStartOffset[Y] + X * sizeof(DWORD));
	DWORD* src = Image;

	// 计算行跨度（Stride）
	// src_skip: 源图像每行末尾跳过的像素数
	// dst_skip: 目标缓冲区每行末尾跳过的像素数
	const long src_skip = ImageW - PasteW;
	const long dst_skip = RENDER_WIDTH - PasteW;

	// 8. 循环复制
	for (long h = 0; h < PasteH; h++)
	{
		// 利用 memcpy 实现 REP MOVSD 的效果
		// 编译器通常会将 memcpy 内联为最优化的指令
		memcpy(dst, src, PasteW * sizeof(DWORD));

		// 移动指针到下一行
		src += PasteW + src_skip;
		dst += PasteW + dst_skip;
	}
}

// -------------------------------------
/**
 * @brief 带透明色（Color Key）处理的图像绘制
 *
 * 功能与 PutImage 相同，但绘制时跳过颜色为 CK_VALUE（0x00FF00FF 品红）的像素。
 * 用于绘制带透明区域的精灵图像（Sprite），如游戏角色、道具等。
 *
 * 性能优化点：
 * - 使用 register 关键字提示编译器将高频指针变量分配到寄存器
 * - 循环计数器使用递减模式，利用 CPU 零标志位避免比较指令
 * - 无论像素是否绘制，目标指针统一在判断后递增，减少分支内操作
 *
 * @param X     目标左上角横坐标
 * @param Y     目标左上角纵坐标
 * @param Image 图像数据指针（格式：[宽][高][像素数据...]）
 */
// 和尾巴掉了有关（注：指蛇身尾部消除时需用此函数恢复背景）
void PutImageCK(long X, long Y, LPDWORD Image)
{
	long ImageW, ImageH;
	long PasteW, PasteH;// 裁剪后绘制宽度，实际高度
	long temp;

	// --- 裁剪逻辑保持不变 (这部分通常不是性能瓶颈) ---
	if (X > XMAX || Y > YMAX) return;

	ImageW = *(Image++);
	ImageH = *(Image++);

	if (X + ImageW <= XMIN || Y + ImageH <= YMIN) return;

	PasteW = ImageW;
	PasteH = ImageH;

	if (X < XMIN) {
		temp = XMIN - X;
		Image += temp;
		PasteW -= temp;
		X = XMIN;
	}

	if (Y < YMIN) {
		temp = YMIN - Y;
		Image += (temp * ImageW);
		PasteH -= temp;
		Y = YMIN;
	}

	temp = X + PasteW - 1;
	if (temp > XMAX) PasteW -= (temp - XMAX);

	temp = Y + PasteH - 1;
	if (temp > YMAX) PasteH -= (temp - YMAX);

	// --- 优化后的核心绘制逻辑 ---

	// 1. 计算起始目标指针 (合并为一行计算，减少中间变量)
	DWORD* dest = (DWORD*)((BYTE*)lpRenderBuffer + (X * 4) + LineStartOffset[Y]);
	DWORD* src = Image;

	// 2. 计算行尾跳过的像素数 (循环外计算一次)
	const long src_skip = ImageW - PasteW;
	const long dst_skip = RENDER_WIDTH - PasteW;

	// 3. 声明寄存器变量 (提示编译器优先使用寄存器)
	register DWORD* r_src = src;
	register DWORD* r_dest = dest;
	register long rx, ry;

	// 4. 外层循环 (Y方向)：使用递减计数器
	for (ry = PasteH; ry > 0; ry--)
	{
		// 5. 内层循环 (X方向)：热点路径，极致优化
		for (rx = PasteW; rx > 0; rx--)
		{
			// 读取像素
			DWORD color = *r_src++;

			// Color Key 判断与绘制
			// 优化：无论是否绘制，dest 指针统一在最后递增，减少分支内操作
			if (color != CK_VALUE)
			{
				*r_dest = color;
			}
			r_dest++;
		}

		// 6. 行尾跳转
		r_src += src_skip;
		r_dest += dst_skip;
	}
}

// -------------------------------------
/**
 * @brief 带透明色处理的图像绘制（纯色替换版）
 *
 * 与 PutImageCK 类似，但遇到非透明像素时，
 * 不绘制图像本身的颜色，而是统一绘制 CURRENTCOLOR（当前颜色）。
 * "ST" = Stencil（模板）：图像仅作为形状遮罩，颜色由 CURRENTCOLOR 决定。
 * 使用内联汇编实现，性能极高。
 *
 * @param X     目标左上角横坐标
 * @param Y     目标左上角纵坐标
 * @param Image 图像数据指针（格式同 PutImageCK）
 */
void PutImageCKST(long X, long Y, LPDWORD Image)
{
	long	ImageW, ImageH;
	long	PasteW, PasteH;
	long	temp;

	if(X > XMAX || Y > YMAX)
		return;						//	Out of range

	ImageW = *(Image ++);
	ImageH = *(Image ++);

	if(X + ImageW <= XMIN || Y + ImageH <= YMIN)
		return;						//	Out of range

	PasteW = ImageW;	
	PasteH = ImageH;

	if(X < XMIN)
	{
		temp = XMIN - X;
		Image += temp;
		PasteW -= temp;
		X=XMIN;	
	}

	if(Y < YMIN)
	{
		temp = YMIN - Y;
		Image += (temp * ImageW);
		PasteH -= temp;
		Y=YMIN;	
	}

	temp = X + PasteW - 1;
	if(temp > XMAX)
	{
		PasteW -= (temp - XMAX);
	}

	temp = Y + PasteH - 1;
	if(temp > YMAX)
	{
		PasteH -= (temp - YMAX);
	}

	_asm {
		MOV		EBX, X
		MOV		ECX, Y
		MOV		EDI, lpRenderBuffer
		MOV		ECX, [LineStartOffset+ECX*4]
		LEA		EDI, [EDI+EBX*4]
		MOV		ESI, Image
		ADD		EDI, ECX

		MOV		EBX, ImageW
		SUB		EBX, PasteW
		SHL		EBX, 2

		MOV		EDX, RENDER_WIDTH
		SUB		EDX, PasteW
		SHL		EDX, 2

PutImage_CK_ST_LOOPY:

		MOV		ECX, PasteW

PutImage_CK_ST_LOOPX:

		LODSD
		CMP		EAX, CK_VALUE
		JZ		PutImage_CK_ST_LOOP_SKIP
		MOV     EAX, CURRENTCOLOR
		STOSD

		DEC		ECX
		JNZ		PutImage_CK_ST_LOOPX

		ADD		ESI, EBX
		ADD		EDI, EDX
		DEC		PasteH
		JNZ		PutImage_CK_ST_LOOPY
	}

	return;

PutImage_CK_ST_LOOP_SKIP:

	_asm {
		ADD		EDI, 4
		DEC		ECX
		JNZ		PutImage_CK_ST_LOOPX

		ADD		ESI, EBX
		ADD		EDI, EDX
		DEC		PasteH
		JNZ		PutImage_CK_ST_LOOPY
	}
}

// -------------------------------------
/**
 * @brief 分配图像缓冲区内存
 *
 * 分配并清零 (ImageW * ImageH + 2) 个 DWORD 大小的内存块。
 * 前 2 个 DWORD 预留给宽高信息（由调用者填写），后续存储像素数据。
 *
 * @param ImageW 图像宽度（像素）
 * @param ImageH 图像高度（像素）
 * @return 分配的缓冲区指针，失败时返回 NULL
 */
LPDWORD AllocImageBuffer(long ImageW, long ImageH)
{
	LPVOID		res;

	res = calloc(ImageW * ImageH + 2, 4);

	return (LPDWORD)res;
}

// -------------------------------------

/**
 * @brief 释放图像缓冲区内存
 *
 * 释放由 AllocImageBuffer() 分配的图像缓冲区。
 * 内部调用标准库 free() 函数。
 *
 * @param Image 要释放的图像缓冲区指针，可为 NULL（函数会检查）
 */
void FreeImageBuffer(LPDWORD Image)
{
	if(Image != NULL) free(Image);
}

// -------------------------------------

/**
 * @brief 无裁剪的图像截取（从渲染缓冲区读取像素）
 *
 * 从当前渲染缓冲区（lpRenderBuffer 指向的页面）中，
 * 截取指定矩形区域的像素数据，存储到输出数组中。
 * 不做边界检查，调用者须确保坐标和尺寸合法。
 * 命名前缀 "_" 表示"无裁剪"版本。
 *
 * 输出格式：
 *   Image[0] = ImageW（宽度）
 *   Image[1] = ImageH（高度）
 *   Image[2...] = 按行存储的像素数据（XRGB 32位）
 *
 * @param X      源区域左上角横坐标
 * @param Y      源区域左上角纵坐标
 * @param ImageW 要截取的宽度（像素）
 * @param ImageH 要截取的高度（像素）
 * @param Image  输出缓冲区指针（须预先分配足够空间）
 */
void _GetImage(long X, long Y, long ImageW, long ImageH, LPDWORD Image)
{
	if (Image == NULL)
		return;

	// 1. 存储图像宽高到输出数组的前两位
	*Image++ = ImageW;
	*Image++ = ImageH;

	// 2. 计算源数据的起始字节地址
	//查表+一维数组的方式模拟二维数组访问的
	BYTE* pSrc = (BYTE*)lpRenderBuffer + LineStartOffset[Y] + X * sizeof(DWORD);

	// 3. 渲染缓冲区每行的总字节数
	const long rowTotalBytes = RENDER_WIDTH * sizeof(DWORD);

	// 4. 循环复制每一行
	for (long y = 0; y < ImageH; ++y)
	{
		// 一次性复制整行（等价于汇编的REP MOVSD）
		memcpy(Image, pSrc, ImageW * sizeof(DWORD));
		// 输出指针移动到下一行存储位置
		Image += ImageW;
		// 源指针移动到下一行的X列位置
		pSrc += rowTotalBytes;
	}
}

// -------------------------------------

/**
 * @brief 带裁剪的图像截取（从渲染缓冲区读取像素）
 *
 * 从当前渲染缓冲区中截取指定区域的像素数据。
 * 先进行边界检查，确保截取区域与裁剪框有交集，
 * 然后调用 _GetImage 的逻辑进行复制。
 *
 * 注意：当前实现未完整处理裁剪（与 _GetImage 相同），
 * 实际使用时建议先检查坐标合法性。
 *
 * @param X      源区域左上角横坐标
 * @param Y      源区域左上角纵坐标
 * @param ImageW 要截取的宽度（像素）
 * @param ImageH 要截取的高度（像素）
 * @param Image  输出缓冲区指针
 */
void GetImage(long X, long Y, long ImageW, long ImageH, LPDWORD Image)
{
	long	PasteW, PasteH;
	long	temp;

	if (Image == NULL) return;

	if (X > XMAX || Y > YMAX)
		return;						//	超出范围

	if (X + ImageW <= XMIN || Y + ImageH <= YMIN)
		return;						//	超出范围

	// 1. 存储图像宽高到输出数组的前两位
	*Image++ = ImageW;
	*Image++ = ImageH;

	// 2. 计算源数据的起始字节地址
	// 查表+一维数组的方式模拟二维数组访问
	BYTE* pSrc = (BYTE*)lpRenderBuffer + LineStartOffset[Y] + X * sizeof(DWORD);

	// 3. 渲染缓冲区每行的总字节数
	const long rowTotalBytes = RENDER_WIDTH * sizeof(DWORD);

	// 4. 循环复制每一行
	for (long y = 0; y < ImageH; ++y)
	{
		// 一次性复制整行（等价于汇编的 REP MOVSD）
		memcpy(Image, pSrc, ImageW * sizeof(DWORD));
		// 输出指针移动到下一行存储位置
		Image += ImageW;
		// 源指针移动到下一行的 X 列位置
		pSrc += rowTotalBytes;
	}
}

// -------------------------------------
//			图像文件加载函数
// -------------------------------------

/**
 * @brief 加载 PPM 格式图像文件
 *
 * PPM（Portable Pixmap）是一种简单的未压缩图像格式，
 * 本函数支持 P6 格式（二进制 RGB 数据）。
 *
 * PPM 文件格式：
 *   P6                          - 格式标识（二进制 RGB）
 *   # 注释行（可选）
 *   宽度 高度                  - 图像尺寸（ASCII 数字）
 *   255                         - 最大颜色值（通常为 255）
 *   [RGB 数据...]              - 每个像素 3 字节（R, G, B）
 *
 * 加载后的图像数据格式：
 *   [0] = 宽度（DWORD）
 *   [1] = 高度（DWORD）
 *   [2...] = XRGB 32位像素数据
 *
 * @param filename PPM 图像文件路径
 * @return 图像缓冲区指针（须用 FreeImageBuffer 释放），失败返回 NULL
 */
LPDWORD LoadPPMImageFile(LPSTR filename)
{
	char	fbuf[128];
	BYTE	Dot[4];
	long	PPMw, PPMh;
	long	i, j, n=0;
	LPDWORD	lpData;
	LPDWORD	res = NULL;
	FILE	*fp;

	fopen_s(&fp,filename,"rb");
	if(fp == NULL)
	{
		InitFail((char*)"Load Image File Fail");
		return NULL;
	}

	fread(fbuf, 1, 128, fp);

	for(i = 0; i < 128; i ++)
	{
		if(fbuf[i] == 0x0A)
		{
			n++;
			if(n == 2)
			{
				char	*stopstring;

				PPMw = strtol(fbuf + i + 1, &stopstring, 10);
				PPMh = strtol(stopstring + 1, &stopstring, 10);
			}
			else
			if(n == 4)
			{

				res = AllocImageBuffer(PPMw, PPMh);
	
				lpData = res;
				
				*(lpData ++) = PPMw;
				*(lpData ++) = PPMh;

				fseek(fp, i + 1, SEEK_SET);

				n = PPMw * PPMh;

				for(j = 0; j < n; j ++)
				{
					fread(Dot, 3, 1, fp);
					*(lpData ++)=(Dot[0]<<16) + (Dot[1]<<8) + Dot[2];
				}
			}
		}
	}
	fclose(fp);
	return res;
}

// -------------------------------------

/**
 * @brief 关闭/释放图像文件
 *
 * 释放由 LoadPPMImageFile() 加载的图像缓冲区。
 * 这是 FreeImageBuffer() 的包装函数，提供更语义化的接口。
 *
 * @param Image 要关闭的图像缓冲区指针
 */
void ClosePicFile(LPDWORD Image)
{
	FreeImageBuffer(Image);
}

