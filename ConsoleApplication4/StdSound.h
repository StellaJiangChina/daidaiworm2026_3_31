/*  StdSound.H  */
//
// 声音播放头文件 - DirectSound 支持
//
// 提供基于 DirectSound 的 PCM 音频播放功能，包括：
//   - DirectSound 初始化（主缓冲区设置）
//   - 声音缓冲区创建和管理
//   - RAW 音频文件加载
//   - 音量/声像控制
//
// 音频格式：
//   - 格式：PCM（脉冲编码调制）
//   - 声道：立体声（2声道）
//   - 采样率：22050 Hz
//   - 位深度：16位
//   - 块对齐：4字节（2声道 × 2字节）
//

// -------------------------------------
// 初始化代码
// -------------------------------------

#include <dsound.h>     // DirectSound 头文件

// -------------------------------------
// DirectSound 接口对象
// -------------------------------------

LPDIRECTSOUND			lpDirectSound;		// DirectSound 主对象

// -------------------------------------

// 最大声音数量
#define	SOUND_MAX_NUM	256

LPDIRECTSOUNDBUFFER		lpPrimarySoundBuffer;                       // 主声音缓冲区
LPDIRECTSOUNDBUFFER		lpSoundData[SOUND_MAX_NUM];	// 声音数据缓冲区句柄数组

// -------------------------------------

/**
 * @brief 以默认模式初始化 DirectSound
 *
 * 创建 DirectSound 对象并设置为普通协作级别。
 * 用于声音初始化失败后的降级处理。
 *
 * @return TRUE 成功，FALSE 失败
 */
BOOL InitDirectSoundDefault(void)
{
	if(DS_OK == DirectSoundCreate(NULL, &lpDirectSound, NULL))
	{
		// 创建成功
		lpDirectSound->SetCooperativeLevel(hWndCopy, DSSCL_NORMAL);
		return TRUE;
	}
	return FALSE;
}

// -------------------------------------

/**
 * @brief 初始化 DirectSound（标准模式）
 *
 * 初始化流程：
 *   1. 创建 DirectSound 对象
 *   2. 设置为优先级协作级别（允许设置主缓冲区格式）
 *   3. 创建主声音缓冲区
 *   4. 设置音频格式为 22KHz 16位立体声
 *
 * 优先级协作级别允许程序控制主缓冲区的音频格式，
 * 确保所有声音以统一格式播放。
 *
 * @return TRUE 初始化成功，FALSE 失败
 */
BOOL InitDSound(void)
{
    DSBUFFERDESC	dsbdesc;    // 缓冲区描述结构
	WAVEFORMATEX	pcmwf;      // 波形格式结构

    // 设置波形格式（PCM 22KHz 16位立体声）
    memset(&pcmwf, 0, sizeof(WAVEFORMATEX));
    pcmwf.wFormatTag = WAVE_FORMAT_PCM;     // PCM 格式
    pcmwf.nChannels = 2;                    // 立体声
    pcmwf.nSamplesPerSec = 22050;           // 22.05KHz 采样率
    pcmwf.nBlockAlign = 4;                  // 每个采样点字节数（2声道 × 2字节）
    pcmwf.nAvgBytesPerSec = 
        pcmwf.nSamplesPerSec * pcmwf.nBlockAlign;   // 每秒字节数
    pcmwf.wBitsPerSample = 16;              // 16位采样
	pcmwf.cbSize = 0;                       // 无额外信息

    // 设置主缓冲区描述
    memset(&dsbdesc, 0, sizeof(DSBUFFERDESC));
    dsbdesc.dwSize = sizeof(DSBUFFERDESC);
    dsbdesc.dwFlags = DSBCAPS_PRIMARYBUFFER;    // 主缓冲区标志

    // 主缓冲区大小由硬件决定
    dsbdesc.dwBufferBytes = 0; 
    dsbdesc.lpwfxFormat = NULL; // 主缓冲区格式参数必须为 NULL

	// 创建 DirectSound 对象
	if(DS_OK == DirectSoundCreate(NULL, &lpDirectSound, NULL))
	{
		// 设置优先级协作级别
	    if(DS_OK == lpDirectSound->SetCooperativeLevel(hWndCopy, DSSCL_PRIORITY))
		{
			// 创建主缓冲区
	        if(DS_OK == lpDirectSound->CreateSoundBuffer(&dsbdesc, &lpPrimarySoundBuffer, NULL))
			{
				// 设置主缓冲区音频格式
				if(DS_OK == lpPrimarySoundBuffer->SetFormat(&pcmwf))
					return TRUE;
	        }
	    }
	}
	return FALSE;
}

// -------------------------------------

/**
 * @brief 释放所有 DirectSound 资源
 *
 * 释放所有声音数据缓冲区和主缓冲区，最后释放 DirectSound 对象。
 * 程序退出前调用。
 */
void ReleaseDSound(void)
{
	// 释放所有声音数据缓冲区
	for(int i=0; i<SOUND_MAX_NUM; i++)
	{
		RELEASE(lpSoundData[i]);
	}

	// 释放主缓冲区和 DirectSound 对象
	RELEASE(lpPrimarySoundBuffer);
	RELEASE(lpDirectSound);
}

// -------------------------------------

/**
 * @brief 创建 DirectSound 辅助缓冲区
 *
 * 创建一个用于存储声音数据的辅助缓冲区。
 * 辅助缓冲区支持音量和声像控制。
 *
 * @param lpWaveFormat  波形格式数据指针（PCMWAVEFORMAT 结构）
 * @param BufLen        缓冲区大小（字节）
 * @param lplpDsb       输出：缓冲区接口指针的指针
 * @return TRUE 创建成功，FALSE 失败
 */
int CreateDirectSoundBuffer(LPBYTE lpWaveFormat, DWORD BufLen, LPDIRECTSOUNDBUFFER *lplpDsb)
{   
	PCMWAVEFORMAT		pcmwf;      // PCM 格式结构
    DSBUFFERDESC		dsbdesc;    // 缓冲区描述
	HRESULT				hres;       // 返回值

    *lplpDsb = NULL;
	memcpy(&pcmwf, lpWaveFormat, sizeof(PCMWAVEFORMAT));

    // 设置缓冲区描述
    memset(&dsbdesc, 0, sizeof(DSBUFFERDESC)); 
    dsbdesc.dwSize = sizeof(DSBUFFERDESC);
    // 软件混音 + 音量控制 + 声像控制
    dsbdesc.dwFlags = DSBCAPS_LOCSOFTWARE | DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLPAN;

    dsbdesc.dwBufferBytes = BufLen; 
    dsbdesc.lpwfxFormat = (LPWAVEFORMATEX)&pcmwf;    // 音频格式

    // 创建缓冲区
    hres = lpDirectSound->CreateSoundBuffer(&dsbdesc, lplpDsb, NULL);
    if(DS_OK == hres) 
	{
        // 成功，有效接口在 *lplpDsb 中
		return TRUE;
    } 

    return FALSE;    
}

// -------------------------------------

/**
 * @brief 向 DirectSound 缓冲区写入音频数据
 *
 * 将音频数据写入缓冲区的指定偏移位置。
 * 支持环形缓冲区（如果数据跨越缓冲区末尾，会返回两个指针）。
 *
 * @param lpDsb         目标缓冲区接口
 * @param dwOffset      写入偏移量（字节）
 * @param lpbSoundData  音频数据源指针
 * @param dwSoundBytes  要写入的字节数
 * @return TRUE 写入成功，FALSE 失败
 */
int	WriteDirectSoundBuffer(LPDIRECTSOUNDBUFFER lpDsb, DWORD dwOffset, LPBYTE lpbSoundData, DWORD dwSoundBytes)
{
    LPVOID lpvPtr1;     // 第一段内存指针
	DWORD dwBytes1;     // 第一段字节数
    LPVOID lpvPtr2;     // 第二段内存指针（环形回绕）
	DWORD dwBytes2;     // 第二段字节数
	HRESULT hres;

    // 锁定缓冲区
    hres = lpDsb->Lock(dwOffset, dwSoundBytes, &lpvPtr1, &dwBytes1, &lpvPtr2, &dwBytes2, 0);    

    // 缓冲区丢失（如 Alt+Tab 切换），尝试恢复
    if(DSERR_BUFFERLOST == hres) 
	{
        lpDsb->Restore();
	    hres = lpDsb->Lock(dwOffset, dwSoundBytes, &lpvPtr1, &dwBytes1, &lpvPtr2, &dwBytes2, 0);    
	}

    // 写入数据
    if(DS_OK == hres) 
	{
        CopyMemory(lpvPtr1, lpbSoundData, dwBytes1);
        if(NULL != lpvPtr2) 
		{
            // 数据跨越缓冲区末尾，写入第二段
            CopyMemory(lpvPtr2, lpbSoundData+dwBytes1, dwBytes2);        
		}
        // 解锁缓冲区
        hres = lpDsb->Unlock(lpvPtr1, dwBytes1, lpvPtr2, dwBytes2);        
		if(DS_OK == hres) 
		{
            return TRUE;    
		}
    }
    return FALSE;
}

// -------------------------------------
//	加载 RAW 声音文件
// -------------------------------------

/**
 * @brief 加载 RAW 格式声音文件
 *
 * 加载无头部的原始 PCM 音频数据到指定声音槽。
 * RAW 格式只包含纯音频采样数据，没有文件头。
 *
 * @param num               声音槽编号（0 ~ SOUND_MAX_NUM-1）
 * @param filename          RAW 文件路径
 * @param nChannels         声道数（1=单声道，2=立体声）
 * @param nSamplesPerSec    采样率（如 22050, 44100）
 * @param wBitsPerSample    位深度（8 或 16）
 */
void LoadRawSndData(int num, LPSTR filename, WORD nChannels, DWORD nSamplesPerSec, WORD  wBitsPerSample)
{
	FILE			*fp;
	DWORD			DataLen;
	WAVEFORMATEX 	WaveFmt;

	// 检查 DirectSound 是否已初始化
	if(lpDirectSound == NULL)
	{
		return;
	}

	// 打开文件
	fopen_s(&fp,filename,"rb");
	if(fp == NULL)
	{
		InitFail((char*)"Load Sound File Fail");
		return;
	}
	
	// 获取文件大小（RAW 文件大小 = 音频数据大小）
	fseek(fp, 0L, SEEK_END);
	DataLen = ftell(fp);
	fseek(fp, 0L, SEEK_SET);

	// 设置波形格式
	WaveFmt.wFormatTag = WAVE_FORMAT_PCM;
	WaveFmt.nChannels = nChannels;
	WaveFmt.nBlockAlign = nChannels * wBitsPerSample / 8;
	WaveFmt.nSamplesPerSec = nSamplesPerSec;
	WaveFmt.nAvgBytesPerSec = nSamplesPerSec * WaveFmt.nBlockAlign;
	WaveFmt.wBitsPerSample = wBitsPerSample;
	WaveFmt.cbSize = 0; 

	// 创建声音缓冲区
	if(CreateDirectSoundBuffer((LPBYTE)(&WaveFmt), DataLen, &lpSoundData[num]))
	{
		{
		    LPVOID		lpvPtr1; 
			DWORD		dwBytes1; 
			LPVOID		lpvPtr2; 
			DWORD		dwBytes2; 
			HRESULT		hres;

			// 重置播放位置到开头
			hres = lpSoundData[num]->SetCurrentPosition(0);
			// 锁定整个缓冲区
			hres = lpSoundData[num]->Lock(0, DataLen, &lpvPtr1, &dwBytes1, &lpvPtr2, &dwBytes2, 0);

		    // 缓冲区丢失则恢复
		    if(DSERR_BUFFERLOST == hres) 
			{
				lpSoundData[num]->Restore();
				hres = lpSoundData[num]->Lock(0, DataLen, &lpvPtr1, &dwBytes1, &lpvPtr2, &dwBytes2, 0);
			}

			// 读取文件数据到缓冲区
			if(DS_OK == hres) 
			{
				fread(lpvPtr1, DataLen, 1, fp);
				hres = lpSoundData[num]->Unlock(lpvPtr1, dwBytes1, lpvPtr2, dwBytes2);
			}
		}
	}
	fclose(fp);
}

// -------------------------------------
// 声音播放控制函数（可在此扩展）
// -------------------------------------
