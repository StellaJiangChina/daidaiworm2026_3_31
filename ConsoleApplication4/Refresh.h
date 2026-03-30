/*  Refresh.h  */
//
// 屏幕刷新头文件
//
// 提供将渲染缓冲区内容输出到 DirectDraw 表面的功能，包括：
//   - 帧率计算和状态显示
//   - 运动模糊效果处理
//   - 不同显示模式的像素格式转换
//   - 鼠标光标绘制
//   - 页面翻转（Page Flip）
//
// 显示模式说明：
//   Mode 0: 32位色，2倍缩放（800×600）- 双线性过滤效果
//   Mode 1: 16位色，2倍缩放（800×600）- 双线性过滤效果
//   Mode 2: 32位色，1倍原始（400×300）
//   Mode 3: 16位色，1倍原始（400×300）
//

/*--------------------------------------*/
//			状态显示
/*--------------------------------------*/

long	FrameRate;              // 当前帧率（乘以100的整数，如 6000 = 60.00 FPS）
long	FrameCount,	FrameCount0;    // 帧计数器（当前/上次统计）
long	FrameTime, FrameTime0, StatusTime;  // 时间戳

// -------------------------------------

/**
 * @brief 显示状态信息（FPS、显示模式、运动模糊状态）
 *
 * 计算并显示当前帧率、屏幕模式、运动模糊状态。
 * 状态信息显示在屏幕右上角，可通过 F12 键切换显示/隐藏。
 */
void ShowStatus(void)
{
	double	nFPS;
	char	NumBuffer[128];

	// 帧率计算（每 500ms 更新一次）
	FrameCount ++;
	FrameTime = timeGetTime();
	if((FrameTime - FrameTime0) >= 500)
	{
		FrameRate = ((FrameCount - FrameCount0) * 100000)/(FrameTime - FrameTime0);
		FrameCount0 = FrameCount;
		FrameTime0 = FrameTime;
	}

	// 如果状态显示开启
	if(StatusFlag)
	{
		// 显示屏幕模式
		SetColor(0x00ff00);
		_Box(RENDER_WIDTH - 112, 3, RENDER_WIDTH - 4, 9);
		SetColor(0);
		switch(ScreenMode)
		{
			case 0:
				_OutTextXY(RENDER_WIDTH - 109, 3, (char*)(const char*)"TRUECOLOR TV MODE");
				break;
			case 1:
				_OutTextXY(RENDER_WIDTH - 109, 3, (char*)(const char*)"HIGHCOLOR TV MODE");
				break;
			case 2:
				_OutTextXY(RENDER_WIDTH - 109, 3, (char*)(const char*)"TRUECOLOR PC MODE");
				break;
			case 3:
				_OutTextXY(RENDER_WIDTH - 109, 3, (char*)(const char*)"HIGHCOLOR PC MODE");
				break;
		}

		// 显示帧率
		nFPS=(double)FrameRate/100.0;
		sprintf_s(NumBuffer, "%.2f FPS", nFPS);
		SetColor(0xffe08f);
		_Box(RENDER_WIDTH - 112, 11, RENDER_WIDTH - 4, 17);
		SetColor(0);
		_OutTextXY(RENDER_WIDTH - 112 + (108 - StrLenEx(NumBuffer) * 6) / 2, 11, NumBuffer);

		// 显示运动模糊状态
		switch(MotionBlurFlag)
		{
			case 1:
				SetColor(0xffc06f);
				_Box(RENDER_WIDTH - 112, 19, RENDER_WIDTH - 4, 25);
				SetColor(0);
				_OutTextXY(RENDER_WIDTH - 109, 19, (char*)(const char*)"LIGHT MOTION BLUR");
				break;
			case 2:
				SetColor(0xffc04f);
				_Box(RENDER_WIDTH - 112, 19, RENDER_WIDTH - 4, 25);
				SetColor(0);
				_OutTextXY(RENDER_WIDTH - 109, 19, (char*)(const char*)"HEAVY MOTION BLUR");
				break;
		}
	}
}				  

/*--------------------------------------*/
//			运动模糊处理
/*--------------------------------------*/

// MMX 位掩码表，用于运动模糊的颜色通道分离
BYTE	MotionBlurBIT_MASK[16] = 
{
	0xFE, 0xFE, 0xFE, 0x00, 0xFE, 0xFE, 0xFE, 0x00,   // 轻度模糊掩码
	0xFC, 0xFC, 0xFC, 0x00, 0xFC, 0xFC, 0xFC, 0x00    // 重度模糊掩码
};

/**
 * @brief 应用运动模糊效果到渲染缓冲区
 *
 * 将当前帧（主渲染页）与前一帧（运动模糊页）进行混合，
 * 产生运动拖影效果。使用 MMX 指令加速计算。
 *
 * 混合公式：
 *   轻度模糊 (Flag=1): Result = (Current × 3 + Previous) / 4
 *   重度模糊 (Flag=2): Result = (Current + Previous) / 2
 */
void MotionBlurRenderBuffer(void)
{

	if(MotionBlurFlag==1)
	{
		// 轻度运动模糊：当前帧权重 75%，前一帧权重 25%
		_asm {
			LEA		ESI, [RenderBuffer]
			MOV		EDI, ESI
			MOVQ	MM7, [MotionBlurBIT_MASK+8]    // 加载掩码 0xFCFCFC
			ADD		EDI, RENDER_HEIGHT * RENDER_WIDTH * 4 * 2  // 指向运动模糊页
			MOV		ECX, RENDER_HEIGHT * RENDER_WIDTH / 4  // 每次处理 4 像素
			MOVQ	MM6, MM7
			MOV		EBX, 16

PixelMotionBlur:

			MOVQ	MM0, [ESI]          // 加载当前帧 2 像素
			MOVQ	MM1, [ESI+8]        // 加载当前帧另 2 像素
			
			MOVQ	MM2, [EDI]          // 加载前一帧 2 像素
			MOVQ	MM3, [EDI+8]        // 加载前一帧另 2 像素

			PAND	MM0, MM6            // 清除低位（准备乘法）
			PAND	MM1, MM7

			PAND	MM2, MM6
			PAND	MM3, MM7

			MOVQ	MM4, MM0            // 复制当前帧
			MOVQ	MM5, MM1

			PADDD	MM0, MM4            // Current × 2
			PADDD	MM1, MM5
			
			PADDD	MM0, MM4            // Current × 3
			PADDD	MM1, MM5

			PADDD	MM0, MM2            // + Previous
			PADDD	MM1, MM3

			PSRLD	MM0, 2              // / 4
			PSRLD	MM1, 2

			MOVQ	[EDI], MM0          // 存回运动模糊页
			ADD		ESI, EBX
			MOVQ	[EDI+8], MM1
			ADD		EDI, EBX

			DEC		ECX
			JNZ		PixelMotionBlur

			EMMS                        // 清空 MMX 状态
		}
	}
	else
	{
		// 重度运动模糊：当前帧和前一帧各 50%
		_asm {
			LEA		ESI, [RenderBuffer]
			MOV		EDI, ESI
			MOVQ	MM7, [MotionBlurBIT_MASK]      // 加载掩码 0xFEFEFE
			ADD		EDI, RENDER_HEIGHT * RENDER_WIDTH * 4 * 2
			MOV		ECX, RENDER_HEIGHT * RENDER_WIDTH / 4
			MOVQ	MM6, MM7
			MOV		EBX, 16

PixelMotionBlur2:

			MOVQ	MM0, [ESI]
			MOVQ	MM1, [ESI+8]
			
			MOVQ	MM2, [EDI]
			MOVQ	MM3, [EDI+8]

			PAND	MM0, MM6
			PAND	MM1, MM7

			PAND	MM2, MM6
			PAND	MM3, MM7
			
			PADDD	MM0, MM2            // Current + Previous
			PADDD	MM1, MM3

			PSRLD	MM0, 1              // / 2
			PSRLD	MM1, 1

			MOVQ	[EDI], MM0
			ADD		ESI, EBX
			MOVQ	[EDI+8], MM1
			ADD		EDI, EBX

			DEC		ECX
			JNZ		PixelMotionBlur2

			EMMS
		}
	}
}

// -------------------------------------

/**
 * @brief 拷贝屏幕数据到 DirectDraw 表面（Mode 0: 32位 2倍缩放）
 *
 * 将 400×300 的渲染缓冲区放大到 800×600，同时进行简单的线性过滤
 * （每两个像素取平均）以平滑图像。
 *
 * @param lpDst   目标表面指针
 * @param lPitch  目标表面行跨度（字节）
 */
void CopyScreenData0(LPDWORD lpDst, long lPitch)
{
	long		lPitchOffset = lPitch + lPitch - 2 * RENDER_WIDTH * 4 + 4;
	// 2 行 - 2 × 缓冲区宽度 + 1

	_asm {
		LEA		ESI, [RenderBuffer]
		CMP		MotionBlurFlag, 0
		JZ		MotionBlurFlagIsFalse
		ADD		ESI, RENDER_HEIGHT * RENDER_WIDTH * 4 * 2  // 使用运动模糊页

MotionBlurFlagIsFalse:

		MOV		EDI, lpDst
		MOV		EDX, RENDER_HEIGHT
		CLD

CopyScreenData0_LoopY:

		MOV		ECX, RENDER_WIDTH - 1

CopyScreenData0_LoopX:

		LODSD                       // 加载源像素
		STOSD                       // 直接写入第一个像素
		MOV		EBX, [ESI]          // 预取下一个像素
		AND		EAX, 0x00FEFEFE     // 清除最低位（准备平均）
		AND		EBX, 0x00FEFEFE
		ADD		EAX, EBX            // 两像素相加
		SHR		EAX, 1              // 除以 2（平均值）
		STOSD                       // 写入平均后的第二个像素

		DEC		ECX
		JNZ		CopyScreenData0_LoopX

		MOVSD                       // 每行最后一个像素

		ADD		EDI, lPitchOffset   // 跳到下一行的起始位置

		DEC		EDX
		JNZ		CopyScreenData0_LoopY
	}
}

// -------------------------------------

/**
 * @brief 拷贝屏幕数据到 DirectDraw 表面（Mode 1: 16位 555格式 2倍缩放）
 *
 * 将 32位渲染缓冲区转换为 16位 5:5:5 RGB 格式，
 * 同时进行 2 倍缩放和简单的线性过滤。
 *
 * @param lpDst   目标表面指针
 * @param lPitch  目标表面行跨度（字节）
 */
void CopyScreenData1_555(LPDWORD lpDst, long lPitch)
{
	long		lPitchOffset = lPitch + lPitch - RENDER_WIDTH * 2 * 2 + 2;
	long		Y = RENDER_HEIGHT;

	_asm {
		LEA		ESI, [RenderBuffer]
		CMP		MotionBlurFlag, 0
		JZ		MotionBlurFlagIsFalse
		ADD		ESI, RENDER_HEIGHT * RENDER_WIDTH * 4 * 2

MotionBlurFlagIsFalse:

		MOV		EDI, lpDst
		MOV		Y, RENDER_HEIGHT
		CLD

CopyScreenData1_555_LoopY:

		MOV		ECX, RENDER_WIDTH - 1

CopyScreenData1_555_LoopX:

		LODSD
		MOV		EBX, EAX
		MOV		EDX, EAX
		AND		EAX, 0xFFFF
		SHR		EDX, 16
		MOV		AX, [RGBPACKWORDL+EAX*2]    // 查表获取低 16 位颜色
		ADD		AX, [RGBPACKWORDH+EDX*2]    // 加上高 16 位颜色
		STOSW

		MOV		EAX, [ESI]
		AND		EBX, 0xFEFEFE
		AND		EAX, 0xFEFEFE
		ADD		EAX, EBX
		SHR		EAX, 1
		MOV		EDX, EAX
		AND		EAX, 0xFFFF
		SHR		EDX, 16
		MOV		AX, [RGBPACKWORDL+EAX*2]
		ADD		AX, [RGBPACKWORDH+EDX*2]
		STOSW

		DEC		ECX
		JNZ		CopyScreenData1_555_LoopX

		LODSD
		MOV		EDX, EAX
		AND		EAX, 0xFFFF
		SHR		EDX, 16
		MOV		AX, [RGBPACKWORDL+EAX*2]
		ADD		AX, [RGBPACKWORDH+EDX*2]
		STOSW

		ADD		EDI, lPitchOffset

		DEC		Y
		JNZ		CopyScreenData1_555_LoopY
	}
}

// -------------------------------------

/**
 * @brief 拷贝屏幕数据到 DirectDraw 表面（Mode 1: 16位 565格式 2倍缩放）
 *
 * 将 32位渲染缓冲区转换为 16位 5:6:5 RGB 格式，
 * 同时进行 2 倍缩放和简单的线性过滤。
 *
 * @param lpDst   目标表面指针
 * @param lPitch  目标表面行跨度（字节）
 */
void CopyScreenData1_565(LPDWORD lpDst, long lPitch)
{
	long		lPitchOffset = lPitch + lPitch - RENDER_WIDTH * 2 * 2 + 2;
	long		Y = RENDER_HEIGHT;

	_asm {
		LEA		ESI, [RenderBuffer]
		CMP		MotionBlurFlag, 0
		JZ		MotionBlurFlagIsFalse
		ADD		ESI, RENDER_HEIGHT * RENDER_WIDTH * 4 * 2

MotionBlurFlagIsFalse:

		MOV		EDI, lpDst
		CLD

CopyScreenData1_565_LoopY:

		MOV		ECX, RENDER_WIDTH - 1

CopyScreenData1_565_LoopX:

		LODSD
		MOV		EBX, EAX
		MOV		EDX, EAX
		AND		EAX, 0xFFFF
		SHR		EDX, 16
		MOV		AX, [RGBPACKWORDL+EAX*2]
		ADD		AX, [RGBPACKWORDH+EDX*2]
		STOSW

		MOV		EAX, [ESI]
		AND		EBX, 0xFEFEFE
		AND		EAX, 0xFEFEFE
		ADD		EAX, EBX
		SHR		EAX, 1
		MOV		EDX, EAX
		AND		EAX, 0xFFFF
		SHR		EDX, 16
		MOV		AX, [RGBPACKWORDL+EAX*2]
		ADD		AX, [RGBPACKWORDH+EDX*2]
		STOSW

		DEC		ECX
		JNZ		CopyScreenData1_565_LoopX

		LODSD
		MOV		EDX, EAX
		AND		EAX, 0xFFFF
		SHR		EDX, 16
		MOV		AX, [RGBPACKWORDL+EAX*2]
		ADD		AX, [RGBPACKWORDH+EDX*2]
		STOSW

		ADD		EDI, lPitchOffset

		DEC		Y
		JNZ		CopyScreenData1_565_LoopY
	}
}

// -------------------------------------

/**
 * @brief 拷贝屏幕数据到 DirectDraw 表面（Mode 2: 32位 1倍原始）
 *
 * 将 400×300 的渲染缓冲区以 1:1 比例拷贝到 400×300 的屏幕，
 * 使用 MMX 指令加速。
 *
 * @param lpDst   目标表面指针
 * @param lPitch  目标表面行跨度（字节）
 */
void CopyScreenData2(LPDWORD lpDst, long lPitch)
{
	long		lPitchOffset = lPitch - RENDER_WIDTH * 4;
	// 1 行 - 缓冲区宽度

	_asm {
		LEA		ESI, [RenderBuffer]
		CMP		MotionBlurFlag, 0
		JZ		MotionBlurFlagIsFalse
		ADD		ESI, RENDER_HEIGHT * RENDER_WIDTH * 4 * 2

MotionBlurFlagIsFalse:

		MOV		EDI, lpDst
		MOV		EDX, RENDER_HEIGHT
		CLD
		MOV		EAX, lPitchOffset
		MOV		EBX, 16

CopyScreenData2_LoopY:

		MOV		ECX, RENDER_WIDTH / 4   // 每次处理 4 像素

CopyScreenData2_LoopX:

		MOVQ	MM0, [ESI]              // MMX 一次移动 8 字节（2 像素）
		MOVQ	MM1, [ESI+8]            // 再移动 2 像素
		MOVQ	[EDI], MM0
		ADD		ESI, EBX
		MOVQ	[EDI+8], MM1
		ADD		EDI, EBX

		DEC		ECX
		JNZ		CopyScreenData2_LoopX

		ADD		EDI, EAX                // 跳到下一行

		DEC		EDX
		JNZ		CopyScreenData2_LoopY

		EMMS
	}
}

// -------------------------------------

/**
 * @brief 拷贝屏幕数据到 DirectDraw 表面（Mode 3: 16位 555格式 1倍）
 *
 * 将 32位渲染缓冲区转换为 16位 5:5:5 RGB 格式，
 * 以 1:1 比例拷贝到 400×300 的屏幕。
 *
 * @param lpDst   目标表面指针
 * @param lPitch  目标表面行跨度（字节）
 */
void CopyScreenData3_555(LPDWORD lpDst, long lPitch)
{
	long		lPitchOffset = lPitch - RENDER_WIDTH * 2;

	_asm {
		LEA		ESI, [RenderBuffer]
		CMP		MotionBlurFlag, 0
		JZ		MotionBlurFlagIsFalse
		ADD		ESI, RENDER_HEIGHT * RENDER_WIDTH * 4 * 2

MotionBlurFlagIsFalse:

		MOV		EDI, lpDst
		MOV		EDX, RENDER_HEIGHT
		CLD

CopyScreenData3_555_LoopY:

		MOV		ECX, RENDER_WIDTH

CopyScreenData3_555_LoopX:

		LODSD
		MOV		EBX, EAX
		AND		EAX, 0xFFFF
		SHR		EBX, 16
		MOV		AX, [RGBPACKWORDL+EAX*2]
		ADD		AX, [RGBPACKWORDH+EBX*2]
		STOSW

		DEC		ECX
		JNZ		CopyScreenData3_555_LoopX

		ADD		EDI, lPitchOffset

		DEC		EDX
		JNZ		CopyScreenData3_555_LoopY
	}
}

// -------------------------------------

/**
 * @brief 拷贝屏幕数据到 DirectDraw 表面（Mode 3: 16位 565格式 1倍）
 *
 * 将 32位渲染缓冲区转换为 16位 5:6:5 RGB 格式，
 * 以 1:1 比例拷贝到 400×300 的屏幕。
 *
 * @param lpDst   目标表面指针
 * @param lPitch  目标表面行跨度（字节）
 */
void CopyScreenData3_565(LPDWORD lpDst, long lPitch)
{
	long		lPitchOffset = lPitch - RENDER_WIDTH * 2;

	_asm {
		LEA		ESI, [RenderBuffer]
		CMP		MotionBlurFlag, 0
		JZ		MotionBlurFlagIsFalse
		ADD		ESI, RENDER_HEIGHT * RENDER_WIDTH * 4 * 2

MotionBlurFlagIsFalse:

		MOV		EDI, lpDst
		MOV		EDX, RENDER_HEIGHT
		CLD

CopyScreenData3_565_LoopY:

		MOV		ECX, RENDER_WIDTH

CopyScreenData3_565_LoopX:

		LODSD
		MOV		EBX, EAX
		AND		EAX, 0xFFFF
		SHR		EBX, 16
		MOV		AX, [RGBPACKWORDL+EAX*2]
		ADD		AX, [RGBPACKWORDH+EBX*2]
		STOSW

		DEC		ECX
		JNZ		CopyScreenData3_565_LoopX

		ADD		EDI, lPitchOffset

		DEC		EDX
		JNZ		CopyScreenData3_565_LoopY
	}
}

// -------------------------------------
//	图像缓冲区（用于状态栏背景保存）
// -------------------------------------

DWORD		StatusImageBuffer[110 * 23 + 2];    // 状态栏背景图像备份

// -------------------------------------
//	刷新屏幕
// -------------------------------------

/**
 * @brief 刷新屏幕（主渲染函数）
 *
 * 每帧调用此函数将渲染缓冲区内容输出到屏幕：
 *   1. 应用运动模糊效果（如果开启）
 *   2. 备份状态栏区域背景
 *   3. 绘制鼠标光标
 *   4. 显示状态信息
 *   5. 锁定后备缓冲区并拷贝像素数据
 *   6. 页面翻转（Flip）
 *   7. 恢复鼠标背景
 *   8. 恢复状态栏背景（如果显示时间超过 5 秒则隐藏）
 *
 * 支持 4 种显示模式，自动处理表面丢失恢复。
 */
void RefreshScreen(void)
{
	DDSURFACEDESC		ddsd;
	HRESULT				ddrval;
	long				AtX, AtY;
	long				xmin, xmax, ymin, ymax;

	// 保存当前裁剪框
	xmin = XMIN; xmax = XMAX; ymin = YMIN; ymax = YMAX;

	ResetClipBox();

	// 应用运动模糊效果
	if(MotionBlurFlag)
	{
		MotionBlurRenderBuffer();
		SetRenderPage(BLURPAGE);    // 后续绘制从运动模糊页读取
	}

	// 备份状态栏区域背景
	if(StatusFlag)
	{
		GetImage(RENDER_WIDTH - 112, 3, 110, 23, StatusImageBuffer);
	}

	// 备份鼠标位置背景并绘制鼠标
	if(MOUSEFLAG && MouseImageBuffer)
	{
		long	Width, Height;

		Width = *(MouseImageBuffer);
		Height = *(MouseImageBuffer + 1);

		AtX = MOUSEX + MOUSEATTACHX;
		AtY = MOUSEY + MOUSEATTACHY;

		GetImage(AtX, AtY, Width, Height, MouseImageBackBuffer);
		PutImageCK(AtX, AtY, MouseImageBuffer);
	}

	// 显示状态信息
	ShowStatus();

	// 锁定 DirectDraw 后备缓冲区
	ddsd.dwSize = sizeof(ddsd);
	while ((ddrval = lpDDSBack->Lock(NULL, &ddsd, 0, NULL)) == DDERR_WASSTILLDRAWING);
	if(ddrval == DD_OK)
	{
		// 根据显示模式选择对应的拷贝函数
		switch(ScreenMode)
		{
			case 0:
				CopyScreenData0((LPDWORD)ddsd.lpSurface, ddsd.lPitch);
				break;
			case 1:
				if(HighColor555Flag)
					CopyScreenData1_555((LPDWORD)ddsd.lpSurface, ddsd.lPitch);
				else
					CopyScreenData1_565((LPDWORD)ddsd.lpSurface, ddsd.lPitch);
				break;
			case 2:
				CopyScreenData2((LPDWORD)ddsd.lpSurface, ddsd.lPitch);
				break;
			case 3:
				if(HighColor555Flag)
					CopyScreenData3_555((LPDWORD)ddsd.lpSurface, ddsd.lPitch);
				else
					CopyScreenData3_565((LPDWORD)ddsd.lpSurface, ddsd.lPitch);
				break;
		}
		lpDDSBack->Unlock(NULL);
	}

	// 页面翻转（将后备缓冲区内容显示到屏幕）
	while(TRUE)
	{
		ddrval = lpDDSPrimary->Flip(NULL, 0);
		if(ddrval == DD_OK)
		{	
			break;
		}
		if(ddrval == DDERR_SURFACELOST)
		{
			// 表面丢失，尝试恢复
			ddrval = RestoreDDraw();
			if(ddrval != DD_OK)
			{
				break;
			}
		}
		if(ddrval != DDERR_WASSTILLDRAWING)
		{
			break;
		}
	}

	// 恢复鼠标背景
	if(MOUSEFLAG && MouseImageBuffer)
	{
		PutImage(AtX, AtY, MouseImageBackBuffer);
	}

	// 恢复状态栏背景或检查是否超时关闭
	if(StatusFlag)
	{
		if((FrameTime - StatusTime) > 5000) StatusFlag = FALSE;  // 5 秒后自动隐藏
		PutImage(RENDER_WIDTH - 112, 3, StatusImageBuffer);
	}

	// 恢复渲染页和裁剪框
	SetRenderPage(MAINPAGE);

	XMIN = xmin; XMAX = xmax; YMIN = ymin; YMAX = ymax;
}
