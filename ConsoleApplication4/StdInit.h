/*  StdInit.H  */
//
// 游戏初始化与主程序框架头文件
//
// 提供 DirectDraw/DirectInput/DirectSound 初始化、窗口管理、
// 消息循环、MMX 检测等功能。这是整个游戏引擎的入口模块。
//
// 主要功能：
//   - 窗口创建与消息处理（WindowProc）
//   - DirectDraw 初始化（全屏独占模式，支持多种分辨率）
//   - MMX 指令集检测（运行前检查 CPU 支持）
//   - 游戏主循环（WinMain → GameLoop）
//   - 热键处理（F10 切换分辨率，F11 运动模糊，F12 状态显示，ESC 退出）
//

// 游戏标题和引擎版权信息
#define TITLE "\"DAIDAI\" WORM v1.0.04"
#define NAME  "EOLITH GAME ENGINE    COPYRIGHT(C)FAN YIPENG, 1999"

// -------------------------------------
// 系统头文件包含
// -------------------------------------

#include <windows.h>    // Windows API 核心
#include <mmsystem.h>   // 多媒体定时器（timeGetTime）

#include <ddraw.h>      // DirectDraw 图形接口

#include <stdio.h>      // 标准输入输出
#include <stdlib.h>     // 标准库函数

// -------------------------------------
// 宏定义
// -------------------------------------

/**
 * @brief 安全释放 COM 对象宏
 *
 * 检查指针非空后调用 Release()，然后将指针设为 NULL。
 * 用于安全释放 DirectX 接口对象，避免重复释放或空指针访问。
 */
#define RELEASE(x) if (x != NULL) { x -> Release(); x = NULL; }

// -------------------------------------
// 结构体对齐设置
// -------------------------------------

#pragma pack (16)       // 按 16 字节边界对齐结构体

// -------------------------------------
// 函数前置声明
// -------------------------------------

/**
 * @brief 初始化失败处理函数
 *
 * 显示错误消息框，释放所有已分配资源，销毁窗口并退出程序。
 *
 * @param msg 要显示的错误消息字符串
 */
void InitFail(LPSTR msg);

// -------------------------------------
// 全局变量
// -------------------------------------

HWND		hWndCopy;       // 主窗口句柄副本（供其他模块使用）
HINSTANCE	hInstanceCopy;  // 应用程序实例句柄副本

BOOL		bActive;        // 应用程序是否处于活动状态（前台窗口）
BOOL		bLive = TRUE;   // 游戏主循环运行标志，设为 FALSE 时退出

// 包含其他模块头文件
#include "StdMath.H"    // 数学函数库
#include "StdStr.H"     // 字符串处理库

#include "StdGraph.H"   // 图形渲染库
#include "StdFont.H"    // 字体渲染库
#include "StdPoly.H"    // 多边形填充库
#include "StdPolyT.H"   // 纹理映射库

#include "SpecialE.H"   // 特效处理库（水波、火焰等）

#include "StdInput.H"   // 输入处理库
#include "StdSound.H"   // 声音播放库
#include "StdVideo.H"   // 视频播放库

// -------------------------------------
// 游戏生命周期回调函数（由用户实现）
// -------------------------------------

/**
 * @brief 游戏初始化回调
 *
 * 在 DirectX 初始化完成后调用，用于加载资源、初始化游戏状态。
 */
void InitGame(void);

/**
 * @brief 游戏主循环回调
 *
 * 每帧调用一次，处理游戏逻辑、更新状态、渲染画面。
 */
void GameLoop(void);

/**
 * @brief 游戏退出回调
 *
 * 在主循环结束后调用，用于释放游戏资源、保存数据。
 */
void ExitGame(void);

// -------------------------------------
// MMX 检测
// -------------------------------------

BOOL		MMXFOUND = FALSE;		// MMX 指令集存在标志

/*--------------------------------------*/
//			检测 MMX CPU 支持
/*--------------------------------------*/

/**
 * @brief CPUID 指令内联汇编宏
 *
 * CPUID 指令用于查询 CPU 信息和功能。
 * 0x0F 0xA2 是 CPUID 的操作码。
 */
#define CPUID	__asm _emit 0x0F __asm _emit 0xA2

/**
 * @brief 检测 CPU 是否支持 MMX 指令集
 *
 * 使用 CPUID 指令查询 CPU 功能标志位，检测是否支持 MMX。
 * 如果检测到 MMX 支持，设置 MMXFOUND = TRUE。
 *
 * 检测流程：
 *   1. 检查 CPUID 指令是否可用（通过 EFLAGS 第 21 位）
 *   2. 执行 CPUID EAX=1 获取功能信息
 *   3. 检查 EDX 第 23 位（MMX 标志位）
 */
void CheckMMX(void)
{
		
	_asm {
		PUSHFD
		POP		EAX
		XOR		EAX, 0x00200000
		PUSH	EAX
		POPFD
		PUSHFD
		POP		EDX
		CMP		EAX, EDX
		JNE		MMXNotFound

		XOR		EAX, EAX
		CPUID
		OR		EAX, EAX
		JZ		MMXNotFound

		MOV		EAX, 1
		CPUID
		TEST	EDX, 00800000H
		JZ		MMXNotFound
	}
	MMXFOUND = TRUE;
	return;

MMXNotFound:
	MMXFOUND = FALSE;
	return;
}

/*--------------------------------------*/
//			DirectDraw 初始化
/*--------------------------------------*/

// -------------------------------------

/**
 * @brief 释放所有 DirectDraw 对象
 *
 * 按相反顺序释放后备缓冲区、主表面和 DirectDraw 对象。
 * 使用 RELEASE 宏确保安全释放。
 */
void ReleaseDDraw(void)
{
	RELEASE(lpDDSBack);
	RELEASE(lpDDSPrimary);
	RELEASE(lpDD);
}

// -------------------------------------

// 屏幕参数
int			ScreenW,	ScreenH;        // 实际屏幕分辨率
int			BitDepth,	ScreenMode = -1; // 颜色深度（16/32位），当前屏幕模式

// 高颜色格式标志
BOOL		HighColor555Flag;   // TRUE = 5:5:5 (RGB 各5位)，FALSE = 5:6:5 (绿6位)

// -------------------------------------

// RGB 颜色打包查找表（用于 16 位颜色模式）
WORD		RGBPACKWORDL[0x10000];  // 低字节查找表（绿+蓝）
WORD		RGBPACKWORDH[0x10000];  // 高字节查找表（红）

/**
 * @brief 初始化 RGB 颜色打包查找表
 *
 * 预计算 32位 XRGB 颜色到 16 位 RGB 的转换表，加速渲染时的颜色格式转换。
 * 根据 HighColor555Flag 选择 555 或 565 格式。
 *
 * 555 格式：R5 G5 B5（每位5位，共15位）
 * 565 格式：R5 G6 B5（绿多1位，共16位）
 */
void InitRGBPACKWORD(void)
{
	long	i;
	long	r, g, b;
		
	if(HighColor555Flag)
	{
		for(i=0; i<0x10000; i++)
		{
			r = (i & 0xF8) << 7;
			RGBPACKWORDH[i] = (WORD)r;
		}
		for(i=0; i<0x10000; i++)
		{
			g = (i & 0xF800) >> 6;
			b = (i & 0x00F8) >> 3;
			RGBPACKWORDL[i] = (WORD)(g + b);
		}
	}
	else
	{
		for(i=0; i<0x10000; i++)
		{
			r = (i & 0xF8) << 8;
			RGBPACKWORDH[i] = (WORD)r;
		}
		for(i=0; i<0x10000; i++)
		{
			g = (i & 0xFC00) >> 5;
			b = (i & 0x00F8) >> 3;
			RGBPACKWORDL[i] = (WORD)(g + b);
		}
	}
}

// -------------------------------------

/**
 * @brief 启动指定模式的 DirectDraw
 *
 * 初始化 DirectDraw，创建主表面和后备缓冲区，设置显示模式。
 *
 * 支持的显示模式：
 *   Mode 0: 800×600，32位色（2倍缩放）
 *   Mode 1: 800×600，16位色（2倍缩放）
 *   Mode 2: 400×300，32位色（1倍原始）
 *   Mode 3: 400×300，16位色（1倍原始）
 *
 * @param Mode 显示模式编号（0~3）
 * @return TRUE 成功，FALSE 失败
 */
BOOL StartDDraw(int Mode)
{
    DDSURFACEDESC	ddsd;       // 表面描述结构
    DDSCAPS			ddscaps;    // 表面能力结构
 	DDPIXELFORMAT	ddpf;       // 像素格式结构
	HRESULT			ddrval;     // DirectDraw 返回值

	// 根据模式设置屏幕参数
	switch(Mode)
	{
		case 0:		// 2 x 32
			ScreenW = RENDER_WIDTH * 2;
			ScreenH = RENDER_HEIGHT * 2;
			BitDepth = 32;
			break;
		case 1:		// 2 x 16
			ScreenW = RENDER_WIDTH * 2;
			ScreenH = RENDER_HEIGHT * 2;
			BitDepth = 16;
			break;
		case 2:		// 1 x 32
			ScreenW = RENDER_WIDTH;
			ScreenH = RENDER_HEIGHT;
			BitDepth = 32;
			break;
		case 3:		// 1 x 16
			ScreenW = RENDER_WIDTH;
			ScreenH = RENDER_HEIGHT;
			BitDepth = 16;
			break;
		default:
			return FALSE;
	}

	// 重新初始化前释放旧对象
	ReleaseDDraw();

    // 创建 DirectDraw 主对象
    ddrval = DirectDrawCreate(NULL, &lpDD, NULL);
    if(ddrval != DD_OK)
    {
		return FALSE;
    }

    // 设置独占模式（全屏）
    ddrval = lpDD->SetCooperativeLevel(hWndCopy, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN);

    if(ddrval != DD_OK)
    {
		return FALSE;
    }

    // 设置显示模式
    ddrval = lpDD->SetDisplayMode(ScreenW, ScreenH, BitDepth);
    if(ddrval != DD_OK)
    {
		return FALSE;
    }

    // 创建主表面（带1个后备缓冲区，支持页面翻转）
    ddsd.dwSize = sizeof(ddsd);
    ddsd.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
    ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE |
		DDSCAPS_FLIP |
		DDSCAPS_COMPLEX;
    ddsd.dwBackBufferCount = 1;

    ddrval = lpDD->CreateSurface(&ddsd, &lpDDSPrimary, NULL);

    // 创建主表面失败
    if(ddrval != DD_OK)
    {
		return FALSE;
    }

	// 获取后备缓冲区指针
    ddscaps.dwCaps = DDSCAPS_BACKBUFFER;
    ddrval = lpDDSPrimary->GetAttachedSurface(&ddscaps, &lpDDSBack);
    if(ddrval != DD_OK)
    {
		return FALSE;
    }

	// 16位模式：检测像素格式并初始化颜色表
	if(BitDepth == 16)
	{
		// 获取像素格式
		ddpf.dwSize = sizeof(ddpf);
		ddrval = lpDDSPrimary->GetPixelFormat(&ddpf);
	    if(ddrval != DD_OK)
		{
			return FALSE;
	    }

		WORD	GBitMask = (WORD)ddpf.dwGBitMask;
	
		// 根据绿色位掩码判断格式
		if(GBitMask == 0x03E0)
		{
			HighColor555Flag = TRUE;    // 5:5:5 格式
		}
		else
		if(GBitMask == 0x07E0)
		{
			HighColor555Flag = FALSE;   // 5:6:5 格式
		}
		else
		{	// 显卡不支持标准高彩色格式
			return FALSE;
		}
		InitRGBPACKWORD();
	}

	// 清空主表面（黑色）
	ddsd.dwSize = sizeof(ddsd);
	while ((ddrval = lpDDSPrimary->Lock(NULL, &ddsd, 0, NULL)) == DDERR_WASSTILLDRAWING);
	if(ddrval == DD_OK)
	{
		LPBYTE	lpDst = (LPBYTE)ddsd.lpSurface;
		DWORD	FillSize = ScreenH * ddsd.lPitch;
		_asm {
			MOV		EDI, lpDst
			MOV		ECX, FillSize
			SHR		ECX, 2
			XOR		EAX, EAX
			CLD
			REP		STOSD
		}
		lpDDSPrimary->Unlock(NULL);
	}

	// 清空后备缓冲区（黑色）
	ddsd.dwSize = sizeof(ddsd);
	while ((ddrval = lpDDSBack->Lock(NULL, &ddsd, 0, NULL)) == DDERR_WASSTILLDRAWING);
	if(ddrval == DD_OK)
	{
		LPBYTE	lpDst = (LPBYTE)ddsd.lpSurface;
		DWORD	FillSize = ScreenH * ddsd.lPitch;
		_asm {
			MOV		EDI, lpDst
			MOV		ECX, FillSize
			SHR		ECX, 2
			XOR		EAX, EAX
			CLD
			REP		STOSD
		}
		lpDDSBack->Unlock(NULL);
	}

	ScreenMode = Mode;
	return TRUE;
}

// -------------------------------------

/**
 * @brief 初始化 DirectDraw（自动尝试所有模式）
 *
 * 依次尝试 4 种显示模式，直到成功为止。
 * 成功后会清空主渲染页和运动模糊页。
 *
 * @return TRUE 成功，FALSE 所有模式均失败
 */
BOOL InitDDraw(void)
{
	int		i;

	for(i=0; i<4; i++)
	{
		if(StartDDraw(i))
		{
			ClearRenderBuffer(0);   // 清空主渲染页
			ClearRenderBuffer(2);   // 清空运动模糊页
			return TRUE;
		}
	}

	return FALSE;
}

// -------------------------------------

/**
 * @brief 恢复丢失的 DirectDraw 表面
 *
 * 当 Alt+Tab 切换或显示模式改变后，DirectDraw 表面可能丢失。
 * 此函数恢复表面并重新清空。
 *
 * @return DirectDraw 操作结果 HRESULT
 */
HRESULT RestoreDDraw(void)
{
	DDSURFACEDESC	ddsd;
    HRESULT			ddrval;

	if(lpDDSPrimary)
	{
		ddrval = lpDDSPrimary->Restore();
		{
			// 清空恢复后的主表面
			ddsd.dwSize = sizeof(ddsd);
			while ((ddrval = lpDDSPrimary->Lock(NULL, &ddsd, 0, NULL)) == DDERR_WASSTILLDRAWING);
			if(ddrval == DD_OK)
			{
				LPBYTE	lpDst = (LPBYTE)ddsd.lpSurface;
				DWORD	FillSize = ScreenH * ddsd.lPitch;
				_asm {
					MOV		EDI, lpDst
					MOV		ECX, FillSize
					SHR		ECX, 2
					XOR		EAX, EAX
					CLD
					REP		STOSD
				}
				lpDDSPrimary->Unlock(NULL);
			}
		}

		if(ddrval==DD_OK)
		{
			if(lpDDSBack)
			{
				ddrval = lpDDSBack->Restore();

				// 清空恢复后的后备缓冲区
				ddsd.dwSize = sizeof(ddsd);
				while ((ddrval = lpDDSBack->Lock(NULL, &ddsd, 0, NULL)) == DDERR_WASSTILLDRAWING);
				if(ddrval == DD_OK)
				{
					LPBYTE	lpDst = (LPBYTE)ddsd.lpSurface;
					DWORD	FillSize = ScreenH * ddsd.lPitch;
					_asm {
						MOV		EDI, lpDst
						MOV		ECX, FillSize
						SHR		ECX, 2
						XOR		EAX, EAX
						CLD
						REP		STOSD
					}
					lpDDSBack->Unlock(NULL);
				}
			}
		}
	}
    return ddrval;
}

/*--------------------------------------*/
//			资源释放
/*--------------------------------------*/

/**
 * @brief 释放所有 DirectX 对象
 *
 * 按正确顺序释放 DirectSound、DirectDraw、DirectInput 资源。
 * 程序退出前调用。
 */
void ReleaseObjects(void)
{
	ReleaseDSound();
	ReleaseDDraw();
	ReleaseDInput();
}

/*--------------------------------------*/
//			初始化失败处理
/*--------------------------------------*/

/**
 * @brief 初始化失败处理函数
 *
 * 显示错误消息框，释放所有资源，销毁窗口。
 * 通常在初始化失败时调用，然后程序会退出。
 *
 * @param msg 错误消息字符串
 */
void InitFail(LPSTR msg)
{
    ReleaseObjects();
    MessageBoxA(hWndCopy, msg, "MESSAGE", MB_OK);
    DestroyWindow(hWndCopy);
}

/*--------------------------------------*/
//	屏幕刷新代码（由 Refresh.H 实现）
/*--------------------------------------*/

#include "Refresh.H"

/*--------------------------------------*/
//			检测程序是否已运行
/*--------------------------------------*/

/**
 * @brief 检测程序是否已经在运行
 *
 * 通过查找具有相同标题的窗口，防止程序多开。
 * 如果检测到已运行的实例，将其窗口带到前台。
 *
 * @return TRUE 已有实例在运行，FALSE 未检测到
 */
BOOL AlreadyRun(void)
{
	HWND FirsthWnd, FirstChildhWnd;

	if((FirsthWnd = FindWindowA(NULL, TITLE)) != NULL)
	{
		FirstChildhWnd = GetLastActivePopup(FirsthWnd);
		BringWindowToTop(FirsthWnd);
	
		if(FirsthWnd != FirstChildhWnd)
		{
			BringWindowToTop(FirstChildhWnd);
		}
		
		ShowWindow(FirsthWnd, SW_SHOWNORMAL);
		
		return TRUE;
	}
	
	return FALSE;
}

/*--------------------------------------*/
//			窗口消息处理
/*--------------------------------------*/

/**
 * @brief 窗口消息处理回调函数
 *
 * 处理所有发送到游戏窗口的消息，包括：
 *   - 窗口激活/失活（暂停/恢复游戏）
 *   - 键盘快捷键（F10/F11/F12/ESC）
 *   - 系统命令（关闭窗口等）
 *
 * @param hWnd    窗口句柄
 * @param message 消息类型
 * @param wParam  消息参数
 * @param lParam  消息参数
 * @return 消息处理结果
 */
long FAR PASCAL WindowProc(HWND hWnd, UINT message, 
                            WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {
    case WM_ACTIVATEAPP:
		bActive = (!(wParam == WA_INACTIVE));

		if(bActive)
		{
			// 窗口激活：重新获取键盘设备
			if(lpKeyboard) lpKeyboard->Acquire();

			KEYSTARTCOUNTER = KEYENDCOUNTER;

			// 恢复视频播放声音
			if(VideoPlayingFlag)
			{
				if(lpAudioDataBuffer)
				{
					lpAudioDataBuffer->Play(NULL, NULL, 0);
				}
			}
		}
		else
		{
			// 窗口失活：释放键盘设备
			if(lpKeyboard) lpKeyboard->Unacquire();

			// 暂停视频播放声音
			if(VideoPlayingFlag)
			{
				if(lpAudioDataBuffer)
				{
					lpAudioDataBuffer->Stop();
				}
			}
		}
        break;

    case WM_CREATE:
        break;

    case WM_SETCURSOR:
        SetCursor(NULL);    // 隐藏鼠标光标
        return TRUE;

    case WM_SYSKEYDOWN:
        switch(wParam)
        {
			case VK_F10:	// F10: 切换 TV/PC 模式（分辨率缩放）
				StatusTime = timeGetTime();
				StatusFlag = TRUE;
				if(ScreenMode > 1)
				{
					ScreenMode -= 2;    // 切换到 2 倍缩放
					if(StartDDraw(ScreenMode) == FALSE)
					{
						if(!InitDDraw())
						{
							InitFail((char*)"Init DirectDraw Fail");
							return NULL;
						}
					}
				}
				else
				{
					ScreenMode += 2;    // 切换到 1 倍原始
					if(StartDDraw(ScreenMode) == FALSE)
					{
						if(!InitDDraw())
						{
							InitFail((char*)"Init DirectDraw Fail");
							return NULL;
						}
					}
				}
            break;
		}
        break;

    case WM_KEYDOWN:
        switch(wParam)
        {
			case VK_F11:	// F11: 切换运动模糊等级
				MotionBlurFlag ++;
				if(MotionBlurFlag > 2)
				{
					MotionBlurFlag = 0;
				}

				StatusTime = timeGetTime();
				StatusFlag = TRUE;
				BackupToMotionBlurBuffer();
			break;

			case VK_F12:	// F12: 切换状态显示（FPS 等）
				StatusTime = timeGetTime();
				StatusFlag = !StatusFlag;
            break;

			case VK_ESCAPE:
				if(VideoPlayingFlag)
					VideoPlayingFlag = FALSE;   // 停止视频播放
				else
				{
					bActive = FALSE;
					bLive = FALSE;
					DestroyWindow(hWnd);        // 退出程序
				}
			    return 0;
		}
        break;

    case WM_DESTROY:
		if(VideoPlayingFlag)
		{
			VideoPlayingFlag = FALSE;	// 停止视频播放
			bLive = FALSE;				// 程序需要退出
		}
		else
		{
			bActive = FALSE;
			bLive = FALSE;

			ReleaseObjects();
		    PostQuitMessage(0);
		}
        break;
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}

/*--------------------------------------*/
//			初始化执行
/*--------------------------------------*/

/**
 * @brief 执行完整的程序初始化
 *
 * 初始化流程：
 *   1. 注册窗口类并创建窗口
 *   2. 初始化行偏移查找表
 *   3. 检测 MMX 支持
 *   4. 初始化 DirectInput
 *   5. 初始化 DirectDraw
 *   6. 初始化 DirectSound（失败不终止）
 *
 * @param hInstance 应用程序实例句柄
 * @param nCmdShow  窗口显示方式
 * @return TRUE 初始化成功，FALSE 失败
 */
BOOL doInit(HINSTANCE hInstance, int nCmdShow)
{
    HWND                hwnd;
    WNDCLASSA            wc;

    // 设置并注册窗口类
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIconA(hInstance, "PROG_ICON");
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH )GetStockObject(BLACK_BRUSH);
    wc.lpszMenuName = NAME;
    wc.lpszClassName = NAME;
    RegisterClassA(&wc);
    
    // 创建全屏窗口
    hwnd = CreateWindowExA(WS_EX_TOPMOST,
                          NAME,
                          TITLE,
                          WS_POPUP,
                          0,
                          0,
                          GetSystemMetrics(SM_CXSCREEN),
                          GetSystemMetrics(SM_CYSCREEN),
                          NULL,
                          NULL,
                          hInstance,
                          NULL);

    if(!hwnd) return FALSE;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    hWndCopy = hwnd;
	hInstanceCopy = hInstance;

	////////////////
	// 系统初始化
	////////////

	SetLineStartOffset();	// 初始化行偏移查找表

	CheckMMX();				// 检测 MMX 支持
	if(MMXFOUND == FALSE)
	{
		InitFail((char*)"MMX CPU required");
		return FALSE;
	}

	if(!InitDInput())					// 初始化 DirectInput
	{
		InitFail((char*)"Init DirectInput Fail");
		return FALSE;
	}

	if(!InitDDraw())					// 初始化 DirectDraw
	{
		InitFail((char*)"Init DirectDraw Fail");
		return FALSE;
	}

	if(!InitDSound())
	{
		ReleaseDSound();
		InitDirectSoundDefault();
		// 声音初始化失败不终止程序
	}

	StatusTime = timeGetTime();

    return TRUE;
} 

/*--------------------------------------*/
//			WinMain 入口
/*--------------------------------------*/

/**
 * @brief Windows 程序入口点
 *
 * 程序主流程：
 *   1. 创建互斥量防止多开
 *   2. 执行初始化（doInit）
 *   3. 调用用户初始化（InitGame）
 *   4. 进入消息/游戏主循环
 *   5. 调用用户清理（ExitGame）
 *   6. 退出程序
 *
 * 主循环逻辑：
 *   - 优先处理 Windows 消息
 *   - 窗口激活时：处理输入 + 游戏循环
 *   - 窗口失活时：等待消息（节省 CPU）
 *
 * @param hInstance     当前实例句柄
 * @param hPrevInstance 前一实例句柄（始终为 NULL）
 * @param lpCmdLine     命令行参数字符串
 * @param nCmdShow      窗口初始显示状态
 * @return 程序退出码
 */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    MSG		msg;
	HANDLE	hMutex;

	// 创建互斥量防止程序多开
	hMutex = CreateMutexA(NULL, TRUE, TITLE);
	if(GetLastError() == ERROR_ALREADY_EXISTS) return NULL;

	if(!doInit(hInstance,nCmdShow))
        return FALSE;

	InitGame();

	// 游戏主循环
    while(bLive)
    {
        if(PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
        {
            if (!GetMessage(&msg, NULL, 0, 0)) return msg.wParam;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
		else
		{
	        if(bActive)
		    {	// 窗口激活：执行游戏逻辑
				KeyboardFunc();
				GameLoop();
			}
	        else
		    {	// 窗口失活：等待消息（降低 CPU 占用）
				WaitMessage();
			}
		}
	}

	ExitGame();

	return NULL;
} 

