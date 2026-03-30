////////////////////////////////////////////////
//
//  标准视频播放头文件: StdVideo.h
//
//  提供基于 Video for Windows (VFW) 的 AVI 视频播放功能。
//  支持视频解码、音频同步播放，以及将视频帧渲染到主缓冲区。
//
//  功能特性：
//    - AVI 文件打开/关闭
//    - 视频流解码（通过 ICDecompress）
//    - 音频流播放（通过 DirectSound）
//    - 帧率同步播放
//    - 循环播放支持
//
////////////////////////////////////////////////

#include <VFW.H>        // Video for Windows 头文件

/*--------------------------------------*/
// 视频播放状态标志
/*--------------------------------------*/

BOOL			VideoPlayingFlag;   // TRUE = 视频正在播放

// 视频原始数据缓冲区和解码后数据缓冲区
DWORD			rawdata[RENDER_HEIGHT * RENDER_WIDTH];      // 压缩帧原始数据
DWORD			VideoDataBuffer[RENDER_HEIGHT * RENDER_WIDTH];  // 解码后 32位图像数据

// AVI 视频流对象和信息
PAVISTREAM 		VideoStream;        // 视频流句柄
AVISTREAMINFO	VideoStreamInfo;    // 视频流信息结构

// 视频解码器句柄
HIC				decomp;             // IC（Installable Compressor）解码器句柄

// 视频格式参数
long			fmtlen;             // 格式信息长度
long			VideoBufLen;        // 视频缓冲区大小（字节）
long			VideoWidth, VideoHeight;    // 视频尺寸
long			VideoBits;          // 视频位深度
long			VideoRate, VideoScale;      // 帧率 = Rate / Scale
long			StartVideoFrame, EndVideoFrame; // 起始/结束帧号

// 视频格式缓冲区
BYTE			VideoSrcFmt[40];    // 源视频格式（BITMAPINFOHEADER）
BYTE			VideoDstFmt[40];    // 目标格式（解码为 32位 RGB）

// AVI 音频流对象和信息
PAVISTREAM 		AudioStream;        // 音频流句柄
AVISTREAMINFO	AudioStreamInfo;    // 音频流信息结构

// 音频格式
WAVEFORMATEX 	AudioSrcFmt;        // 音频源格式
/*
    WORD  wFormatTag;       // 格式标签（PCM = 1）
	WORD  nChannels;        // 声道数
    DWORD nSamplesPerSec;   // 采样率
    DWORD nAvgBytesPerSec;  // 每秒字节数
	WORD  nBlockAlign;      // 块对齐
	WORD  wBitsPerSample;   // 位深度
*/

// 音频帧参数
long			StartAudioFrame, EndAudioFrame; // 音频起始/结束帧
long			AudioBufLen;        // 音频缓冲区大小

// DirectSound 音频缓冲区
LPDIRECTSOUNDBUFFER		lpAudioDataBuffer;  // 音频播放缓冲区

/*--------------------------------------*/

/**
 * @brief 打开并加载 AVI 文件的音频流
 *
 * 从 AVI 文件中提取音频流，创建 DirectSound 缓冲区，
 * 并将音频数据加载到缓冲区中准备播放。
 *
 * @param FileName AVI 文件路径
 */
void OpenAudioStream(char *FileName)
{
	long res;

	// 关闭已存在的音频流
	if (AudioStream)
	{
		AVIStreamRelease(AudioStream);
		AudioStream = 0;
	}

	// 打开音频流
	res = AVIStreamOpenFromFileA(&AudioStream, FileName, streamtypeAUDIO, 0, OF_READ, 0);

	if(res == 0)
	{
		// 获取格式大小
		res = AVIStreamFormatSize(AudioStream, 0, &fmtlen);
		if(res == 0 && fmtlen <= 16)
		{
			// 读取音频格式
			res = AVIStreamReadFormat(AudioStream, 0, &AudioSrcFmt, &fmtlen);
			if(res == 0)
			{
				// 仅支持 PCM 格式
				if(AudioSrcFmt.wFormatTag==WAVE_FORMAT_PCM)
				{
					StartAudioFrame = AVIStreamStart(AudioStream);
					EndAudioFrame = AVIStreamEnd(AudioStream);
				
					// 计算音频数据大小
					AudioBufLen = (EndAudioFrame - StartAudioFrame) * AudioSrcFmt.nChannels * (AudioSrcFmt.wBitsPerSample/8);

					// 创建 DirectSound 缓冲区
					RELEASE(lpAudioDataBuffer);
					if(CreateDirectSoundBuffer((LPBYTE)(&AudioSrcFmt), AudioBufLen, &lpAudioDataBuffer))
					{
						{
						    LPVOID		lpvPtr1; 
							DWORD		dwBytes1; 
							LPVOID		lpvPtr2; 
							DWORD		dwBytes2; 
							HRESULT		hres;

							// 重置播放位置并锁定缓冲区
							hres = lpAudioDataBuffer->SetCurrentPosition(0);
							hres = lpAudioDataBuffer->Lock(0, AudioBufLen, &lpvPtr1, &dwBytes1, &lpvPtr2, &dwBytes2, 0);

						    // 缓冲区丢失则恢复
						    if(DSERR_BUFFERLOST == hres) 
							{
								lpAudioDataBuffer->Restore();
								hres = lpAudioDataBuffer->Lock(0, AudioBufLen, &lpvPtr1, &dwBytes1, &lpvPtr2, &dwBytes2, 0);
							}

							// 读取音频数据到缓冲区
							if(DS_OK == hres) 
							{
								res = AVIStreamRead(AudioStream, StartAudioFrame, EndAudioFrame - StartAudioFrame, lpvPtr1, AudioBufLen, 0, 0);
								hres = lpAudioDataBuffer->Unlock(lpvPtr1, dwBytes1, lpvPtr2, dwBytes2);
							}
						}
					}
				}
			}
		}
	}
	// 释放音频流（数据已拷贝到 DirectSound 缓冲区）
	if (AudioStream)
	{
		AVIStreamRelease(AudioStream);
		AudioStream = 0;
	}
}

/*--------------------------------------*/

/**
 * @brief 打开 AVI 视频流
 *
 * 打开 AVI 文件的视频流，获取视频格式信息，
 * 创建视频解码器（ICDecompress）。
 *
 * @param FileName AVI 文件路径
 * @return TRUE 成功，FALSE 失败
 */
int OpenVideoStream(char *FileName)
{	
	long res;

	// 格式指针
	LPBITMAPINFOHEADER	lpsrcfmt = (LPBITMAPINFOHEADER)VideoSrcFmt;
	LPBITMAPINFOHEADER	lpdstfmt = (LPBITMAPINFOHEADER)VideoDstFmt;

	// 关闭已存在的视频流
	if (VideoStream)
	{
		AVIStreamRelease(VideoStream);
		VideoStream = 0;
	}

	// 打开视频流
	res = AVIStreamOpenFromFileA(&VideoStream, FileName, streamtypeVIDEO, 0, OF_READ, 0);

	if(res == 0)
	{
		// 获取格式大小
		res = AVIStreamFormatSize(VideoStream, 0, &fmtlen);
		if(res == 0 && fmtlen <= 40)
		{
			// 读取视频格式
			res = AVIStreamReadFormat(VideoStream, 0, VideoSrcFmt, &fmtlen);
			if(res == 0)
			{
				// 获取视频尺寸
				VideoWidth = lpsrcfmt->biWidth;
				VideoHeight = lpsrcfmt->biHeight;
				VideoBits = lpsrcfmt->biBitCount;

				// 获取帧范围
				StartVideoFrame = AVIStreamStart(VideoStream);
				EndVideoFrame = AVIStreamEnd(VideoStream);

				// 设置目标格式为 32位 RGB
				memcpy(VideoDstFmt, VideoSrcFmt, fmtlen);
				lpdstfmt->biBitCount = 32;
				lpdstfmt->biCompression = BI_RGB;
				lpdstfmt->biSizeImage = VideoWidth * VideoHeight * 4;

				// 检查视频尺寸是否适合渲染缓冲区
				if(VideoWidth <= RENDER_WIDTH && VideoHeight <= RENDER_HEIGHT)
				{
					// 获取流信息
					res = AVIStreamInfo(VideoStream, &VideoStreamInfo, sizeof(VideoStreamInfo));
					if(res == 0)
					{
						VideoBufLen = lpdstfmt->biSizeImage;
						VideoRate = VideoStreamInfo.dwRate;
						VideoScale = VideoStreamInfo.dwScale;

						// 关闭已存在的解码器
						if(decomp)
						{
							ICClose(decomp);
						}
						// 创建视频解码器
						decomp = ICDecompressOpen(ICTYPE_VIDEO, VideoStreamInfo.fccHandler, lpsrcfmt, lpdstfmt);
						if(decomp!=0)
						{
							return TRUE;
						}
					}
				}
			}
		}
	}
	return FALSE;
}

/*--------------------------------------*/

/**
 * @brief 加载指定帧号的视频帧
 *
 * 从视频流中读取指定帧的压缩数据，解码为 32位 RGB 图像，
 * 存储到 VideoDataBuffer 中。
 *
 * @param num 帧号（从 StartVideoFrame 开始）
 */
void LoadVideoFrame(long num)
{
	long res;
	LPBITMAPINFOHEADER	lpsrcfmt = (LPBITMAPINFOHEADER)VideoSrcFmt;
	LPBITMAPINFOHEADER	lpdstfmt = (LPBITMAPINFOHEADER)VideoDstFmt;

	// 读取压缩帧数据
	res = AVIStreamRead(VideoStream, num, 1, rawdata, VideoBufLen, 0, 0);
	// 解码为 32位 RGB
	ICDecompress(decomp, 0, lpsrcfmt, rawdata, lpdstfmt, VideoDataBuffer);
}

/*--------------------------------------*/

/**
 * @brief 打开 AVI 文件（视频+音频）
 *
 * 同时打开视频流和音频流，准备播放。
 *
 * @param FileName AVI 文件路径
 * @return TRUE 成功，FALSE 失败
 */
int OpenVideoFile(char *FileName)
{
	if(OpenVideoStream(FileName))
	{
		OpenAudioStream(FileName);
		return TRUE;
	}
	return FALSE;
}

/*--------------------------------------*/

/**
 * @brief 关闭 AVI 文件并释放资源
 *
 * 释放音频缓冲区、关闭解码器、释放音视频流。
 */
void CloseVideoFile(void)
{	
	// 释放音频缓冲区
	RELEASE(lpAudioDataBuffer);

	// 关闭解码器
	if(decomp)
	{
		ICClose(decomp);
		decomp = NULL;
	}

	// 释放音频流
	if(AudioStream)
	{
		AVIStreamRelease(AudioStream);
		AudioStream = NULL;
	}

	// 释放视频流
	if(VideoStream)
	{
		AVIStreamRelease(VideoStream);
		VideoStream = NULL;
	}
}

/*--------------------------------------*/

/**
 * @brief 将视频帧翻转并拷贝到渲染缓冲区
 *
 * AVI 视频帧存储是上下颠倒的（DIB 格式），
 * 此函数将 VideoDataBuffer 中的帧垂直翻转后，
 * 居中拷贝到主渲染缓冲区。
 *
 * 使用汇编优化进行高速内存拷贝。
 */
void FilpWholeVideoBuffer(void)		// 翻转视频图像
{
	// 源指针指向最后一行（从底部开始）
	LPDWORD		lpSrc = (LPDWORD)VideoDataBuffer + VideoWidth * (VideoHeight - 1);
	// 水平居中的行偏移
	DWORD		LineOff = (RENDER_WIDTH - VideoWidth) / 2 * 4;
	// 源行跨度（向上移动需要减去两行）
	DWORD		LineOff2 = VideoWidth * 4 * 2;
	// 垂直居中的起始偏移
	DWORD		HighOff = (RENDER_HEIGHT - VideoHeight) * RENDER_WIDTH / 2 * 4;

	_asm {
		MOV		ESI, lpSrc
		MOV		EDI, lpRenderBuffer
		ADD		EDI, HighOff

		MOV		EDX,VideoHeight

ExpandVideoBuffer_LoopY:
		ADD		EDI, LineOff      // 跳过左侧空白

		MOV		ECX, VideoWidth
		REP		MOVSD           // 拷贝一行

		ADD		EDI, LineOff      // 跳过右侧空白

		SUB		ESI,LineOff2    // 源指针向上移动两行（因为 MOVSD 已自动 +4）

		DEC		EDX
		JNZ		ExpandVideoBuffer_LoopY
	}
}

/*--------------------------------------*/

// 前置声明（在 Refresh.H 中定义）
void RefreshScreen(void);

/**
 * @brief 播放 AVI 视频文件（单次播放）
 *
 * 打开 AVI 文件，按帧率同步播放视频和音频，
 * 直到播放结束或用户中断（ESC 键）。
 *
 * @param FileName AVI 文件路径
 */
void PlayAVIFile(char *FileName)
{
	long		frame;
	long		time1, time2, time3;
    MSG			msg;
	double		temp;

	if(OpenVideoFile(FileName)==FALSE) return;

	frame = StartVideoFrame;

	ClearRenderBuffer(0);

	time1 = timeGetTime();
	time2 = time1;
	time3 = 0;

	VideoPlayingFlag = TRUE;

	do
	{
        // 处理 Windows 消息
        if(PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
        {
            if (!GetMessage(&msg, NULL, 0, 0)) return;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
		else
		{
	        if(bActive)
		    {
				// 恢复暂停时的时间
				if(time3 > 0)
				{
					time2 += time3;
					time3 = 0;
				}

				time1 = timeGetTime();
				// 计算当前应该播放的帧
				temp= (frame - StartVideoFrame) * VideoScale * 1000.0 / VideoRate;
				if((time1 - time2) >= (long)temp)
				{
					LoadVideoFrame(frame);
					frame++;
					if(frame >= EndVideoFrame)
					{
						VideoPlayingFlag = FALSE;
					}
					// 第一帧时开始播放音频
					if(frame <= StartVideoFrame+1)
					{
						if(lpAudioDataBuffer)
							lpAudioDataBuffer->Play(NULL, NULL, 0);
					}
					FilpWholeVideoBuffer();
				}
				RefreshScreen();
			}
			else
			{
				// 窗口失活时记录时间差
				time3 = timeGetTime() - time1;
			}
		}
	}	while(VideoPlayingFlag);

	CloseVideoFile();
	// 如果程序需要退出，销毁窗口
	if(bLive == FALSE)
	{
		DestroyWindow(hWndCopy);
	}

	return;
}

/*--------------------------------------*/

/**
 * @brief 循环播放 AVI 视频文件
 *
 * 与 PlayAVIFile 类似，但播放结束后自动回到开头继续播放，
 * 直到用户中断（ESC 键）。
 *
 * @param FileName AVI 文件路径
 */
void LoopPlayAVIFile(char *FileName)
{
	long		frame;
	long		time1, time2, time3;
    MSG			msg;
	double		temp;

	if(OpenVideoFile(FileName)==FALSE) return;

	frame = StartVideoFrame;

	ClearRenderBuffer(0);

	time1 = timeGetTime();
	time2 = time1;
	time3 = 0;

	VideoPlayingFlag = TRUE;

	do
	{
        // 处理 Windows 消息
        if(PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
        {
            if (!GetMessage(&msg, NULL, 0, 0)) return;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
		else
		{
	        if(bActive)
		    {
				// 恢复暂停时的时间
				if(time3 > 0)
				{
					time2 += time3;
					time3 = 0;
				}

				time1 = timeGetTime();
				// 计算当前应该播放的帧
				temp= (frame - StartVideoFrame) * VideoScale * 1000.0 / VideoRate;
				if((time1 - time2) >= (long)temp)
				{
					LoadVideoFrame(frame);
					frame++;
					// 循环播放
					if(frame >= EndVideoFrame)
					{
						frame = StartVideoFrame;
						time1 = timeGetTime();
						time2 = time1;
						time3 = 0;

						// 重新开始音频
						if(lpAudioDataBuffer)
						{
							lpAudioDataBuffer->Stop();
							lpAudioDataBuffer->SetCurrentPosition(0);
							lpAudioDataBuffer->Play(NULL, NULL, 0);
						}
					}

					// 第一帧时开始播放音频
					if(frame <= StartVideoFrame+1)
					{
						if(lpAudioDataBuffer)
							lpAudioDataBuffer->Play(NULL, NULL, 0);
					}
					FilpWholeVideoBuffer();
				}
				RefreshScreen();
			}
			else
			{
				// 窗口失活时记录时间差
				time3 = timeGetTime() - time1;
			}
		}
	}	while(VideoPlayingFlag);

	CloseVideoFile();
	// 如果程序需要退出，销毁窗口
	if(bLive == FALSE)
	{
		DestroyWindow(hWndCopy);
	}

	return;
}

