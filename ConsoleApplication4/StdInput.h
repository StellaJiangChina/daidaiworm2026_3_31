/*  StdInput.H  */
//
// 输入处理头文件 - DirectInput 键盘和鼠标支持
//
// 提供基于 DirectInput 的键盘和鼠标输入处理，包括：
//   - 键盘按键状态检测（按下/释放）
//   - 键盘事件缓冲（支持按键队列）
//   - 鼠标位置和按钮状态
//   - 鼠标光标图像设置和绘制
//
// 键盘输入模型：
//   KEYSTATUSFLAG[256] - 实时按键状态（每帧更新，1=按下，0=释放）
//   KEYBUFFER[]        - 按键事件队列（仅记录按下事件，用于触发式输入）
//
// 鼠标输入模型：
//   MOUSEX, MOUSEY     - 当前鼠标位置（相对于渲染缓冲区）
//   MouseButton0~3     - 按钮状态（0=左键，1=右键，2=中键，3=附加键）
//   TimeFunc           - 定时器回调，每 10ms 读取一次鼠标数据

#define DIRECTINPUT_VERSION         0x0800  // DirectInput 8.0

#include <dinput.h>     // DirectInput 头文件

#include "KEYDEF.H"     // 键盘扫描码定义

// 输入缓冲区大小（键盘和鼠标事件队列长度）
#define DINPUT_BUFFERSIZE       64      // 缓冲区元素数量

// -------------------------------------
// DirectInput 接口对象
// -------------------------------------

LPDIRECTINPUT			lpDirectInput;	// DirectInput 主对象
LPDIRECTINPUTDEVICE		lpKeyboard;		// 键盘设备接口
LPDIRECTINPUTDEVICE		lpMouse; 		// 鼠标设备接口

// -------------------------------------
//		全局数据
// -------------------------------------

// 键盘状态数组：KEYSTATUSFLAG[扫描码] = 1 表示该键当前被按下
BYTE    KEYSTATUSFLAG[256];

// 键盘事件循环缓冲区：存储按键按下事件（用于触发式检测）
BYTE    KEYBUFFER[DINPUT_BUFFERSIZE];

// 键盘缓冲区读写指针（循环队列）
long	KEYSTARTCOUNTER = 0, KEYENDCOUNTER = 0;

// 多媒体定时器 ID（用于鼠标轮询）
UINT	uTimerID = NULL;

// 鼠标显示标志：TRUE = 显示自定义鼠标光标
int		MOUSEFLAG = FALSE;

// 鼠标事件缓冲区（保留，当前未使用）
BYTE    MOUSEBUFFER[DINPUT_BUFFERSIZE];

// 鼠标缓冲区读写指针（保留，当前未使用）
long	MOUSESTARTCOUNTER = 0, MOUSEENDCOUNTER = 0;

// 鼠标当前位置（限制在渲染区域内）
long	MOUSEX = RENDER_WIDTH - 1;   // 鼠标 X 坐标
long	MOUSEY = RENDER_HEIGHT - 1;  // 鼠标 Y 坐标

// 鼠标按钮状态（0=释放，0x80=按下）
long	MouseButton0 = 0;   // 左键
long	MouseButton1 = 0;   // 右键
long	MouseButton2 = 0;   // 中键
long	MouseButton3 = 0;   // 附加键（侧键等）

// -------------------------------------

/**
 * @brief 释放 DirectInput 资源
 *
 * 停止定时器，释放键盘和鼠标设备，释放 DirectInput 对象。
 * 程序退出前调用。
 */
void ReleaseDInput(void) 
{ 
	// 停止多媒体定时器
	if(uTimerID != NULL)
	{
		timeKillEvent(uTimerID);
		uTimerID = NULL;
	}

    if (lpDirectInput) 
    { 
        if(lpKeyboard) 
        { 
            // 释放设备前必须先 Unacquire
            lpKeyboard->Unacquire(); 
            lpKeyboard->Release();
            lpKeyboard = NULL; 
        }
		if(lpMouse)
		{
            lpMouse->Unacquire(); 
            lpMouse->Release();
            lpMouse = NULL; 
		}
        lpDirectInput->Release();
        lpDirectInput = NULL; 
    } 
} 

// -------------------------------------

/**
 * @brief 重置键盘事件缓冲区
 *
 * 清空按键事件队列（将读写指针都设为 0）。
 * 通常在场景切换时调用，避免旧按键事件影响新场景。
 */
void ResetKeyBuffer(void)
{
	KEYSTARTCOUNTER = 0; KEYENDCOUNTER = 0;
}

// -------------------------------------

/**
 * @brief 从键盘事件缓冲区读取一个按键
 *
 * 非阻塞读取，如果缓冲区为空返回 0。
 * 返回的是按键扫描码（定义见 KEYDEF.H）。
 *
 * @return 按键扫描码，缓冲区空时返回 0
 */
BYTE GetKey(void)
{
	BYTE	res;

	// 缓冲区空检查
	if(KEYSTARTCOUNTER == KEYENDCOUNTER) return 0;

	// 读取并移动读指针
	res = KEYBUFFER[KEYSTARTCOUNTER];
	KEYSTARTCOUNTER ++;
	if(KEYSTARTCOUNTER >= DINPUT_BUFFERSIZE) KEYSTARTCOUNTER = 0;

	return res;
}

// -------------------------------------

/**
 * @brief 处理键盘输入（每帧调用）
 *
 * 从 DirectInput 读取缓冲的键盘数据，更新：
 *   - KEYSTATUSFLAG[]: 实时按键状态
 *   - KEYBUFFER[]: 按键按下事件队列
 *
 * 如果设备丢失（如 Alt+Tab 切换），尝试重新获取设备。
 */
void KeyboardFunc(void)
{
	// 获取键盘数据
    if(lpKeyboard)
	{
		DIDEVICEOBJECTDATA	rgod[DINPUT_BUFFERSIZE];	// 接收缓冲数据
		DWORD				cod;						// 实际读取的元素数
		HRESULT				hr; 
 
    again:;
		cod = DINPUT_BUFFERSIZE;
		hr = lpKeyboard->GetDeviceData(sizeof(DIDEVICEOBJECTDATA), rgod, &cod, 0);
		if(hr != DI_OK)			// 出错或缓冲区溢出
		{
			int		i;
			// 清空按键状态
			for(i=0; i<256; i++) KEYSTATUSFLAG[i] = 0;

            if(hr == DIERR_INPUTLOST)
			{
                hr = lpKeyboard->Acquire();
				if FAILED(hr)
				{
					return;
				}
				if SUCCEEDED(hr)
				{
                    goto again;     // 重新尝试读取
                }
            }
        }

        // 处理读取到的数据
        if(SUCCEEDED(hr) && cod > 0)
		{
            DWORD	iod;

            for(iod=0; iod<cod; iod++)
			{
				// 最高位为 1 表示按下，0 表示释放
				if((rgod[iod].dwData & 0x80))
				{
					// 记录按下事件到队列
					KEYBUFFER[KEYENDCOUNTER] = (BYTE)(rgod[iod].dwOfs);
					KEYENDCOUNTER ++;
					if(KEYENDCOUNTER >= DINPUT_BUFFERSIZE) KEYENDCOUNTER = 0;
				}
                // 更新实时状态
                KEYSTATUSFLAG[rgod[iod].dwOfs] = (BYTE)(rgod[iod].dwData & 0x80);
            }
        }
    }
}

// -------------------------------------

/**
 * @brief 定时器回调函数（每 10ms 调用）
 *
 * 读取鼠标输入数据，更新鼠标位置和按钮状态。
 * 鼠标位置被限制在渲染区域内 [0, RENDER_WIDTH-1] × [0, RENDER_HEIGHT-1]。
 *
 * @param uTimerID 定时器 ID
 * @param uMsg     消息类型
 * @param dwUser   用户数据
 * @param dw1      保留
 * @param dw2      保留
 */
void CALLBACK TimeFunc(UINT uTimerID, UINT uMsg, DWORD dwUser, DWORD dw1, DWORD dw2)
{
	// 窗口非激活时不处理
	if(!bActive || !bLive) return;
	
	// 读取鼠标数据
	while (TRUE)
	{
		DIDEVICEOBJECTDATA	data;
		DWORD				elements=1;
		HRESULT hr = lpMouse->GetDeviceData(sizeof(data), &data, &elements, 0);

	    if FAILED(hr)
		{
			if(hr == DIERR_INPUTLOST)
			{
				hr = lpMouse->Acquire();
				if FAILED(hr)
				{
					return;
				}
			}
		}
		else
		if(elements == 1)
		{
			// 根据数据类型更新相应状态
			switch(data.dwOfs)
			{
				case DIMOFS_X:
				MOUSEX+=data.dwData;    // X 轴相对位移
				break;
				case DIMOFS_Y: 
				MOUSEY+=data.dwData;    // Y 轴相对位移
				break;
				case DIMOFS_BUTTON0: 
				MouseButton0=data.dwData;   // 左键
				break;
				case DIMOFS_BUTTON1: 
				MouseButton1=data.dwData;   // 右键
				break;
				case DIMOFS_BUTTON2: 
				MouseButton2=data.dwData;   // 中键
				break;
				case DIMOFS_BUTTON3: 
				MouseButton3=data.dwData;   // 附加键
				break;
			}
		}
		else if (elements==0) break;    // 无更多数据
	}

	// 限制鼠标位置在渲染区域内
	if(MOUSEX < 0) MOUSEX = 0;
	if(MOUSEY < 0) MOUSEY = 0;
	if(MOUSEX >= RENDER_WIDTH) MOUSEX = RENDER_WIDTH - 1;
	if(MOUSEY >= RENDER_HEIGHT) MOUSEY = RENDER_HEIGHT - 1;
}

// -------------------------------------

/**
 * @brief 初始化 DirectInput
 *
 * 创建 DirectInput 对象，初始化鼠标和键盘设备：
 *   1. 创建 DirectInput8 对象
 *   2. 创建并配置鼠标设备（独占模式，前台访问）
 *   3. 创建并配置键盘设备（非独占模式，前台访问）
 *   4. 设置 10ms 定时器轮询鼠标
 *
 * @return TRUE 初始化成功，FALSE 失败
 */
BOOL InitDInput(void)
{ 
    HRESULT hr;
 
    // 创建 DirectInput 对象
    hr = DirectInput8Create(hInstanceCopy, DIRECTINPUT_VERSION,
		IID_IDirectInput8, (LPVOID*)&lpDirectInput, NULL);

    if FAILED(hr)
	{
		return FALSE;
	}

	/*  鼠标设备初始化  */
	hr = lpDirectInput->CreateDevice(GUID_SysMouse, &lpMouse, NULL);
	if (FAILED(hr))
	{
		return FALSE;
	}

	hr = lpMouse->SetDataFormat(&c_dfDIMouse);
	if (FAILED(hr))
	{
		return FALSE;
	}

	// 独占模式 + 前台访问（全屏游戏常用设置）
	hr = lpMouse->SetCooperativeLevel(hWndCopy,
               DISCL_EXCLUSIVE | DISCL_FOREGROUND);
	if (FAILED(hr))
	{
		return FALSE;
	}
 
	// 设置鼠标缓冲区大小
	DIPROPDWORD property;
	property.diph.dwSize=sizeof(DIPROPDWORD);
	property.diph.dwHeaderSize=sizeof(DIPROPHEADER);
	property.diph.dwObj=0;
	property.diph.dwHow=DIPH_DEVICE;
	property.dwData=64;

	hr = lpMouse->SetProperty(DIPROP_BUFFERSIZE, &property.diph);
	if (FAILED(hr))
	{
		return FALSE;
	}

    hr = lpMouse->Acquire(); 
    if FAILED(hr) 
    { 
		return FALSE;
    } 

	/*  键盘设备初始化  */
    hr = lpDirectInput->CreateDevice(GUID_SysKeyboard, &lpKeyboard, NULL);
    if FAILED(hr)
    {
		return FALSE;
    }
 
    hr = lpKeyboard->SetDataFormat(&c_dfDIKeyboard); 
    if FAILED(hr) 
    { 
		return FALSE;
    } 
 
    // 非独占模式 + 前台访问（允许系统快捷键）
    hr = lpKeyboard->SetCooperativeLevel(hWndCopy, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND);
    if FAILED(hr) 
    { 
		return FALSE;
    } 
 
	// 设置键盘缓冲区大小
	property.diph.dwSize=sizeof(DIPROPDWORD);
	property.diph.dwHeaderSize=sizeof(DIPROPHEADER);
	property.diph.dwObj=0;
	property.diph.dwHow=DIPH_DEVICE;
	property.dwData=DINPUT_BUFFERSIZE;

	hr = lpKeyboard->SetProperty(DIPROP_BUFFERSIZE, &property.diph);

    if FAILED(hr)
	{
        return FALSE;
    }

    hr = lpKeyboard->Acquire(); 
    if FAILED(hr)
    { 
		return FALSE;
    } 
 
	// 创建 10ms 周期定时器（用于鼠标轮询）
	if((uTimerID = timeSetEvent(10, 0, TimeFunc, (DWORD)0, TIME_PERIODIC)) == NULL)
	{
		InitFail((char*)"Set timer Fail");
		return FALSE;
	}

    return TRUE; 
} 

// -------------------------------------
//	鼠标图形代码
// -------------------------------------

LPDWORD		MouseImageBuffer = NULL;        // 鼠标光标图像缓冲区
LPDWORD		MouseImageBackBuffer = NULL;    // 鼠标背景备份缓冲区（用于恢复）

long	MOUSEATTACHX = 0;   // 鼠标热点 X 偏移（图像左上角到热点的距离）
long	MOUSEATTACHY = 0;   // 鼠标热点 Y 偏移

// -------------------------------------

/**
 * @brief 设置鼠标热点 X 偏移
 * @param X 热点 X 坐标（相对于图像左上角）
 */
#define SetMouseAttachX(X)		MOUSEATTACHX = (X)

/**
 * @brief 设置鼠标热点 Y 偏移
 * @param Y 热点 Y 坐标（相对于图像左上角）
 */
#define SetMouseAttachY(Y)		MOUSEATTACHY = (Y)

/**
 * @brief 显示鼠标光标
 */
#define MouseOn()		MOUSEFLAG = TRUE

/**
 * @brief 隐藏鼠标光标
 */
#define MouseOff()		MOUSEFLAG = FALSE

// -------------------------------------

/**
 * @brief 设置鼠标光标图像
 *
 * 设置自定义鼠标光标图像，并分配背景备份缓冲区。
 * 图像格式与 PutImage 相同（前两个 DWORD 为宽高）。
 *
 * @param Image 鼠标图像数据指针，NULL 表示清除
 */
void SetMouseImage(LPDWORD Image)
{
	long	Width, Height;

	if(Image == NULL) return;

	Width = *(Image);
	Height = *(Image + 1);

	if(Width<=0 || Height<=0) return;

	// 设置图像指针
	MouseImageBuffer = Image;

	// 为背景备份分配内存
	FreeImageBuffer(MouseImageBackBuffer);
	MouseImageBackBuffer = AllocImageBuffer(Width, Height);
}

// -------------------------------------
