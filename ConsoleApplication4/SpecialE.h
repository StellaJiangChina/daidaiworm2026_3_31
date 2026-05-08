/*  SpecialE.H  */
//
// 特殊图像效果渲染模块
//
// 提供多种基于 MMX 指令集的图像混合与特效处理函数：
//   - 加法混合 (Add)
//   - 减法混合 (Sub)
//   - 亮度调节 (Bright)
//   - Alpha 混合 (Alpha Blend)
//   - 带透明色的上述混合模式
//   - 白噪声生成
//   - 水波纹效果 (Ripple Effect)
//
// 技术特点：
//   所有混合函数均使用 MMX 指令（PADDUSB, PSUBUSB, PMULLW 等）
//   实现像素级的并行处理，每个 MMX 寄存器可同时处理 4 个像素通道。
//   这是 1999-2000 年代流行的 SIMD 优化技术。
//
// 混合模式说明：
//   AD = Additive (加法混合): 源像素 + 目标像素，结果饱和到 255
//   SB = Subtractive (减法混合): 目标像素 - 源像素，结果饱和到 0
//   BR = Brightness (亮度调节): 源像素 * Bright / 256
//   AB = Alpha Blend (Alpha混合): 源像素*Alpha + 目标像素*(256-Alpha) / 256
//   CK = Color Key (透明色): 跳过品红色 (0x00FF00FF) 像素
//

// -------------------------------------
//			特殊图像混合函数
// -------------------------------------

// Alpha 混合系数缓冲区（用于 MMX 乘法）
// ALPHA1 存储源图像的 Alpha 值，ALPHA2 存储 (256-Alpha)
// 每个 WORD 重复 4 次以填充 64 位 MMX 寄存器
//WORD	ALPHA1[4];
//WORD	ALPHA2[4];

// -------------------------------------

/**
 * @brief 加法混合图像绘制 (Additive Blend)
 *
 * 将图像以加法模式混合到渲染缓冲区：
 *   结果 = 源像素 + 目标像素（每个通道独立，饱和到 255）
 *
 * 使用 MMX PADDUSB 指令实现无符号字节饱和加法，
 * 适合制作发光、火焰、爆炸等亮部叠加效果。
 *
 * @param X     目标左上角横坐标
 * @param Y     目标左上角纵坐标
 * @param Image 图像数据指针（格式：[宽][高][像素数据...]）
 */
#include "stdint.h"
void PutImageAD(long X, long Y, LPDWORD Image)
{
	long	ImageW, ImageH;
	long	PasteW, PasteH;
	long	temp;

	if(X > XMAX || Y > YMAX) return;

	ImageW = *(Image ++);
	ImageH = *(Image ++);

	if(X + ImageW <= XMIN || Y + ImageH <= YMIN) return;

	PasteW = ImageW;	
	PasteH = ImageH;

	// 左边界裁剪
	if(X < XMIN)
	{
		temp = XMIN - X;
		Image += temp;
		PasteW -= temp;
		X=XMIN;	
	}

	// 上边界裁剪
	if(Y < YMIN)
	{
		temp = YMIN - Y;
		Image += (temp * ImageW);
		PasteH -= temp;
		Y=YMIN;	
	}

	// 右边界裁剪
	temp = X + PasteW - 1;
	if(temp > XMAX)
	{
		PasteW -= (temp - XMAX);
	}

	// 下边界裁剪
	temp = Y + PasteH - 1;
	if(temp > YMAX)
	{
		PasteH -= (temp - YMAX);
	}

	// MMX 汇编实现加法混合
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

PutImage_AD_LOOPY:

		MOV		ECX, PasteW

PutImage_AD_LOOPX:

		LODSD							// EAX = [ESI], ESI += 4

		MOVD		MM1, [EDI]			// MM1 = 目标像素
		MOVD		MM0, EAX			// MM0 = 源像素
		PADDUSB		MM0, MM1			// MM0 = 源 + 目标（饱和加法）
		MOVD		EAX, MM0

		STOSD							// [EDI] = EAX, EDI += 4

		DEC		ECX
		JNZ		PutImage_AD_LOOPX

		ADD		ESI, EBX			// 跳到图像下一行
		ADD		EDI, EDX			// 跳到渲染缓冲区下一行

		DEC		PasteH
		JNZ		PutImage_AD_LOOPY
		EMMS							// 清空 MMX 状态
	}
}

// -------------------------------------
//负责虫子死后的混合正确颜色 
/**
 * @brief 减法混合图像绘制 (Subtractive Blend)
 *
 * 将图像以减法模式混合到渲染缓冲区：
 *   结果 = 目标像素 - 源像素（每个通道独立，饱和到 0）
 *
 * 使用 MMX PSUBUSB 指令实现无符号字节饱和减法，
 * 适合制作阴影、暗部叠加、腐蚀等效果。
 *
 * @param X     目标左上角横坐标
 * @param Y     目标左上角纵坐标
 * @param Image 图像数据指针
 */
/**
 * @brief 减法混合绘制一张图像
 * @param X  目标左上角X
 * @param Y  目标左上角Y
 * @param Image 图像数据，前两个DWORD分别是宽和高，后面是图像内容（行优先，像素为DWORD：ARGB）
 */
static inline uint8_t sub_sat8(uint8_t a, uint8_t b)
{
	return (a > b) ? (uint8_t)(a - b) : 0;
}

/*
 * PutImageSB:
 * Image 指向的内存布局：第一个 DWORD = 宽 (ImageW)
 *                         第二个 DWORD = 高 (ImageH)
 *                         后续为像素数据（按行，像素为 32-bit ARGB）
 */
void PutImageSB(long X, long Y, LPDWORD Image)
{
	long ImageW, ImageH;
	long PasteW, PasteH;
	long temp;

	if (X > XMAX || Y > YMAX) return;

	ImageW = (long)(Image[0]);
	ImageH = (long)(Image[1]);
	Image += 2; /* 跳过宽高，指向像素数据 */

	if (X + ImageW <= XMIN || Y + ImageH <= YMIN) return;

	PasteW = ImageW;
	PasteH = ImageH;

	if (X < XMIN)
	{
		temp = XMIN - X;
		Image += temp;    /* 向右跳过 temp 列像素 */
		PasteW -= temp;
		X = XMIN;
	}

	if (Y < YMIN)
	{
		temp = YMIN - Y;
		Image += (temp * ImageW); /* 向下跳过若干整行 */
		PasteH -= temp;
		Y = YMIN;
	}

	temp = X + PasteW - 1;
	if (temp > XMAX)
	{
		PasteW -= (temp - XMAX);
	}

	temp = Y + PasteH - 1;
	if (temp > YMAX)
	{
		PasteH -= (temp - YMAX);
	}

	if (PasteW <= 0 || PasteH <= 0) return;

	/* 主循环：逐行逐像素做通道饱和减法 (dst_chan = dst_chan - src_chan, clamp >=0) */
	for (long row = 0; row < PasteH; ++row)
	{
		/* 源像素行起始 */
		DWORD* pSrc = Image + row * ImageW;

		/* 目标行起始：
		   LineStartOffset 存的是字节偏移（和你原汇编一致）
		   把字节偏移转换为像素索引除以4（每像素4字节） */
		int lineOffsetBytes = LineStartOffset[Y + row];
		DWORD* pDst = lpRenderBuffer + (lineOffsetBytes / 4) + X;

		for (long col = 0; col < PasteW; ++col)
		{
			uint32_t s = pSrc[col];
			uint32_t d = pDst[col];

			uint8_t sa = (uint8_t)((s >> 24) & 0xFF);
			uint8_t sr = (uint8_t)((s >> 16) & 0xFF);
			uint8_t sg = (uint8_t)((s >> 8) & 0xFF);
			uint8_t sb = (uint8_t)(s & 0xFF);

			uint8_t da = (uint8_t)((d >> 24) & 0xFF);
			uint8_t dr = (uint8_t)((d >> 16) & 0xFF);
			uint8_t dg = (uint8_t)((d >> 8) & 0xFF);
			uint8_t db = (uint8_t)(d & 0xFF);

			uint8_t ra = sub_sat8(da, sa);
			uint8_t rr = sub_sat8(dr, sr);
			uint8_t rg = sub_sat8(dg, sg);
			uint8_t rb = sub_sat8(db, sb);

			pDst[col] = ((uint32_t)ra << 24) | ((uint32_t)rr << 16) | ((uint32_t)rg << 8) | (uint32_t)rb;
		}
	}
}



// -------------------------------------
//负责刚开始加载第一张首页图片的淡入淡出
/**
 * @brief 亮度调节图像绘制 (Brightness)
 *
 * 将图像以指定亮度绘制到渲染缓冲区：
 *   结果 = 源像素 * Bright / 256
 *
 * Bright 范围：0~255
 *   - 0: 完全黑色（绘制黑色矩形）
 *   - 255: 原始亮度（等同于 PutImage）
 *   - 中间值: 按比例缩放每个颜色通道
 *
 * 使用 MMX PMULLW + PSRLW 实现 16 位定点乘法。
 *
 * @param X      目标左上角横坐标
 * @param Y      目标左上角纵坐标
 * @param Image  图像数据指针
 * @param Bright 亮度系数 (0~255)
 */
/*
 * PutImageBR:
 * Image 内存布局：Image[0]=宽, Image[1]=高, 之后为像素数据（按行，像素为 32-bit ARGB）
 * Bright: 0..255（>255 表示直接用 PutImage）
 */
void PutImageBR(long X, long Y, LPDWORD Image, long Bright)
{
	long ImageW = (long)Image[0];
	long ImageH = (long)Image[1];
	long PasteW, PasteH;
	long temp;

	/* Bright <= 0: 绘制黑色矩形（使用原始宽高） */
	if (Bright <= 0)
	{
		SetColor(0);
		Box(X, Y, X + ImageW - 1, Y + ImageH - 1);
		return;
	}
	/* Bright > 255: 直接正常绘制（调用原 PutImage） */
	if (Bright > 255)
	{
		PutImage(X, Y, Image);
		return;
	}

	if (X > XMAX || Y > YMAX) return;

	/* 跳过宽高，指向像素数据 */
	Image += 2;

	if (X + ImageW <= XMIN || Y + ImageH <= YMIN) return;

	PasteW = ImageW;
	PasteH = ImageH;

	if (X < XMIN)
	{
		temp = XMIN - X;
		Image += temp;        /* 跳过左侧若干列像素 */
		PasteW -= temp;
		X = XMIN;
	}

	if (Y < YMIN)
	{
		temp = YMIN - Y;
		Image += (temp * ImageW); /* 跳过若干整行 */
		PasteH -= temp;
		Y = YMIN;
	}

	temp = X + PasteW - 1;
	if (temp > XMAX)
	{
		PasteW -= (temp - XMAX);
	}

	temp = Y + PasteH - 1;
	if (temp > YMAX)
	{
		PasteH -= (temp - YMAX);
	}

	if (PasteW <= 0 || PasteH <= 0) return;

	/*
	 * 对每个像素的每个通道做 (src_chan * Bright) >> 8
	 * 对应原 MMX: PUNPCKLBW + PMULLW + PSRLW + PACKUSWB
	 */
	for (long row = 0; row < PasteH; ++row)
	{
		DWORD* pSrc = Image + row * ImageW;
		/* LineStartOffset 存的是字节偏移，与原汇编一致 */
		int lineOffsetBytes = LineStartOffset[Y + row];
		DWORD* pDst = lpRenderBuffer + (lineOffsetBytes / 4) + X;

		for (long col = 0; col < PasteW; ++col)
		{
			uint32_t s = pSrc[col];
			/* 分离 ARGB 通道 */
			unsigned int sa = (s >> 24) & 0xFF;
			unsigned int sr = (s >> 16) & 0xFF;
			unsigned int sg = (s >> 8) & 0xFF;
			unsigned int sb = (s) & 0xFF;

			/* 乘以 Bright 并右移 8 位 (等同于 /256) */
			unsigned int ra = (sa * (unsigned int)Bright) >> 8;
			unsigned int rr = (sr * (unsigned int)Bright) >> 8;
			unsigned int rg = (sg * (unsigned int)Bright) >> 8;
			unsigned int rb = (sb * (unsigned int)Bright) >> 8;

			/* 结果必然在 0..255 范围内，无需额外饱和 */
			pDst[col] = (ra << 24) | (rr << 16) | (rg << 8) | rb;
		}
	}
}

// -------------------------------------
//start按回车进入后的淡入淡出
/**
 * @brief Alpha 混合图像绘制 (Alpha Blend)
 *
 * 将图像以 Alpha 透明度混合到渲染缓冲区：
 *   结果 = 源像素 * Alpha/256 + 目标像素 * (256-Alpha)/256
 *
 * Alpha 范围：0~255
 *   - 0: 完全透明（不绘制）
 *   - 255: 完全不透明（等同于 PutImage）
 *   - 中间值: 半透明混合
 *
 * 这是标准的图像叠加混合模式，适合制作淡入淡出、
 * 阴影、玻璃效果等。
 *
 * @param X     目标左上角横坐标
 * @param Y     目标左上角纵坐标
 * @param Image 图像数据指针
 * @param Alpha 透明度系数 (0~255)
 */
void PutImageAB(long X, long Y, LPDWORD Image, long Alpha)
{
	long ImageW, ImageH;
	long PasteW, PasteH;
	long temp;

	/* 与原代码行为保持一致：Alpha <= 0：不绘制；Alpha > 255：直接调用 PutImage */
	if (Alpha <= 0) {
		return;
	}
	if (Alpha > 255) {
		PutImage(X, Y, Image);
		return;
	}

	if (X > XMAX || Y > YMAX) return;

	/* 读取宽高（保留 Image 指针未被修改的情况以便上面的 PutImage 调用能正确使用） */
	ImageW = (long)Image[0];
	ImageH = (long)Image[1];

	/* 跳过宽高，指向像素数据 */
	LPDWORD pixelData = Image + 2;

	if (X + ImageW <= XMIN || Y + ImageH <= YMIN) return;

	PasteW = ImageW;
	PasteH = ImageH;

	if (X < XMIN)
	{
		temp = XMIN - X;
		pixelData += temp;    /* 跳过左侧若干列像素 */
		PasteW -= temp;
		X = XMIN;
	}

	if (Y < YMIN)
	{
		temp = YMIN - Y;
		pixelData += (temp * ImageW); /* 跳过若干整行 */
		PasteH -= temp;
		Y = YMIN;
	}

	temp = X + PasteW - 1;
	if (temp > XMAX)
	{
		PasteW -= (temp - XMAX);
	}

	temp = Y + PasteH - 1;
	if (temp > YMAX)
	{
		PasteH -= (temp - YMAX);
	}

	if (PasteW <= 0 || PasteH <= 0) return;

	/* 计算系数 */
	unsigned int a_src = (unsigned int)Alpha;
	unsigned int a_dst = (unsigned int)(256 - Alpha);

	/* 主循环：逐行逐像素混合 */
	for (long row = 0; row < PasteH; ++row)
	{
		DWORD* pSrc = pixelData + row * ImageW;
		/* LineStartOffset 存的是字节偏移（与原汇编一致） */
		int lineOffsetBytes = LineStartOffset[Y + row];
		DWORD* pDst = lpRenderBuffer + (lineOffsetBytes / 4) + X;

		for (long col = 0; col < PasteW; ++col)
		{
			uint32_t s = pSrc[col];
			uint32_t d = pDst[col];

			unsigned int sa = (s >> 24) & 0xFF;
			unsigned int sr = (s >> 16) & 0xFF;
			unsigned int sg = (s >> 8) & 0xFF;
			unsigned int sb = (s) & 0xFF;

			unsigned int da = (d >> 24) & 0xFF;
			unsigned int dr = (d >> 16) & 0xFF;
			unsigned int dg = (d >> 8) & 0xFF;
			unsigned int db = (d) & 0xFF;

			/* (src*Alpha + dst*(256-Alpha)) >> 8 */
			unsigned int ra = (sa * a_src + da * a_dst) >> 8;
			unsigned int rr = (sr * a_src + dr * a_dst) >> 8;
			unsigned int rg = (sg * a_src + dg * a_dst) >> 8;
			unsigned int rb = (sb * a_src + db * a_dst) >> 8;

			pDst[col] = (ra << 24) | (rr << 16) | (rg << 8) | rb;
		}
	}
}

/* PutImageCKAD: 若源像素 != CK_VALUE，则目标像素 = saturate(dst + src)（每通道 0..255） */
void PutImageCKAD(long X, long Y, LPDWORD Image)
{
	long ImageW, ImageH;
	long PasteW, PasteH;
	long temp;

	if (X > XMAX || Y > YMAX)
		return;

	/* 读取宽高并移动到像素数据 */
	ImageW = (long)(Image[0]);
	ImageH = (long)(Image[1]);
	Image += 2; /* 指向像素数据 */

	if (X + ImageW <= XMIN || Y + ImageH <= YMIN)
		return;

	PasteW = ImageW;
	PasteH = ImageH;

	if (X < XMIN)
	{
		temp = XMIN - X;
		Image += temp;    /* 跳过左侧 temp 列 */
		PasteW -= temp;
		X = XMIN;
	}

	if (Y < YMIN)
	{
		temp = YMIN - Y;
		Image += (temp * ImageW); /* 跳过若干整行 */
		PasteH -= temp;
		Y = YMIN;
	}

	temp = X + PasteW - 1;
	if (temp > XMAX)
	{
		PasteW -= (temp - XMAX);
	}

	temp = Y + PasteH - 1;
	if (temp > YMAX)
	{
		PasteH -= (temp - YMAX);
	}

	if (PasteW <= 0 || PasteH <= 0) return;

	for (long row = 0; row < PasteH; ++row)
	{
		DWORD* pSrc = Image + row * ImageW;
		int lineOffsetBytes = LineStartOffset[Y + row]; /* 假定为字节偏移 */
		DWORD* pDst = lpRenderBuffer + (lineOffsetBytes / 4) + X;

		for (long col = 0; col < PasteW; ++col)
		{
			uint32_t s = pSrc[col];
			if (s == CK_VALUE) {
				/* 透明色：跳过 */
				continue;
			}

			uint32_t d = pDst[col];

			unsigned int sa = (s >> 24) & 0xFF;
			unsigned int sr = (s >> 16) & 0xFF;
			unsigned int sg = (s >> 8) & 0xFF;
			unsigned int sb = (s) & 0xFF;

			unsigned int da = (d >> 24) & 0xFF;
			unsigned int dr = (d >> 16) & 0xFF;
			unsigned int dg = (d >> 8) & 0xFF;
			unsigned int db = (d) & 0xFF;

			/* 每通道相加并饱和到 0..255 */
			unsigned int ra = sa + da;
			unsigned int rr = sr + dr;
			unsigned int rg = sg + dg;
			unsigned int rb = sb + db;

			if (ra > 255) ra = 255;
			if (rr > 255) rr = 255;
			if (rg > 255) rg = 255;
			if (rb > 255) rb = 255;

			pDst[col] = (ra << 24) | (rr << 16) | (rg << 8) | rb;
		}
	}
}

// -------------------------------------

/**
 * @brief 带透明色的减法混合图像绘制
 *
 * 功能同 PutImageSB，但跳过颜色为 CK_VALUE（品红）的像素。
 *
 * @param X     目标左上角横坐标
 * @param Y     目标左上角纵坐标
 * @param Image 图像数据指针
 */
void PutImageCKSB(long X, long Y, LPDWORD Image)
{
	long	ImageW, ImageH;
	long	PasteW, PasteH;
	long	temp;

	if(X > XMAX || Y > YMAX)
		return;						//	超出范围

	ImageW = *(Image ++);
	ImageH = *(Image ++);

	if(X + ImageW <= XMIN || Y + ImageH <= YMIN)
		return;						//	超出范围

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

PutImage_CK_SB_LOOPY:

		MOV		ECX, PasteW

PutImage_CK_SB_LOOPX:

		LODSD
		CMP		EAX, CK_VALUE
		JZ		PutImage_CK_SB_LOOP_SKIP

		MOVD		MM1, EAX
		MOVD		MM0, [EDI]
		PSUBUSB		MM0, MM1
		MOVD		EAX, MM0

		STOSD

		DEC		ECX
		JNZ		PutImage_CK_SB_LOOPX

		ADD		ESI, EBX
		ADD		EDI, EDX
		DEC		PasteH
		JNZ		PutImage_CK_SB_LOOPY
		EMMS
	}

	return;

PutImage_CK_SB_LOOP_SKIP:

	_asm {
		ADD		EDI, 4
		DEC		ECX
		JNZ		PutImage_CK_SB_LOOPX

		ADD		ESI, EBX
		ADD		EDI, EDX
		DEC		PasteH
		JNZ		PutImage_CK_SB_LOOPY
		EMMS
	}
}

// -------------------------------------

/**
 * @brief 带透明色的 Alpha 混合图像绘制
 *
 * 功能同 PutImageAB，但跳过颜色为 CK_VALUE（品红）的像素。
 * 这是最常用的精灵混合模式：透明区域 + 半透明效果。
 *
 * @param X     目标左上角横坐标
 * @param Y     目标左上角纵坐标
 * @param Image 图像数据指针
 * @param Alpha 透明度系数 (0~255)
 */
void PutImageCKAB(long X, long Y, LPDWORD Image, long Alpha)
{
	long	ImageW, ImageH;
	long	PasteW, PasteH;
	long	temp;

	if(Alpha <= 0)
	{
		return;
	}
	if(Alpha > 255)
	{
		PutImageCK(X, Y, Image);
		return;
	}

	if(X > XMAX || Y > YMAX)
		return;						//	超出范围

	ImageW = *(Image ++);
	ImageH = *(Image ++);

	if(X + ImageW <= XMIN || Y + ImageH <= YMIN)
		return;						//	超出范围

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

	for(temp = 0; temp < 4; temp ++)
	{
		ALPHA1[temp] = (WORD)Alpha;
		ALPHA2[temp] = (WORD)(256 - Alpha);
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

		MOVQ		MM3, [ALPHA1]		// MM3 = Alpha
		MOVQ		MM4, [ALPHA2]		// MM4 = 256 - Alpha
		SUB		EDX, PasteW			// [注：此行似乎为冗余代码]

PutImage_CK_AB_LOOPY:

		MOV		ECX, PasteW

PutImage_CK_AB_LOOPX:

		LODSD
		CMP		EAX, CK_VALUE
		JZ		PutImage_CK_AB_LOOP_SKIP


		MOVD		MM1, EAX			// MM1 = 源像素
		MOVD		MM2, [EDI]			// MM2 = 目标像素

		PXOR		MM0, MM0			// MM0 = 0

		PUNPCKLBW	MM1, MM0			// 解包
		PUNPCKLBW	MM2, MM0			// 解包

		PMULLW		MM1, MM3
		PMULLW		MM2, MM4

		PADDUSW		MM1, MM2
		PSRLW		MM1, 8

		PACKUSWB	MM1, MM0
		MOVD		EAX, MM1

		STOSD

		DEC		ECX
		JNZ		PutImage_CK_AB_LOOPX

		ADD		ESI, EBX
		ADD		EDI, EDX
		DEC		PasteH
		JNZ		PutImage_CK_AB_LOOPY
		EMMS
	}

	return;

PutImage_CK_AB_LOOP_SKIP:

	_asm {
		ADD		EDI, 4
		DEC		ECX
		JNZ		PutImage_CK_AB_LOOPX

		ADD		ESI, EBX
		ADD		EDI, EDX
		DEC		PasteH
		JNZ		PutImage_CK_AB_LOOPY
		EMMS
	}
}

// -------------------------------------
//			特效处理函数
// -------------------------------------

/**
 * @brief 清空特效计算缓冲区 (SFX1PAGE)
 *
 * 将第 4 页（SFX1PAGE）清零，用于水波、火焰等特效的初始状态。
 */
#define ClearSFXBuffer() ClearRenderBuffer(4)

// -------------------------------------
//	白噪声生成
// -------------------------------------

/**
 * @brief 生成白噪声画面
 *
 * 在渲染缓冲区生成随机灰度像素，形成电视雪花/噪声效果。
 * 使用 Brightness 查找表将随机数转换为灰度颜色。
 *
 * 生成后自动将噪声复制到所有 6 个页面，确保各页面内容一致。
 */
void WhiteNoise(void)
{
	long	i;
	
	// 生成随机灰度像素到第 0 页
	for(i = 0; i < RENDER_WIDTH * RENDER_HEIGHT / 4; i ++)
	{
		RenderBuffer[i] = Brightness[rand()&0xff];
	}
	// 将噪声复制到其他页面（Page 1~5）
	_asm {
		LEA		ESI, [RenderBuffer]
		LEA		EDI, [RenderBuffer + RENDER_WIDTH * RENDER_HEIGHT * 4 / 4]
		MOV		ECX, RENDER_WIDTH * RENDER_HEIGHT / 4
		REP		MOVSD
		LEA		ESI, [RenderBuffer]
		MOV		ECX, RENDER_WIDTH * RENDER_HEIGHT / 4
		REP		MOVSD
		LEA		ESI, [RenderBuffer]
		MOV		ECX, RENDER_WIDTH * RENDER_HEIGHT / 4
		REP		MOVSD
	}
}

// -------------------------------------
//	水波纹效果 (Ripple Effect)
// -------------------------------------

/**
 * @brief 水波纹缓冲区指针
 *
 * 使用 RenderBuffer 的第 8、9 页（超出正常 6 页范围）
 * 作为水波高度场的双缓冲。
 *   RippleBuffer1 - 当前帧高度场
 *   RippleBuffer2 - 下一帧高度场
 *
 * 每个元素是 short（16位有符号），表示该位置的水波高度/位移。
 */
short	*RippleBuffer1 = (short *)RenderBuffer + RENDER_HEIGHT * RENDER_WIDTH * 8,
		*RippleBuffer2 = (short *)RenderBuffer + RENDER_HEIGHT * RENDER_WIDTH * 9;

// -------------------------------------

/**
 * @brief 水波传播计算
 *
 * 使用二维波动方程模拟水波的传播和衰减：
 *   新高度 = (邻居平均高度 * 2 - 当前高度) * 衰减系数
 *
 * 邻居包括：左、右、上、下四个方向。
 * 衰减系数由 SAR AX, 7（除以 128）控制，值越大衰减越快。
 *
 * 每帧调用一次，交替交换 RippleBuffer1 和 RippleBuffer2。
 */
void RippleSpread(void)
{
	long	Width, Height, LineAdd;

	Width = XMAX - XMIN - 1;
	Height = YMAX - YMIN - 1;
	LineAdd = (RENDER_WIDTH - Width) * 2;	// 每行跳过的字节数（short = 2字节）

	_asm {
		MOV		ESI, RippleBuffer1
		MOV		EDI, RippleBuffer2

		MOV		RippleBuffer2, ESI		// 交换缓冲区指针
		MOV		RippleBuffer1, EDI

		MOV		EAX, YMIN
		MOV		EBX, XMIN
		INC		EAX						// 从 YMIN+1 开始（避开边界）
		INC		EBX						// 从 XMIN+1 开始

		MOV		EAX, [LineStartOffset + EAX * 4]
		SHR		EAX, 1					// 字节偏移 -> short 索引

		ADD		ESI, EAX
		ADD		EDI, EAX

		LEA		ESI, [ESI + EBX * 2]
		LEA		EDI, [EDI + EBX * 2]

		MOV		EDX, Height

RippleSpread_LoopY:

		MOV		ECX, Width

RippleSpread_LoopX:

		// 计算邻居平均高度
		MOV		AX, [ESI - 2]				// 左邻居 [x-1]
		MOV		BX, [ESI + 2]				// 右邻居 [x+1]
		ADD		AX, [ESI - RENDER_WIDTH * 2]	// 上邻居 [y-1]
		ADD		BX, [ESI + RENDER_WIDTH * 2]	// 下邻居 [y+1]
		ADD		AX, BX						// AX = 四个邻居之和
		SAR		AX, 1						// AX = 邻居平均 * 2
		ADD		ESI, 2						// 源指针前进
		SUB		AX, [EDI]					// 减去当前高度
		MOV		BX, AX
		SAR		BX, 7						// 计算衰减值（除以 128）
		SUB		AX, BX						// 应用衰减
		STOSW								// 存储新高度


		DEC		ECX
		JNZ		RippleSpread_LoopX

		ADD		ESI, LineAdd			// 跳到下一行
		ADD		EDI, LineAdd
		DEC		EDX
		JNZ		RippleSpread_LoopY
	}
}

// -------------------------------------
/*
void RenderRippleAsm(void)
{
	long	Width, Height;
	long	x, y;

	Width = XMAX - XMIN - 1;
	Height = YMAX - YMIN - 1;

	y= YMIN + 1;

	_asm {

RenderRippleFast_LoopY:

		MOV		ESI, RippleBuffer1
		MOV		EAX, y
		MOV		EBX, XMIN
		MOV		EAX, [LineStartOffset + EAX * 4]
		INC		EBX
		MOV		EDX, EAX
		MOV		x, EBX
		SHR		EAX, 1
		ADD		ESI, EAX
		LEA		ESI, [ESI + EBX * 2]				// lpRipple
		LEA		EDI, [EDX + EBX * 4 + RenderBuffer]	// lpDst

		MOV		ECX, Width

RenderRippleFast_LoopX:

		MOV		AX, [ESI - 2]				// 
		SUB		AX, [ESI + 2]				// ax = xoff

		MOV		BX, [ESI - RENDER_WIDTH * 2]	// 
		SUB		BX, [ESI + RENDER_WIDTH * 2]	// bx = yoff

		MOVSX	EAX, AX
		MOVSX	EBX, BX
		ADD		EAX, x
		ADD		EBX, y

		CMP		EAX, XMIN
		JL		RenderRippleFast_Skip
		CMP		EAX, XMAX
		JG		RenderRippleFast_Skip

		CMP		EBX, YMIN
		JL		RenderRippleFast_Skip
		CMP		EBX, YMAX
		JG		RenderRippleFast_Skip

		MOV		EBX, [LineStartOffset + EBX * 4]
		LEA		EBX, [EBX + EAX * 4 + RenderBuffer + RENDER_WIDTH * RENDER_HEIGHT * 4 * 3]
		MOV		EAX, [EBX]				// lpSrc

		STOSD
		INC		x

		ADD		ESI, 2							// lpRipple ++
		DEC		ECX
		JNZ		RenderRippleFast_LoopX
		
		INC		y
		DEC		Height
		JNZ		RenderRippleFast_LoopY
	}

	return;

	_asm {

RenderRippleFast_Skip:

		MOV		EAX, [EDI + RENDER_WIDTH * RENDER_HEIGHT * 4 * 3]

		STOSD
		INC		x

		ADD		ESI, 2							// lpRipple ++
		DEC		ECX
		JNZ		RenderRippleFast_LoopX
		
		INC		y
		DEC		Height
		JNZ		RenderRippleFast_LoopY
	}
}
*/

// -------------------------------------

/**
 * @brief 水波渲染
 *
 * 根据 RippleBuffer1 中的高度场，从 BACKPAGE（第3页）采样像素，
 * 模拟光线折射效果，将扭曲后的图像绘制到主渲染页。
 *
 * 折射计算：
 *   x偏移 = (左邻居高度 - 右邻居高度) / 8
 *   y偏移 = (上邻居高度 - 下邻居高度) / 8
 *   采样坐标 = (x + x偏移, y + y偏移)
 *
 * 如果折射后的坐标超出范围，则使用原始坐标采样。
 */
void RenderRipple(void)
{
	long	x, y, xoff, yoff;
	short	*lpRip;
	long	*lpDst;

	for(y=YMIN+1; y<YMAX; y++)
	{
		lpRip = RippleBuffer1 + y*RENDER_WIDTH + XMIN + 1;
		lpDst = (long *)RenderBuffer + MultipleWidth[y] + XMIN + 1;
		for(x=XMIN+1; x<XMAX; x++)
		{
			// 计算 X 方向折射偏移（基于水平梯度）
			xoff = (*(lpRip-1)-*(lpRip+1))>>3;
			// 计算 Y 方向折射偏移（基于垂直梯度）
			yoff = (*(lpRip-RENDER_WIDTH)-*(lpRip+RENDER_WIDTH))>>3;
			// 根据偏移采样背景像素
			if(y+yoff<=YMAX && y+yoff>=YMIN && x+xoff<=XMAX && x+xoff>=XMIN)
			{
				*lpDst=*((long *)RenderBuffer + RENDER_HEIGHT * RENDER_WIDTH * 3 + MultipleWidth[y + yoff] + x + xoff);
			}
			else
			{
				*lpDst=*((long *)RenderBuffer + RENDER_HEIGHT * RENDER_WIDTH * 3 + MultipleWidth[y] + x);
			}
			lpRip ++;
			lpDst++;
		}
	}
}

// -------------------------------------

/**
 * @brief 添加水波涟漪
 *
 * 在指定位置创建一个圆形的水波扰动。
 *
 * @param x      涟漪中心 X 坐标
 * @param y      涟漪中心 Y 坐标
 * @param size   涟漪半径（像素）
 * @param weight 涟漪强度/高度值
 */
void AddRipple(int x, int y, int size, int weight)
{
	// 边界检查：确保涟漪不会超出裁剪框
	if ((x+size)>=XMAX || (y+size)>=YMAX || 
		(x-size)<XMIN || (y-size)<YMIN) return;

	// 在圆形区域内设置高度值
	for (int posx=x-size; posx<x+size; posx++)
	for (int posy=y-size; posy<y+size; posy++)
		if ((posx-x)*(posx-x) + (posy-y)*(posy-y) < size*size)
			RippleBuffer1[MultipleWidth[posy]+posx] = weight;
}
