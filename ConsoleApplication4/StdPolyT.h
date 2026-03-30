/*  StdPolyT.H  */
//
// 纹理映射多边形渲染模块 (Texture-Mapped Polygon Rendering)
//
// 本模块提供带纹理贴图的四边形（Quad）渲染功能，支持多种混合模式：
//   - 普通纹理映射 (T)
//   - 加法混合 (AD)
//   - 减法混合 (SB)
//   - Alpha 混合 (AB)
//   - 亮度调节 (BR)
//   - 透明色处理 (CK)
//
// 技术实现：
//   采用扫描线填充算法，将四边形分解为水平扫描线，
//   对每条扫描线进行纹理坐标插值（U/V 线性插值），
//   然后使用 MMX 指令进行像素级纹理采样和混合。
//
// 纹理坐标格式：
//   U/V 使用 8.24 固定点数（左移 24 位），在 HLineT 系列函数中
//   通过右移 23 位转换为 256x256 纹理的数组索引。
//
// 纹理格式：
//   支持 256x256 像素的 32 位 XRGB 纹理，
//   带透明色的纹理使用高位 (0x80000000+) 作为透明标记。
//

// -------------------------------------
//			纹理坐标插值表
// -------------------------------------

// 每行扫描线左侧的纹理 U/V 坐标
long		PolygonTextureUL[RENDER_HEIGHT];	// 左侧 U 坐标
long		PolygonTextureUR[RENDER_HEIGHT];	// 右侧 U 坐标

long		PolygonTextureVL[RENDER_HEIGHT];	// 左侧 V 坐标
long		PolygonTextureVR[RENDER_HEIGHT];	// 右侧 V 坐标

// 当前扫描线的纹理坐标和步进值
long		PolyLineU1,		PolyLineV1;		// 扫描线起点纹理坐标
long		PolyLineU2,		PolyLineV2;		// 扫描线终点纹理坐标
long		PolyLineUa = 0,	PolyLineVa = 0;	// U/V 每像素步进值（插值增量）

LPDWORD		lpTextureSource;				// 当前纹理数据源指针
LPDWORD		lpNULL;							// NULL 占位（用于 8 字节对齐）

// Alpha 混合系数（用于 MMX 乘法）
WORD		ALPHA1[4], ALPHA2[4];

// -------------------------------------
//			纹理加载函数
// -------------------------------------

/**
 * @brief 加载纹理文件并进行预处理
 *
 * 从文件加载 128x128 或 256x256 纹理，并执行以下预处理：
 *   1. 检测是否包含透明色 (CK_VALUE)
 *   2. 将 128x128 纹理扩展为 256x256（通过双线性插值）
 *   3. 对透明纹理进行特殊标记处理（高位标记法）
 *
 * 透明纹理标记系统：
 *   - 0x80000000: 完全透明 (Flag=1000)
 *   - 0x90000000: 1个邻居不透明 (Flag=1001)
 *   - 0xA0000000: 2个邻居不透明 (Flag=1010)
 *   - 0xB0000000: 3个邻居不透明 (Flag=1011)
 *
 * @param DataBuffer 纹理数据缓冲区（输出）
 * @param filename   纹理文件路径
 */
void LoadTextureFile(LPDWORD DataBuffer, LPSTR filename)
{
	long		x, y, k = 0;
	DWORD		c, c2;
	FILE		*fp;

	// 读取原始纹理数据（128x128 像素，每像素 4 字节）
	fopen_s(&fp,filename, "rb");
	fread(DataBuffer, 0x4000, 4, fp);
	fclose(fp);

	// 检测透明色并复制到 256x256 位置（先水平扩展）
	for(y=127; y>=0; y--)
	for(x=127; x>=0; x--)
	{
		c = *(DataBuffer+y*128+x);
		if(c == CK_VALUE)
		{	// 发现透明色，标记为透明纹理
			k = 1;
			c = c | 0x80000000;			// 设置透明标记位 (Flag=1000)
		}
		*(DataBuffer+y*512+x*2) = c;	// 复制到 256x256 位置（水平翻倍）
	}

	if(k == 0)
	{
		// 普通纹理（无透明色）：使用双线性插值扩展
		// 水平方向插值
		for(y=0; y<256; y+=2)
		for(x=0; x<256; x+=2)
			*(DataBuffer+y*256+x+1) = ((*(DataBuffer+y*256+x)&0xFEFEFE)+(*(DataBuffer+y*256+x+2)&0xFEFEFE))/2;

		// 垂直方向插值
		for(y=0; y<255; y+=2)
		for(x=0; x<256; x++)
			*(DataBuffer+(y+1)*256+x) = ((*(DataBuffer+y*256+x)&0xFEFEFE)+(*(DataBuffer+(y+2)*256+x)&0xFEFEFE))/2;
	}
	else
	{
		// 透明纹理：需要特殊处理透明色插值
		// 水平方向插值（带透明检测）
		for(y=0; y<256; y+=2)
		for(x=0; x<256; x+=2)
		{	// H-check: 检查水平相邻像素
			c =	*(DataBuffer+y*256+x) & 0xFFFFFF;
			c2 = *(DataBuffer+y*256+x+2) & 0xFFFFFF;
			if(c == CK_VALUE || c2 == CK_VALUE)
			{
				if(c == CK_VALUE && c2 == CK_VALUE)
				{	// 两边都透明
					*(DataBuffer+y*256+x+1) = CK_VALUE | 0x80000000;
				}
				else
				{	// 一边透明：使用平均值并标记
					if(c == CK_VALUE) c = c2;
					*(DataBuffer+y*256+x+1) = (c & 0xFEFEFE) | 0xA0000000;
				}
			}
			else
			{
				// 都不透明：正常平均
				*(DataBuffer+y*256+x+1) = ((c&0xFEFEFE)+(c2&0xFEFEFE))/2;
			}
		}

		// 垂直方向插值（带透明检测）
		for(y=0; y<255; y+=2)
		for(x=0; x<256; x+=2)
		{	// V-check: 检查垂直相邻像素
			c =	*(DataBuffer+y*256+x) & 0xFFFFFF;
			c2 = *(DataBuffer+(y+2)*256+x) & 0xFFFFFF;
			if(c == CK_VALUE || c2 == CK_VALUE)
			{
				if(c == CK_VALUE && c2 == CK_VALUE)
				{
					*(DataBuffer+(y+1)*256+x) = CK_VALUE | 0x80000000;
				}
				else
				{
					if(c == CK_VALUE) c = c2;
					*(DataBuffer+(y+1)*256+x) = (c & 0xFEFEFE) | 0xA0000000;
				}
			}
			else
			{
				*(DataBuffer+(y+1)*256+x) = ((c&0xFEFEFE)+(c2&0xFEFEFE))/2;
			}
		}

		// 对角线方向插值（中心像素）
		for(y=0; y<255; y+=2)
		for(x=0; x<255; x+=2)
		{	// HV-check: 检查四个对角邻居
			k = 0;
			c2 = 0;

			c =	*(DataBuffer+y*256+x) & 0xFFFFFF;
			if(c != CK_VALUE)
			{
				c2 += (c&0xFCFCFC);
				k++;
			}
			c =	*(DataBuffer+y*256+x+2) & 0xFFFFFF;
			if(c != CK_VALUE)
			{
				c2 += (c&0xFCFCFC);
				k++;
			}
			c =	*(DataBuffer+(y+2)*256+x+2) & 0xFFFFFF;
			if(c != CK_VALUE)
			{
				c2 += (c&0xFCFCFC);
				k++;
			}
			c =	*(DataBuffer+(y+2)*256+x) & 0xFFFFFF;
			if(c != CK_VALUE)
			{
				c2 += (c&0xFCFCFC);
				k++;
			}

			// 根据不透明邻居数量设置标记
			switch(k)
			{
				case 0:
					// 全透明
					*(DataBuffer+(y+1)*256+x+1) = CK_VALUE | 0x80000000;
					break;
				case 1:
					// 1个邻居不透明
					*(DataBuffer+(y+1)*256+x+1) = c2 | 0x90000000;
					break;
				case 2:
					// 2个邻居不透明
					*(DataBuffer+(y+1)*256+x+1) = (c2 / 2) | 0xA0000000;
					break;
				case 3:
					// 3个邻居不透明
					*(DataBuffer+(y+1)*256+x+1) = c2 | 0xB0000000;
					break;
				case 4:
					// 全不透明：正常平均
					*(DataBuffer+(y+1)*256+x+1) = c2 / 4;
					break;
			}
		}
	}
}

// -------------------------------------
//			边扫描（带 UV 插值）
// -------------------------------------

/**
 * @brief 扫描边并插值纹理坐标
 *
 * 使用 Bresenham 算法扫描从 (X1,Y1) 到 (X2,Y2) 的边，
 * 同时线性插值计算纹理坐标 (U,V)。
 *
 * 将结果写入 PolygonSideL/R 和 PolygonTextureUL/UR/VL/VR 数组。
 *
 * @param X1,Y1 边起点坐标
 * @param X2,Y2 边终点坐标
 * @param U1,V1 起点纹理坐标
 * @param U2,V2 终点纹理坐标
 */
void ScanEdgeUV(
		long X1, long Y1,
		long X2, long Y2,
		long U1, long V1, 
		long U2, long V2
	)
{
	long	deltax, deltay, halfx, halfy, dotn;
	long	x, y, dirx, diry, b;
	long	Ua;
	long	Va;

	// 水平边特殊处理
	if(Y1==Y2 && Y1>=YMIN && Y1<=YMAX)
	{
		if(X1<PolygonSideL[Y1])
		{
			PolygonSideL[Y1]=X1;
			PolygonTextureUL[Y1]=U1;
			PolygonTextureVL[Y1]=V1;
		}
		if(X1>PolygonSideR[Y1])
		{
			PolygonSideR[Y1]=X1;
			PolygonTextureUR[Y1]=U1;
			PolygonTextureVR[Y1]=V1;
		}
		if(X2<PolygonSideL[Y1])
		{
			PolygonSideL[Y1]=X2;
			PolygonTextureUL[Y1]=U2;
			PolygonTextureVL[Y1]=V2;
		}
		if(X2>PolygonSideR[Y1])
		{
			PolygonSideR[Y1]=X2;
			PolygonTextureUR[Y1]=U2;
			PolygonTextureVR[Y1]=V2;
		}
		return;
	}

	// 确定扫描方向
	dirx=1;diry=1;
	if((deltax=X2-X1)<0)
	{
		deltax=-deltax;
		dirx=-1;
	}
	if((deltay=Y2-Y1)<0)
	{
		deltay=-deltay;
		diry=-1;
	}
	x=X1;y=Y1;b=0;
	if(deltax<deltay)
	{
		// 陡峭边（Y 变化更快）
		Ua=(U2-U1)/deltay;
		Va=(V2-V1)/deltay;
		halfy=deltay>>1;
		dotn=deltay;
		do
		{
			if(y>=YMIN && y<=YMAX)
			{
				if(x<PolygonSideL[y])
				{
					PolygonSideL[y]=x;
					PolygonTextureUL[y]=U1;
					PolygonTextureVL[y]=V1;
				}
			    if(x>PolygonSideR[y])
				{
					PolygonSideR[y]=x;
					PolygonTextureUR[y]=U1;
					PolygonTextureVR[y]=V1;
				}
			}
			y+=diry;
			b+=deltax;
			U1+=Ua;
			V1+=Va;
			if(b>halfy)
			{
				b-=deltay;
				x+=dirx;
			}
		} while(dotn--);
    }
	else
	{
		// 平缓边（X 变化更快）
		Ua=(U2-U1)/deltax;
		Va=(V2-V1)/deltax;
		halfx=deltax>>1;
		dotn=deltax;
		do
		{
			if(y>=YMIN && y<=YMAX)
			{
				if(x<PolygonSideL[y])
				{
					PolygonSideL[y]=x;
					PolygonTextureUL[y]=U1;
					PolygonTextureVL[y]=V1;
				}
			    if(x>PolygonSideR[y])
				{
					PolygonSideR[y]=x;
					PolygonTextureUR[y]=U1;
					PolygonTextureVR[y]=V1;
				}
			}
			x+=dirx;
			b+=deltay;
			U1+=Ua;
			V1+=Va;
			if(b>halfx)
			{
				b-=deltax;
				y+=diry;
			}
		}	while(dotn--);
	}
}

// -------------------------------------
//			纹理水平线绘制
// -------------------------------------

/**
 * @brief 普通纹理映射水平线
 *
 * 绘制一条带纹理映射的水平扫描线。
 * 使用固定点数进行纹理坐标插值，从纹理中采样像素。
 *
 * 纹理坐标转换：
 *   U 右移 23 位得到 0-255 的纹理 X 坐标
 *   V 右移 13 位并与 0xFC00 与操作得到纹理行偏移
 */
void HLineT(void)
{
	_asm {
		MOV		ECX, PolyLineXR
		SUB		ECX, PolyLineXL

		// 如果 DeltaX==0，跳过插值计算
		JZ		Texel_L_equal_R

		// 计算 U 步进值：Ua = (U2-U1) / DeltaX
		MOV		EAX, PolyLineU2
		SUB		EAX, PolyLineU1
		CDQ
		IDIV	ECX
		MOV		PolyLineUa, EAX

		// 计算 V 步进值：Va = (V2-V1) / DeltaX
		MOV		EAX, PolyLineV2
		SUB		EAX, PolyLineV1
		CDQ
		IDIV	ECX
		MOV		PolyLineVa, EAX

		// 偏移校正（用于四舍五入）
		ADD		PolyLineU1, 0x400000
		ADD		PolyLineV1, 0x400000

Texel_L_equal_R:

		MOV		EBP, PolyLineU1
		MOV		EAX, XMIN
		MOV		EBX, PolyLineV1
		CMP		EAX, PolyLineXL
		JLE		Texel_L1
		MOV		ESI, PolyLineXL		// ESI=XL, XMIN>XL 需要左裁剪
		MOV		PolyLineXL, EAX		// XL=XMIN
		SUB		EAX, ESI			// EAX=XMIN-XL
		SUB		ECX, EAX			// ECX=ECX-(XMIN-XL)
		MOV		ESI, EAX

		// 左裁剪：调整 U/V 起始值
		MOV		EAX, PolyLineUa
		IMUL	ESI
		ADD		EBP, EAX

		MOV		EAX, PolyLineVa
		IMUL	ESI
		ADD		EBX, EAX
						
Texel_L1:

		// 右裁剪
		MOV		EAX, XMAX
		SUB		EAX, PolyLineXR		// EAX=XMAX-XR
		JGE		Texel_L2
		ADD		ECX, EAX			// EAX<0, ECX=ECX-|EAX|

Texel_L2:

		// 设置目标指针
		MOV		EAX, PolyLineY
		MOV		EDI, lpRenderBuffer
		MOV		EAX, [LineStartOffset + EAX * 4]
		ADD		EDI, EAX
		MOV		EAX, PolyLineXL
		LEA		EDI, [EDI + EAX * 4]
		INC		ECX
		MOV		EDX, lpTextureSource

Texel_Loop:

		// 纹理采样
		MOV		EAX, EBP
		MOV		ESI, EBX
		SHR		EAX, 23				// EAX = U >> 23 (0-255)
		ADD		EBP, PolyLineUa		// U += Ua
		SHR		ESI, 13				// ESI = V >> 13
		LEA		EAX, [EDX+EAX*4]	// EAX = 纹理行起始
		AND		SI, 0xFC00			// ESI = (V >> 13) & 0xFC00
		ADD		EBX, PolyLineVa		// V += Va
		MOV		EAX, [ESI+EAX]		// EAX = 纹理像素
		STOSD						// 存储像素

		DEC		ECX
		JNZ		Texel_Loop
	}
}

// -------------------------------------

/**
 * @brief 加法混合纹理水平线
 *
 * 在普通纹理映射基础上，使用 PADDUSB 指令将纹理像素
 * 与目标像素进行饱和加法混合。
 */
void HLineTAD(void)
{
	_asm {
		MOV		ECX, PolyLineXR
		SUB		ECX, PolyLineXL

		JZ		Texel_AD_L_equal_R

		MOV		EAX, PolyLineU2
		SUB		EAX, PolyLineU1
		CDQ
		IDIV	ECX
		MOV		PolyLineUa, EAX

		MOV		EAX, PolyLineV2
		SUB		EAX, PolyLineV1
		CDQ
		IDIV	ECX
		MOV		PolyLineVa, EAX

		ADD		PolyLineU1, 0x400000
		ADD		PolyLineV1, 0x400000

Texel_AD_L_equal_R:

		MOV		EBP, PolyLineU1
		MOV		EAX, XMIN
		MOV		EBX, PolyLineV1
		CMP		EAX, PolyLineXL
		JLE		Texel_AD_L1
		MOV		ESI, PolyLineXL
		MOV		PolyLineXL, EAX
		SUB		EAX, ESI
		SUB		ECX, EAX
		MOV		ESI, EAX

		MOV		EAX, PolyLineUa
		IMUL	ESI
		ADD		EBP, EAX

		MOV		EAX, PolyLineVa
		IMUL	ESI
		ADD		EBX, EAX
						
Texel_AD_L1:

		MOV		EAX, XMAX
		SUB		EAX, PolyLineXR
		JGE		Texel_AD_L2
		ADD		ECX, EAX

Texel_AD_L2:

		MOV		EAX, PolyLineY
		MOV		EDI, lpRenderBuffer
		MOV		EAX, [LineStartOffset + EAX * 4]
		ADD		EDI, EAX
		MOV		EAX, PolyLineXL
		LEA		EDI, [EDI + EAX * 4]
		INC		ECX
		MOV		EDX, lpTextureSource

Texel_AD_Loop:

		MOV		EAX, EBP
		MOV		ESI, EBX
		SHR		EAX, 23
		ADD		EBP, PolyLineUa
		SHR		ESI, 13
		LEA		EAX, [EDX+EAX*4]
		AND		SI, 0xFC00
		ADD		EBX, PolyLineVa
		MOVD		MM1, [EDI]			// 目标像素
		MOVD		MM0, [ESI+EAX]		// 纹理像素
		PADDUSB		MM0, MM1			// 饱和加法
		MOVD		EAX, MM0
		STOSD

		DEC		ECX
		JNZ		Texel_AD_Loop
		EMMS
	}
}

// -------------------------------------

/**
 * @brief 减法混合纹理水平线
 *
 * 使用 PSUBUSB 指令将目标像素减去纹理像素（饱和到 0）。
 */
void HLineTSB(void)
{
	_asm {
		MOV		ECX, PolyLineXR
		SUB		ECX, PolyLineXL

		JZ		Texel_SB_L_equal_R

		MOV		EAX, PolyLineU2
		SUB		EAX, PolyLineU1
		CDQ
		IDIV	ECX
		MOV		PolyLineUa, EAX

		MOV		EAX, PolyLineV2
		SUB		EAX, PolyLineV1
		CDQ
		IDIV	ECX
		MOV		PolyLineVa, EAX

		ADD		PolyLineU1, 0x400000
		ADD		PolyLineV1, 0x400000

Texel_SB_L_equal_R:

		MOV		EBP, PolyLineU1
		MOV		EAX, XMIN
		MOV		EBX, PolyLineV1
		CMP		EAX, PolyLineXL
		JLE		Texel_SB_L1
		MOV		ESI, PolyLineXL
		MOV		PolyLineXL, EAX
		SUB		EAX, ESI
		SUB		ECX, EAX
		MOV		ESI, EAX

		MOV		EAX, PolyLineUa
		IMUL	ESI
		ADD		EBP, EAX

		MOV		EAX, PolyLineVa
		IMUL	ESI
		ADD		EBX, EAX
						
Texel_SB_L1:

		MOV		EAX, XMAX
		SUB		EAX, PolyLineXR
		JGE		Texel_SB_L2
		ADD		ECX, EAX

Texel_SB_L2:

		MOV		EAX, PolyLineY
		MOV		EDI, lpRenderBuffer
		MOV		EAX, [LineStartOffset + EAX * 4]
		ADD		EDI, EAX
		MOV		EAX, PolyLineXL
		LEA		EDI, [EDI + EAX * 4]
		INC		ECX
		MOV		EDX, lpTextureSource

Texel_SB_Loop:

		MOV		EAX, EBP
		MOV		ESI, EBX
		SHR		EAX, 23
		ADD		EBP, PolyLineUa
		SHR		ESI, 13
		LEA		EAX, [EDX+EAX*4]
		AND		SI, 0xFC00
		ADD		EBX, PolyLineVa
		MOVD		MM1, [EDI]			// 目标像素
		MOVD		MM0, [ESI+EAX]		// 纹理像素
		PSUBUSB		MM1, MM0			// 饱和减法
		MOVD		EAX, MM1
		STOSD

		DEC		ECX
		JNZ		Texel_SB_Loop
		EMMS
	}
}

// -------------------------------------

/**
 * @brief Alpha 混合纹理水平线
 *
 * 使用 MMX 进行 Alpha 混合：
 *   结果 = 纹理 * Alpha/256 + 目标 * (256-Alpha)/256
 */
void HLineTAB(void)
{
	_asm {
		MOV		ECX, PolyLineXR
		SUB		ECX, PolyLineXL

		JZ		Texel_AB_L_equal_R

		MOV		EAX, PolyLineU2
		SUB		EAX, PolyLineU1
		CDQ
		IDIV	ECX
		MOV		PolyLineUa, EAX

		MOV		EAX, PolyLineV2
		SUB		EAX, PolyLineV1
		CDQ
		IDIV	ECX
		MOV		PolyLineVa, EAX

		ADD		PolyLineU1, 0x400000
		ADD		PolyLineV1, 0x400000

Texel_AB_L_equal_R:

		MOV		EBP, PolyLineU1
		MOV		EAX, XMIN
		MOV		EBX, PolyLineV1
		CMP		EAX, PolyLineXL
		JLE		Texel_AB_L1
		MOV		ESI, PolyLineXL
		MOV		PolyLineXL, EAX
		SUB		EAX, ESI
		SUB		ECX, EAX
		MOV		ESI, EAX

		MOV		EAX, PolyLineUa
		IMUL	ESI
		ADD		EBP, EAX

		MOV		EAX, PolyLineVa
		IMUL	ESI
		ADD		EBX, EAX
						
Texel_AB_L1:

		MOV		EAX, XMAX
		SUB		EAX, PolyLineXR
		JGE		Texel_AB_L2
		ADD		ECX, EAX

Texel_AB_L2:

		MOV		EAX, PolyLineY
		MOV		EDI, lpRenderBuffer
		MOV		EAX, [LineStartOffset + EAX * 4]
		ADD		EDI, EAX
		MOV		EAX, PolyLineXL
		LEA		EDI, [EDI + EAX * 4]

		MOVQ		MM3, [ALPHA1]		// MM3 = Alpha
		MOVQ		MM4, [ALPHA2]		// MM4 = 256 - Alpha

		INC		ECX
		MOV		EDX, lpTextureSource

Texel_AB_Loop:

		MOV		EAX, EBP
		MOV		ESI, EBX
		SHR		EAX, 23
		ADD		EBP, PolyLineUa
		SHR		ESI, 13
		LEA		EAX, [EDX+EAX*4]
		AND		SI, 0xFC00
		ADD		EBX, PolyLineVa

		MOVD		MM1, [ESI+EAX]		// MM1 = 纹理像素
		MOVD		MM2, [EDI]			// MM2 = 目标像素

		PXOR		MM0, MM0			// MM0 = 0

		PUNPCKLBW	MM1, MM0			// 解包纹理像素
		PUNPCKLBW	MM2, MM0			// 解包目标像素

		PMULLW		MM1, MM3
		PMULLW		MM2, MM4

		PADDUSW		MM1, MM2
		PSRLW		MM1, 8

		PACKUSWB	MM1, MM0
		MOVD		EAX, MM1

		STOSD

		DEC		ECX
		JNZ		Texel_AB_Loop
		EMMS
	}
}

// -------------------------------------

/**
 * @brief 亮度调节纹理水平线
 *
 * 使用 MMX 将纹理像素乘以亮度系数（Bright/256）。
 */
void HLineTBR(void)
{
	_asm {
		MOV		ECX, PolyLineXR
		SUB		ECX, PolyLineXL

		JZ		Texel_BR_L_equal_R

		MOV		EAX, PolyLineU2
		SUB		EAX, PolyLineU1
		CDQ
		IDIV	ECX
		MOV		PolyLineUa, EAX

		MOV		EAX, PolyLineV2
		SUB		EAX, PolyLineV1
		CDQ
		IDIV	ECX
		MOV		PolyLineVa, EAX

		ADD		PolyLineU1, 0x400000
		ADD		PolyLineV1, 0x400000

Texel_BR_L_equal_R:

		MOV		EBP, PolyLineU1
		MOV		EAX, XMIN
		MOV		EBX, PolyLineV1
		CMP		EAX, PolyLineXL
		JLE		Texel_BR_L1
		MOV		ESI, PolyLineXL
		MOV		PolyLineXL, EAX
		SUB		EAX, ESI
		SUB		ECX, EAX
		MOV		ESI, EAX

		MOV		EAX, PolyLineUa
		IMUL	ESI
		ADD		EBP, EAX

		MOV		EAX, PolyLineVa
		IMUL	ESI
		ADD		EBX, EAX
						
Texel_BR_L1:

		MOV		EAX, XMAX
		SUB		EAX, PolyLineXR
		JGE		Texel_BR_L2
		ADD		ECX, EAX

Texel_BR_L2:

		MOV		EAX, PolyLineY
		MOV		EDI, lpRenderBuffer
		MOV		EAX, [LineStartOffset + EAX * 4]
		ADD		EDI, EAX
		MOV		EAX, PolyLineXL
		LEA		EDI, [EDI + EAX * 4]

		MOVQ		MM3, [ALPHA1]		// MM3 = Bright 系数

		INC		ECX
		MOV		EDX, lpTextureSource

Texel_BR_Loop:

		MOV		EAX, EBP
		MOV		ESI, EBX
		SHR		EAX, 23
		ADD		EBP, PolyLineUa
		SHR		ESI, 13
		LEA		EAX, [EDX+EAX*4]
		AND		SI, 0xFC00
		ADD		EBX, PolyLineVa

		MOVD		MM1, [ESI+EAX]		// MM1 = 纹理像素

		PXOR		MM0, MM0			// MM0 = 0

		PUNPCKLBW	MM1, MM0			// 解包

		PMULLW		MM1, MM3

		PSRLW		MM1, 8

		PACKUSWB	MM1, MM0
		MOVD		EAX, MM1

		STOSD

		DEC		ECX
		JNZ		Texel_BR_Loop
		EMMS
	}
}

// -------------------------------------
//			带透明色的纹理水平线
// -------------------------------------

/**
 * @brief 带透明色的普通纹理水平线
 *
 * 检测纹理像素的高位标记，根据透明标记类型决定如何绘制：
 *   - < 0x80000000: 正常绘制
 *   - 0x80000000+: 跳过（完全透明）
 *   - 0x90000000+: 与背景 3:1 混合
 *   - 0xA0000000+: 与背景 1:1 混合
 *   - 0xB0000000+: 与背景 1:3 混合
 */
void HLineTCK(void)
{
	_asm {
		MOV		ECX, PolyLineXR
		SUB		ECX, PolyLineXL

		JZ		Texel_CK_L_equal_R

		MOV		EAX, PolyLineU2
		SUB		EAX, PolyLineU1
		CDQ
		IDIV	ECX
		MOV		PolyLineUa, EAX

		MOV		EAX, PolyLineV2
		SUB		EAX, PolyLineV1
		CDQ
		IDIV	ECX
		MOV		PolyLineVa, EAX

		ADD		PolyLineU1, 0x400000
		ADD		PolyLineV1, 0x400000

Texel_CK_L_equal_R:

		MOV		EBP, PolyLineU1
		MOV		EAX, XMIN
		MOV		EBX, PolyLineV1
		CMP		EAX, PolyLineXL
		JLE		Texel_CK_L1
		MOV		ESI, PolyLineXL
		MOV		PolyLineXL, EAX
		SUB		EAX, ESI
		SUB		ECX, EAX
		MOV		ESI, EAX

		MOV		EAX, PolyLineUa
		IMUL	ESI
		ADD		EBP, EAX

		MOV		EAX, PolyLineVa
		IMUL	ESI
		ADD		EBX, EAX
						
Texel_CK_L1:

		MOV		EAX, XMAX
		SUB		EAX, PolyLineXR
		JGE		Texel_CK_L2
		ADD		ECX, EAX

Texel_CK_L2:

		MOV		EAX, PolyLineY
		MOV		EDI, lpRenderBuffer
		MOV		EAX, [LineStartOffset + EAX * 4]
		ADD		EDI, EAX
		MOV		EAX, PolyLineXL
		LEA		EDI, [EDI + EAX * 4]
		INC		ECX

Texel_CK_Loop:

		MOV		EAX, EBP
		MOV		ESI, EBX
		SHR		EAX, 23
		MOV		EDX, lpTextureSource
		SHR		ESI, 13
		LEA		EAX, [EDX+EAX*4]
		AND		SI, 0xFC00
		ADD		EBP, PolyLineUa
		MOV		EAX, [ESI+EAX]
		ADD		EBX, PolyLineVa
		CMP		EAX, 0x80000000		// 检查透明标记
		JAE		Texel_CK_Skip

		STOSD

		DEC		ECX
		JNZ		Texel_CK_Loop
	}

	return;

Texel_CK_Skip:

	_asm {
		CMP		EAX, CK_VALUE | 0x80000000	// 完全透明
		JNZ		Texel_CK_K3

		ADD		EDI, 4					// 跳过

		DEC		ECX
		JNZ		Texel_CK_Loop
	}

	return;

Texel_CK_K3:

	_asm {
		CMP		EAX, 0xB0000000			// 检查 0xB 标记（3个邻居不透明）
		JB		Texel_CK_K2
		
		MOV		EDX, [EDI]
		AND		EDX, 0xFCFCFC
		ADD		EAX, EDX
		SHR		EAX, 2					// (纹理 + 背景*3) / 4
		STOSD

		DEC		ECX
		JNZ		Texel_CK_Loop
	}

	return;

Texel_CK_K2:

	_asm {
		CMP		EAX, 0xA0000000			// 检查 0xA 标记（2个邻居不透明）
		JB		Texel_CK_K1

		MOV		EDX, [EDI]
		AND		EDX, 0xFEFEFE
		ADD		EAX, EDX
		SHR		EAX, 1					// (纹理 + 背景) / 2
		STOSD

		DEC		ECX
		JNZ		Texel_CK_Loop
	}

	return;

Texel_CK_K1:

	_asm {
		MOV		EDX, [EDI]
		AND		EDX, 0xFCFCFC
		LEA		EDX, [EDX+EDX*2]		// 背景 * 3
		ADD		EAX, EDX
		SHR		EAX, 2					// (纹理 + 背景*3) / 4
		STOSD

		DEC		ECX
		JNZ		Texel_CK_Loop
	}
}

// -------------------------------------

/**
 * @brief 带透明色的加法混合纹理水平线
 */
void HLineTCKAD(void)
{
	_asm {
		MOV		ECX, PolyLineXR
		SUB		ECX, PolyLineXL

		JZ		Texel_CK_AD_L_equal_R

		MOV		EAX, PolyLineU2
		SUB		EAX, PolyLineU1
		CDQ
		IDIV	ECX
		MOV		PolyLineUa, EAX

		MOV		EAX, PolyLineV2
		SUB		EAX, PolyLineV1
		CDQ
		IDIV	ECX
		MOV		PolyLineVa, EAX

		ADD		PolyLineU1, 0x400000
		ADD		PolyLineV1, 0x400000

Texel_CK_AD_L_equal_R:

		MOV		EBP, PolyLineU1
		MOV		EAX, XMIN
		MOV		EBX, PolyLineV1
		CMP		EAX, PolyLineXL
		JLE		Texel_CK_AD_L1
		MOV		ESI, PolyLineXL
		MOV		PolyLineXL, EAX
		SUB		EAX, ESI
		SUB		ECX, EAX
		MOV		ESI, EAX

		MOV		EAX, PolyLineUa
		IMUL	ESI
		ADD		EBP, EAX

		MOV		EAX, PolyLineVa
		IMUL	ESI
		ADD		EBX, EAX
						
Texel_CK_AD_L1:

		MOV		EAX, XMAX
		SUB		EAX, PolyLineXR
		JGE		Texel_CK_AD_L2
		ADD		ECX, EAX

Texel_CK_AD_L2:

		MOV		EAX, PolyLineY
		MOV		EDI, lpRenderBuffer
		MOV		EAX, [LineStartOffset + EAX * 4]
		ADD		EDI, EAX
		MOV		EAX, PolyLineXL
		LEA		EDI, [EDI + EAX * 4]
		INC		ECX

Texel_CK_AD_Loop:

		MOV		EAX, EBP
		MOV		ESI, EBX
		SHR		EAX, 23
		MOV		EDX, lpTextureSource
		SHR		ESI, 13
		LEA		EAX, [EDX+EAX*4]
		AND		SI, 0xFC00
		ADD		EBP, PolyLineUa
		MOV		EAX, [ESI+EAX]
		ADD		EBX, PolyLineVa
		CMP		EAX, 0x80000000
		JAE		Texel_CK_AD_Skip

		MOVD		MM0, EAX
		MOVD		MM1, [EDI]
		PADDUSB		MM0, MM1
		MOVD		EAX, MM0
		STOSD

		DEC		ECX
		JNZ		Texel_CK_AD_Loop
		EMMS
	}

	return;

Texel_CK_AD_Skip:

	_asm {
		CMP		EAX, CK_VALUE | 0x80000000
		JNZ		Texel_CK_AD_K3

		ADD		EDI, 4

		DEC		ECX
		JNZ		Texel_CK_AD_Loop
		EMMS
	}

	return;

Texel_CK_AD_K3:

	_asm {
		CMP		EAX, 0xB0000000
		JB		Texel_CK_AD_K2

		SHR		EAX, 2

		MOVD		MM0, EAX
		MOVD		MM1, [EDI]
		PADDUSB		MM0, MM1
		MOVD		EAX, MM0
		STOSD

		DEC		ECX
		JNZ		Texel_CK_AD_Loop
		EMMS
	}

	return;

Texel_CK_AD_K2:

	_asm {
		CMP		EAX, 0xA0000000
		JB		Texel_CK_AD_K1

		SHR		EAX, 1

		MOVD		MM0, EAX
		MOVD		MM1, [EDI]
		PADDUSB		MM0, MM1
		MOVD		EAX, MM0
		STOSD

		DEC		ECX
		JNZ		Texel_CK_AD_Loop
		EMMS
	}

	return;

Texel_CK_AD_K1:

	_asm {
		SHR		EAX, 2

		MOVD		MM0, EAX
		MOVD		MM1, [EDI]
		PADDUSB		MM0, MM1
		MOVD		EAX, MM0

		STOSD

		DEC		ECX
		JNZ		Texel_CK_AD_Loop
		EMMS
	}
}

// -------------------------------------

/**
 * @brief 带透明色的减法混合纹理水平线
 */
void HLineTCKSB(void)
{
	_asm {
		MOV		ECX, PolyLineXR
		SUB		ECX, PolyLineXL

		JZ		Texel_CK_SB_L_equal_R

		MOV		EAX, PolyLineU2
		SUB		EAX, PolyLineU1
		CDQ
		IDIV	ECX
		MOV		PolyLineUa, EAX

		MOV		EAX, PolyLineV2
		SUB		EAX, PolyLineV1
		CDQ
		IDIV	ECX
		MOV		PolyLineVa, EAX

		ADD		PolyLineU1, 0x400000
		ADD		PolyLineV1, 0x400000

Texel_CK_SB_L_equal_R:

		MOV		EBP, PolyLineU1
		MOV		EAX, XMIN
		MOV		EBX, PolyLineV1
		CMP		EAX, PolyLineXL
		JLE		Texel_CK_SB_L1
		MOV		ESI, PolyLineXL
		MOV		PolyLineXL, EAX
		SUB		EAX, ESI
		SUB		ECX, EAX
		MOV		ESI, EAX

		MOV		EAX, PolyLineUa
		IMUL	ESI
		ADD		EBP, EAX

		MOV		EAX, PolyLineVa
		IMUL	ESI
		ADD		EBX, EAX
						
Texel_CK_SB_L1:

		MOV		EAX, XMAX
		SUB		EAX, PolyLineXR
		JGE		Texel_CK_SB_L2
		ADD		ECX, EAX

Texel_CK_SB_L2:

		MOV		EAX, PolyLineY
		MOV		EDI, lpRenderBuffer
		MOV		EAX, [LineStartOffset + EAX * 4]
		ADD		EDI, EAX
		MOV		EAX, PolyLineXL
		LEA		EDI, [EDI + EAX * 4]
		INC		ECX

Texel_CK_SB_Loop:

		MOV		EAX, EBP
		MOV		ESI, EBX
		SHR		EAX, 23
		MOV		EDX, lpTextureSource
		SHR		ESI, 13
		LEA		EAX, [EDX+EAX*4]
		AND		SI, 0xFC00
		ADD		EBP, PolyLineUa
		MOV		EAX, [ESI+EAX]
		ADD		EBX, PolyLineVa
		CMP		EAX, 0x80000000
		JAE		Texel_CK_SB_Skip

		MOVD		MM0, EAX
		MOVD		MM1, [EDI]
		PSUBUSB		MM1, MM0
		MOVD		EAX, MM1
		STOSD

		DEC		ECX
		JNZ		Texel_CK_SB_Loop
		EMMS
	}

	return;

Texel_CK_SB_Skip:

	_asm {
		CMP		EAX, CK_VALUE | 0x80000000
		JNZ		Texel_CK_SB_K3

		ADD		EDI, 4

		DEC		ECX
		JNZ		Texel_CK_SB_Loop
		EMMS
	}

	return;

Texel_CK_SB_K3:

	_asm {
		CMP		EAX, 0xB0000000
		JB		Texel_CK_SB_K2

		SHR		EAX, 2

		MOVD		MM0, EAX
		MOVD		MM1, [EDI]
		PSUBUSB		MM1, MM0
		MOVD		EAX, MM1
		STOSD

		DEC		ECX
		JNZ		Texel_CK_SB_Loop
		EMMS
	}

	return;

Texel_CK_SB_K2:

	_asm {
		CMP		EAX, 0xA0000000
		JB		Texel_CK_SB_K1

		SHR		EAX, 1

		MOVD		MM0, EAX
		MOVD		MM1, [EDI]
		PSUBUSB		MM1, MM0
		MOVD		EAX, MM1
		STOSD

		DEC		ECX
		JNZ		Texel_CK_SB_Loop
		EMMS
	}

	return;

Texel_CK_SB_K1:

	_asm {
		SHR		EAX, 2

		MOVD		MM0, EAX
		MOVD		MM1, [EDI]
		PSUBUSB		MM1, MM0
		MOVD		EAX, MM1

		STOSD

		DEC		ECX
		JNZ		Texel_CK_SB_Loop
		EMMS
	}
}

// -------------------------------------

/**
 * @brief 带透明色的 Alpha 混合纹理水平线
 */
void HLineTCKAB(void)
{
	_asm {
		MOV		ECX, PolyLineXR
		SUB		ECX, PolyLineXL

		JZ		Texel_CK_AB_L_equal_R

		MOV		EAX, PolyLineU2
		SUB		EAX, PolyLineU1
		CDQ
		IDIV	ECX
		MOV		PolyLineUa, EAX

		MOV		EAX, PolyLineV2
		SUB		EAX, PolyLineV1
		CDQ
		IDIV	ECX
		MOV		PolyLineVa, EAX

		ADD		PolyLineU1, 0x400000
		ADD		PolyLineV1, 0x400000

Texel_CK_AB_L_equal_R:

		MOV		EBP, PolyLineU1
		MOV		EAX, XMIN
		MOV		EBX, PolyLineV1
		CMP		EAX, PolyLineXL
		JLE		Texel_CK_AB_L1
		MOV		ESI, PolyLineXL
		MOV		PolyLineXL, EAX
		SUB		EAX, ESI
		SUB		ECX, EAX
		MOV		ESI, EAX

		MOV		EAX, PolyLineUa
		IMUL	ESI
		ADD		EBP, EAX

		MOV		EAX, PolyLineVa
		IMUL	ESI
		ADD		EBX, EAX
						
Texel_CK_AB_L1:

		MOV		EAX, XMAX
		SUB		EAX, PolyLineXR
		JGE		Texel_CK_AB_L2
		ADD		ECX, EAX

Texel_CK_AB_L2:

		MOV		EAX, PolyLineY
		MOV		EDI, lpRenderBuffer
		MOV		EAX, [LineStartOffset + EAX * 4]
		ADD		EDI, EAX
		MOV		EAX, PolyLineXL

		MOVQ		MM3, [ALPHA1]		// MM3 = Alpha
		MOVQ		MM4, [ALPHA2]		// MM4 = 256 - Alpha

		INC		ECX
		LEA		EDI, [EDI + EAX * 4]

Texel_CK_AB_Loop:

		MOV		EAX, EBP
		MOV		ESI, EBX
		SHR		EAX, 23
		MOV		EDX, lpTextureSource
		SHR		ESI, 13
		LEA		EAX, [EDX+EAX*4]
		AND		SI, 0xFC00
		ADD		EBP, PolyLineUa
		MOV		EAX, [ESI+EAX]
		ADD		EBX, PolyLineVa
		CMP		EAX, 0x80000000
		JAE		Texel_CK_AB_Skip

		MOVD		MM1, EAX			// MM1 = 纹理像素
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
		JNZ		Texel_CK_AB_Loop
		EMMS
	}

	return;

Texel_CK_AB_Skip:

	_asm {
		CMP		EAX, CK_VALUE | 0x80000000
		JNZ		Texel_CK_AB_K3

		ADD		EDI, 4

		DEC		ECX
		JNZ		Texel_CK_AB_Loop
		EMMS
	}

	return;

Texel_CK_AB_K3:

	_asm {
		CMP		EAX, 0xB0000000
		JB		Texel_CK_AB_K2

		MOV		EDX, [EDI]
		AND		EDX, 0xFCFCFC
		ADD		EAX, EDX
		SHR		EAX, 2

		MOVD		MM1, EAX			// MM1 = EAX
		MOVD		MM2, [EDI]			// MM2 = [EDI]

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
		JNZ		Texel_CK_AB_Loop
		EMMS
	}

	return;

Texel_CK_AB_K2:

	_asm {
		CMP		EAX, 0xA0000000
		JB		Texel_CK_AB_K1

		MOV		EDX, [EDI]
		AND		EDX, 0xFEFEFE
		ADD		EAX, EDX
		SHR		EAX, 1

		MOVD		MM1, EAX
		MOVD		MM2, [EDI]

		PXOR		MM0, MM0

		PUNPCKLBW	MM1, MM0
		PUNPCKLBW	MM2, MM0

		PMULLW		MM1, MM3
		PMULLW		MM2, MM4

		PADDUSW		MM1, MM2
		PSRLW		MM1, 8

		PACKUSWB	MM1, MM0
		MOVD		EAX, MM1
		STOSD

		DEC		ECX
		JNZ		Texel_CK_AB_Loop
		EMMS
	}

	return;

Texel_CK_AB_K1:

	_asm {
		MOV		EDX, [EDI]
		AND		EDX, 0xFCFCFC
		LEA		EDX, [EDX+EDX*2]
		ADD		EAX, EDX
		SHR		EAX, 2

		MOVD		MM1, EAX
		MOVD		MM2, [EDI]

		PXOR		MM0, MM0

		PUNPCKLBW	MM1, MM0
		PUNPCKLBW	MM2, MM0

		PMULLW		MM1, MM3
		PMULLW		MM2, MM4

		PADDUSW		MM1, MM2
		PSRLW		MM1, 8

		PACKUSWB	MM1, MM0
		MOVD		EAX, MM1
		STOSD

		DEC		ECX
		JNZ		Texel_CK_AB_Loop
		EMMS
	}
}

// -------------------------------------

/**
 * @brief 带透明色的亮度调节纹理水平线
 */
void HLineTCKBR(void)
{
	_asm {
		MOV		ECX, PolyLineXR
		SUB		ECX, PolyLineXL

		JZ		Texel_CK_BR_L_equal_R

		MOV		EAX, PolyLineU2
		SUB		EAX, PolyLineU1
		CDQ
		IDIV	ECX
		MOV		PolyLineUa, EAX

		MOV		EAX, PolyLineV2
		SUB		EAX, PolyLineV1
		CDQ
		IDIV	ECX
		MOV		PolyLineVa, EAX

		ADD		PolyLineU1, 0x400000
		ADD		PolyLineV1, 0x400000

Texel_CK_BR_L_equal_R:

		MOV		EBP, PolyLineU1
		MOV		EAX, XMIN
		MOV		EBX, PolyLineV1
		CMP		EAX, PolyLineXL
		JLE		Texel_CK_BR_L1
		MOV		ESI, PolyLineXL
		MOV		PolyLineXL, EAX
		SUB		EAX, ESI
		SUB		ECX, EAX
		MOV		ESI, EAX

		MOV		EAX, PolyLineUa
		IMUL	ESI
		ADD		EBP, EAX

		MOV		EAX, PolyLineVa
		IMUL	ESI
		ADD		EBX, EAX
						
Texel_CK_BR_L1:

		MOV		EAX, XMAX
		SUB		EAX, PolyLineXR
		JGE		Texel_CK_BR_L2
		ADD		ECX, EAX

Texel_CK_BR_L2:

		MOV		EAX, PolyLineY
		MOV		EDI, lpRenderBuffer
		MOV		EAX, [LineStartOffset + EAX * 4]
		ADD		EDI, EAX
		MOV		EAX, PolyLineXL

		MOVQ		MM3, [ALPHA1]		// MM3 = Bright 系数

		INC		ECX
		LEA		EDI, [EDI + EAX * 4]

Texel_CK_BR_Loop:

		MOV		EAX, EBP
		MOV		ESI, EBX
		SHR		EAX, 23
		MOV		EDX, lpTextureSource
		SHR		ESI, 13
		LEA		EAX, [EDX+EAX*4]
		AND		SI, 0xFC00
		ADD		EBP, PolyLineUa
		MOV		EAX, [ESI+EAX]
		ADD		EBX, PolyLineVa
		CMP		EAX, 0x80000000
		JAE		Texel_CK_BR_Skip

		MOVD		MM1, EAX

		PXOR		MM0, MM0

		PUNPCKLBW	MM1, MM0

		PMULLW		MM1, MM3

		PSRLW		MM1, 8

		PACKUSWB	MM1, MM0
		MOVD		EAX, MM1
		STOSD

		DEC		ECX
		JNZ		Texel_CK_BR_Loop
		EMMS
	}

	return;

Texel_CK_BR_Skip:

	_asm {
		CMP		EAX, CK_VALUE | 0x80000000
		JNZ		Texel_CK_BR_K3

		ADD		EDI, 4

		DEC		ECX
		JNZ		Texel_CK_BR_Loop
		EMMS
	}

	return;

Texel_CK_BR_K3:

	_asm {
		CMP		EAX, 0xB0000000
		JB		Texel_CK_BR_K2

		MOV		EDX, [EDI]
		AND		EDX, 0xFCFCFC
		ADD		EAX, EDX
		SHR		EAX, 2

		MOVD		MM1, EAX

		PXOR		MM0, MM0

		PUNPCKLBW	MM1, MM0

		PMULLW		MM1, MM3

		PSRLW		MM1, 8

		PACKUSWB	MM1, MM0
		MOVD		EAX, MM1
		STOSD

		DEC		ECX
		JNZ		Texel_CK_BR_Loop
		EMMS
	}

	return;

Texel_CK_BR_K2:

	_asm {
		CMP		EAX, 0xA0000000
		JB		Texel_CK_BR_K1

		MOV		EDX, [EDI]
		AND		EDX, 0xFEFEFE
		ADD		EAX, EDX
		SHR		EAX, 1

		MOVD		MM1, EAX

		PXOR		MM0, MM0

		PUNPCKLBW	MM1, MM0

		PMULLW		MM1, MM3

		PSRLW		MM1, 8

		PACKUSWB	MM1, MM0
		MOVD		EAX, MM1
		STOSD

		DEC		ECX
		JNZ		Texel_CK_BR_Loop
		EMMS
	}

	return;

Texel_CK_BR_K1:

	_asm {
		MOV		EDX, [EDI]
		AND		EDX, 0xFCFCFC
		LEA		EDX, [EDX+EDX*2]
		ADD		EAX, EDX
		SHR		EAX, 2

		MOVD		MM1, EAX

		PXOR		MM0, MM0

		PUNPCKLBW	MM1, MM0

		PMULLW		MM1, MM3

		PSRLW		MM1, 8

		PACKUSWB	MM1, MM0
		MOVD		EAX, MM1
		STOSD

		DEC		ECX
		JNZ		Texel_CK_BR_Loop
		EMMS
	}
}

// -------------------------------------
//			四边形绘制函数
// -------------------------------------

/**
 * @brief 普通纹理映射四边形
 *
 * 绘制一个带纹理的四边形，四个顶点按顺时针或逆时针顺序排列。
 * 使用扫描线算法，先计算四条边的扫描线交点，然后逐行绘制。
 *
 * 纹理坐标范围：0-255（对应 256x256 纹理）
 *
 * @param X1,Y1 顶点1坐标
 * @param X2,Y2 顶点2坐标
 * @param X3,Y3 顶点3坐标
 * @param X4,Y4 顶点4坐标
 * @param U1,V1 顶点1纹理坐标
 * @param U2,V2 顶点2纹理坐标
 * @param U3,V3 顶点3纹理坐标
 * @param U4,V4 顶点4纹理坐标
 * @param TextureSource 纹理数据指针
 */
void QuadT(
		long X1, long Y1,
		long X2, long Y2,
		long X3, long Y3,
		long X4, long Y4,
		long U1, long V1,
		long U2, long V2,
		long U3, long V3,
		long U4, long V4,
		LPDWORD TextureSource
	)
{
	long	counter, Ymin, Ymax;
	long	Xmax, Xmin;

	// 计算 Y 范围
	Ymin=Y1;
	Ymax=Y1;
	if(Y2<Ymin) Ymin=Y2;
	if(Y2>Ymax) Ymax=Y2;
	if(Y3<Ymin) Ymin=Y3;
	if(Y3>Ymax) Ymax=Y3;
	if(Y4<Ymin) Ymin=Y4;
	if(Y4>Ymax) Ymax=Y4;
	if(Ymax<YMIN || Ymin>YMAX) return;
	if(Ymin<YMIN) Ymin=YMIN;
	if(Ymax>YMAX) Ymax=YMAX;

	// 计算 X 范围（用于早期剔除）
	Xmin=X1;
	Xmax=X1;
	if(X2<Xmin) Xmin=X2;
	if(X2>Xmax) Xmax=X2;
	if(X3<Xmin) Xmin=X3;
	if(X3>Xmax) Xmax=X3;
	if(X4<Xmin) Xmin=X4;
	if(X4>Xmax) Xmax=X4;
	if(Xmax<XMIN || Xmin>XMAX) return;

	// 初始化扫描线边界
	for(counter=Ymin; counter<=Ymax; counter++)
	{
		PolygonSideL[counter]=1000000000;
		PolygonSideR[counter]=-1000000000;
	}

	// 纹理坐标转换为 8.24 固定点数
	U1=(U1<<24); U2=(U2<<24); U3=(U3<<24); U4=(U4<<24);
	V1=(V1<<24); V2=(V2<<24); V3=(V3<<24); V4=(V4<<24);

	// 扫描四条边
	ScanEdgeUV(X1,Y1,X2,Y2,U1,V1,U2,V2);
	ScanEdgeUV(X2,Y2,X3,Y3,U2,V2,U3,V3);
	ScanEdgeUV(X3,Y3,X4,Y4,U3,V3,U4,V4);
	ScanEdgeUV(X4,Y4,X1,Y1,U4,V4,U1,V1);
	
	lpTextureSource = TextureSource;

	// 逐行绘制
	for(PolyLineY = Ymin; PolyLineY <= Ymax; PolyLineY ++)
	{
		PolyLineXL = PolygonSideL[PolyLineY];
		PolyLineXR = PolygonSideR[PolyLineY];

		if(PolyLineXL > XMAX || PolyLineXR < XMIN)
		{
			// 超出裁剪范围，跳过
		}
		else
		{
			PolyLineU1 = PolygonTextureUL[PolyLineY];
			PolyLineV1 = PolygonTextureVL[PolyLineY];
			PolyLineU2 = PolygonTextureUR[PolyLineY];
			PolyLineV2 = PolygonTextureVR[PolyLineY];
			HLineT();
		}
	}
}

// -------------------------------------

/**
 * @brief 加法混合纹理四边形
 */
void QuadTAD(
		long X1, long Y1,
		long X2, long Y2,
		long X3, long Y3,
		long X4, long Y4,
		long U1, long V1,
		long U2, long V2,
		long U3, long V3,
		long U4, long V4,
		LPDWORD TextureSource
	)
{
	long	counter, Ymin, Ymax;
	long	Xmax, Xmin;

	Ymin=Y1;
	Ymax=Y1;
	if(Y2<Ymin) Ymin=Y2;
	if(Y2>Ymax) Ymax=Y2;
	if(Y3<Ymin) Ymin=Y3;
	if(Y3>Ymax) Ymax=Y3;
	if(Y4<Ymin) Ymin=Y4;
	if(Y4>Ymax) Ymax=Y4;
	if(Ymax<YMIN || Ymin>YMAX) return;
	if(Ymin<YMIN) Ymin=YMIN;
	if(Ymax>YMAX) Ymax=YMAX;

	Xmin=X1;
	Xmax=X1;
	if(X2<Xmin) Xmin=X2;
	if(X2>Xmax) Xmax=X2;
	if(X3<Xmin) Xmin=X3;
	if(X3>Xmax) Xmax=X3;
	if(X4<Xmin) Xmin=X4;
	if(X4>Xmax) Xmax=X4;
	if(Xmax<XMIN || Xmin>XMAX) return;

	for(counter=Ymin; counter<=Ymax; counter++)
	{
		PolygonSideL[counter]=1000000000;
		PolygonSideR[counter]=-1000000000;
	}

	U1=(U1<<24); U2=(U2<<24); U3=(U3<<24); U4=(U4<<24);
	V1=(V1<<24); V2=(V2<<24); V3=(V3<<24); V4=(V4<<24);

	ScanEdgeUV(X1,Y1,X2,Y2,U1,V1,U2,V2);
	ScanEdgeUV(X2,Y2,X3,Y3,U2,V2,U3,V3);
	ScanEdgeUV(X3,Y3,X4,Y4,U3,V3,U4,V4);
	ScanEdgeUV(X4,Y4,X1,Y1,U4,V4,U1,V1);
	
	lpTextureSource = TextureSource;

	for(PolyLineY = Ymin; PolyLineY <= Ymax; PolyLineY ++)
	{
		PolyLineXL = PolygonSideL[PolyLineY];
		PolyLineXR = PolygonSideR[PolyLineY];

		if(PolyLineXL > XMAX || PolyLineXR < XMIN)
		{
			// Needn't draw it
		}
		else
		{
			PolyLineU1 = PolygonTextureUL[PolyLineY];
			PolyLineV1 = PolygonTextureVL[PolyLineY];
			PolyLineU2 = PolygonTextureUR[PolyLineY];
			PolyLineV2 = PolygonTextureVR[PolyLineY];
			HLineTAD();
		}
	}
}

// -------------------------------------

/**
 * @brief 减法混合纹理四边形
 */
void QuadTSB(
		long X1, long Y1,
		long X2, long Y2,
		long X3, long Y3,
		long X4, long Y4,
		long U1, long V1,
		long U2, long V2,
		long U3, long V3,
		long U4, long V4,
		LPDWORD TextureSource
	)
{
	long	counter, Ymin, Ymax;
	long	Xmax, Xmin;

	Ymin=Y1;
	Ymax=Y1;
	if(Y2<Ymin) Ymin=Y2;
	if(Y2>Ymax) Ymax=Y2;
	if(Y3<Ymin) Ymin=Y3;
	if(Y3>Ymax) Ymax=Y3;
	if(Y4<Ymin) Ymin=Y4;
	if(Y4>Ymax) Ymax=Y4;
	if(Ymax<YMIN || Ymin>YMAX) return;
	if(Ymin<YMIN) Ymin=YMIN;
	if(Ymax>YMAX) Ymax=YMAX;

	Xmin=X1;
	Xmax=X1;
	if(X2<Xmin) Xmin=X2;
	if(X2>Xmax) Xmax=X2;
	if(X3<Xmin) Xmin=X3;
	if(X3>Xmax) Xmax=X3;
	if(X4<Xmin) Xmin=X4;
	if(X4>Xmax) Xmax=X4;
	if(Xmax<XMIN || Xmin>XMAX) return;

	for(counter=Ymin; counter<=Ymax; counter++)
	{
		PolygonSideL[counter]=1000000000;
		PolygonSideR[counter]=-1000000000;
	}

	U1=(U1<<24); U2=(U2<<24); U3=(U3<<24); U4=(U4<<24);
	V1=(V1<<24); V2=(V2<<24); V3=(V3<<24); V4=(V4<<24);

	ScanEdgeUV(X1,Y1,X2,Y2,U1,V1,U2,V2);
	ScanEdgeUV(X2,Y2,X3,Y3,U2,V2,U3,V3);
	ScanEdgeUV(X3,Y3,X4,Y4,U3,V3,U4,V4);
	ScanEdgeUV(X4,Y4,X1,Y1,U4,V4,U1,V1);
	
	lpTextureSource = TextureSource;

	for(PolyLineY = Ymin; PolyLineY <= Ymax; PolyLineY ++)
	{
		PolyLineXL = PolygonSideL[PolyLineY];
		PolyLineXR = PolygonSideR[PolyLineY];

		if(PolyLineXL > XMAX || PolyLineXR < XMIN)
		{
			// Needn't draw it
		}
		else
		{
			PolyLineU1 = PolygonTextureUL[PolyLineY];
			PolyLineV1 = PolygonTextureVL[PolyLineY];
			PolyLineU2 = PolygonTextureUR[PolyLineY];
			PolyLineV2 = PolygonTextureVR[PolyLineY];
			HLineTSB();
		}
	}
}

// -------------------------------------

/**
 * @brief Alpha 混合纹理四边形
 *
 * @param Alpha 透明度系数 (0-255)
 */
void QuadTAB(
		long X1, long Y1,
		long X2, long Y2,
		long X3, long Y3,
		long X4, long Y4,
		long U1, long V1,
		long U2, long V2,
		long U3, long V3,
		long U4, long V4,
		LPDWORD TextureSource,
		long	Alpha
	)
{
	long	counter, Ymin, Ymax;
	long	Xmax, Xmin;

	if(Alpha < 0) Alpha = 0;
	if(Alpha > 255) Alpha = 255;

	Ymin=Y1;
	Ymax=Y1;
	if(Y2<Ymin) Ymin=Y2;
	if(Y2>Ymax) Ymax=Y2;
	if(Y3<Ymin) Ymin=Y3;
	if(Y3>Ymax) Ymax=Y3;
	if(Y4<Ymin) Ymin=Y4;
	if(Y4>Ymax) Ymax=Y4;
	if(Ymax<YMIN || Ymin>YMAX) return;
	if(Ymin<YMIN) Ymin=YMIN;
	if(Ymax>YMAX) Ymax=YMAX;

	Xmin=X1;
	Xmax=X1;
	if(X2<Xmin) Xmin=X2;
	if(X2>Xmax) Xmax=X2;
	if(X3<Xmin) Xmin=X3;
	if(X3>Xmax) Xmax=X3;
	if(X4<Xmin) Xmin=X4;
	if(X4>Xmax) Xmax=X4;
	if(Xmax<XMIN || Xmin>XMAX) return;

	for(counter=Ymin; counter<=Ymax; counter++)
	{
		PolygonSideL[counter]=1000000000;
		PolygonSideR[counter]=-1000000000;
	}

	U1=(U1<<24); U2=(U2<<24); U3=(U3<<24); U4=(U4<<24);
	V1=(V1<<24); V2=(V2<<24); V3=(V3<<24); V4=(V4<<24);

	ScanEdgeUV(X1,Y1,X2,Y2,U1,V1,U2,V2);
	ScanEdgeUV(X2,Y2,X3,Y3,U2,V2,U3,V3);
	ScanEdgeUV(X3,Y3,X4,Y4,U3,V3,U4,V4);
	ScanEdgeUV(X4,Y4,X1,Y1,U4,V4,U1,V1);
	
	lpTextureSource = TextureSource;

	// 设置 Alpha 系数
	for(Xmin = 0; Xmin < 4; Xmin ++)
	{
		ALPHA1[Xmin] = (WORD)Alpha;
		ALPHA2[Xmin] = (WORD)(256 - Alpha);
	}

	for(PolyLineY = Ymin; PolyLineY <= Ymax; PolyLineY ++)
	{
		PolyLineXL = PolygonSideL[PolyLineY];
		PolyLineXR = PolygonSideR[PolyLineY];

		if(PolyLineXL > XMAX || PolyLineXR < XMIN)
		{
			// Needn't draw it
		}
		else
		{
			PolyLineU1 = PolygonTextureUL[PolyLineY];
			PolyLineV1 = PolygonTextureVL[PolyLineY];
			PolyLineU2 = PolygonTextureUR[PolyLineY];
			PolyLineV2 = PolygonTextureVR[PolyLineY];
			HLineTAB();
		}
	}
}

// -------------------------------------

/**
 * @brief 亮度调节纹理四边形
 *
 * @param Bright 亮度系数 (0-255)
 */
void QuadTBR(
		long X1, long Y1,
		long X2, long Y2,
		long X3, long Y3,
		long X4, long Y4,
		long U1, long V1,
		long U2, long V2,
		long U3, long V3,
		long U4, long V4,
		LPDWORD TextureSource,
		long	Bright
	)
{
	long	counter, Ymin, Ymax;
	long	Xmax, Xmin;

	if(Bright <= 0)
	{
		Quad(X1, Y1, X2, Y2, X3, Y3, X4, Y4, 0);
		return;
	}
	if(Bright > 255)
	{
		QuadT(X1, Y1, X2, Y2, X3, Y3, X4, Y4, U1, V1, U2, V2, U3, V3, U4, V4, TextureSource);
		return;
	}

	Ymin=Y1;
	Ymax=Y1;
	if(Y2<Ymin) Ymin=Y2;
	if(Y2>Ymax) Ymax=Y2;
	if(Y3<Ymin) Ymin=Y3;
	if(Y3>Ymax) Ymax=Y3;
	if(Y4<Ymin) Ymin=Y4;
	if(Y4>Ymax) Ymax=Y4;
	if(Ymax<YMIN || Ymin>YMAX) return;
	if(Ymin<YMIN) Ymin=YMIN;
	if(Ymax>YMAX) Ymax=YMAX;

	Xmin=X1;
	Xmax=X1;
	if(X2<Xmin) Xmin=X2;
	if(X2>Xmax) Xmax=X2;
	if(X3<Xmin) Xmin=X3;
	if(X3>Xmax) Xmax=X3;
	if(X4<Xmin) Xmin=X4;
	if(X4>Xmax) Xmax=X4;
	if(Xmax<XMIN || Xmin>XMAX) return;

	for(counter=Ymin; counter<=Ymax; counter++)
	{
		PolygonSideL[counter]=1000000000;
		PolygonSideR[counter]=-1000000000;
	}

	U1=(U1<<24); U2=(U2<<24); U3=(U3<<24); U4=(U4<<24);
	V1=(V1<<24); V2=(V2<<24); V3=(V3<<24); V4=(V4<<24);

	ScanEdgeUV(X1,Y1,X2,Y2,U1,V1,U2,V2);
	ScanEdgeUV(X2,Y2,X3,Y3,U2,V2,U3,V3);
	ScanEdgeUV(X3,Y3,X4,Y4,U3,V3,U4,V4);
	ScanEdgeUV(X4,Y4,X1,Y1,U4,V4,U1,V1);
	
	lpTextureSource = TextureSource;

	for(Xmin = 0; Xmin < 4; Xmin ++)
	{
		ALPHA1[Xmin] = (WORD)Bright;
	}

	for(PolyLineY = Ymin; PolyLineY <= Ymax; PolyLineY ++)
	{
		PolyLineXL = PolygonSideL[PolyLineY];
		PolyLineXR = PolygonSideR[PolyLineY];

		if(PolyLineXL > XMAX || PolyLineXR < XMIN)
		{
			// Needn't draw it
		}
		else
		{
			PolyLineU1 = PolygonTextureUL[PolyLineY];
			PolyLineV1 = PolygonTextureVL[PolyLineY];
			PolyLineU2 = PolygonTextureUR[PolyLineY];
			PolyLineV2 = PolygonTextureVR[PolyLineY];
			HLineTBR();
		}
	}
}

// -------------------------------------

/**
 * @brief 带透明色的普通纹理四边形
 */
void QuadTCK(
		long X1, long Y1,
		long X2, long Y2,
		long X3, long Y3,
		long X4, long Y4,
		long U1, long V1,
		long U2, long V2,
		long U3, long V3,
		long U4, long V4,
		LPDWORD TextureSource
	)
{
	long	counter, Ymin, Ymax;
	long	Xmax, Xmin;

	Ymin=Y1;
	Ymax=Y1;
	if(Y2<Ymin) Ymin=Y2;
	if(Y2>Ymax) Ymax=Y2;
	if(Y3<Ymin) Ymin=Y3;
	if(Y3>Ymax) Ymax=Y3;
	if(Y4<Ymin) Ymin=Y4;
	if(Y4>Ymax) Ymax=Y4;
	if(Ymax<YMIN || Ymin>YMAX) return;
	if(Ymin<YMIN) Ymin=YMIN;
	if(Ymax>YMAX) Ymax=YMAX;

	Xmin=X1;
	Xmax=X1;
	if(X2<Xmin) Xmin=X2;
	if(X2>Xmax) Xmax=X2;
	if(X3<Xmin) Xmin=X3;
	if(X3>Xmax) Xmax=X3;
	if(X4<Xmin) Xmin=X4;
	if(X4>Xmax) Xmax=X4;
	if(Xmax<XMIN || Xmin>XMAX) return;

	for(counter=Ymin; counter<=Ymax; counter++)
	{
		PolygonSideL[counter]=1000000000;
		PolygonSideR[counter]=-1000000000;
	}

	U1=(U1<<24); U2=(U2<<24); U3=(U3<<24); U4=(U4<<24);
	V1=(V1<<24); V2=(V2<<24); V3=(V3<<24); V4=(V4<<24);

	ScanEdgeUV(X1,Y1,X2,Y2,U1,V1,U2,V2);
	ScanEdgeUV(X2,Y2,X3,Y3,U2,V2,U3,V3);
	ScanEdgeUV(X3,Y3,X4,Y4,U3,V3,U4,V4);
	ScanEdgeUV(X4,Y4,X1,Y1,U4,V4,U1,V1);
	
	lpTextureSource = TextureSource;

	for(PolyLineY = Ymin; PolyLineY <= Ymax; PolyLineY ++)
	{
		PolyLineXL = PolygonSideL[PolyLineY];
		PolyLineXR = PolygonSideR[PolyLineY];

		if(PolyLineXL > XMAX || PolyLineXR < XMIN)
		{
			// Needn't draw it
		}
		else
		{
			PolyLineU1 = PolygonTextureUL[PolyLineY];
			PolyLineV1 = PolygonTextureVL[PolyLineY];
			PolyLineU2 = PolygonTextureUR[PolyLineY];
			PolyLineV2 = PolygonTextureVR[PolyLineY];
			HLineTCK();
		}
	}
}

// -------------------------------------

/**
 * @brief 带透明色的加法混合纹理四边形
 */
void QuadTCKAD(
		long X1, long Y1,
		long X2, long Y2,
		long X3, long Y3,
		long X4, long Y4,
		long U1, long V1,
		long U2, long V2,
		long U3, long V3,
		long U4, long V4,
		LPDWORD TextureSource
	)
{
	long	counter, Ymin, Ymax;
	long	Xmax, Xmin;

	Ymin=Y1;
	Ymax=Y1;
	if(Y2<Ymin) Ymin=Y2;
	if(Y2>Ymax) Ymax=Y2;
	if(Y3<Ymin) Ymin=Y3;
	if(Y3>Ymax) Ymax=Y3;
	if(Y4<Ymin) Ymin=Y4;
	if(Y4>Ymax) Ymax=Y4;
	if(Ymax<YMIN || Ymin>YMAX) return;
	if(Ymin<YMIN) Ymin=YMIN;
	if(Ymax>YMAX) Ymax=YMAX;

	Xmin=X1;
	Xmax=X1;
	if(X2<Xmin) Xmin=X2;
	if(X2>Xmax) Xmax=X2;
	if(X3<Xmin) Xmin=X3;
	if(X3>Xmax) Xmax=X3;
	if(X4<Xmin) Xmin=X4;
	if(X4>Xmax) Xmax=X4;
	if(Xmax<XMIN || Xmin>XMAX) return;

	for(counter=Ymin; counter<=Ymax; counter++)
	{
		PolygonSideL[counter]=1000000000;
		PolygonSideR[counter]=-1000000000;
	}

	U1=(U1<<24); U2=(U2<<24); U3=(U3<<24); U4=(U4<<24);
	V1=(V1<<24); V2=(V2<<24); V3=(V3<<24); V4=(V4<<24);

	ScanEdgeUV(X1,Y1,X2,Y2,U1,V1,U2,V2);
	ScanEdgeUV(X2,Y2,X3,Y3,U2,V2,U3,V3);
	ScanEdgeUV(X3,Y3,X4,Y4,U3,V3,U4,V4);
	ScanEdgeUV(X4,Y4,X1,Y1,U4,V4,U1,V1);
	
	lpTextureSource = TextureSource;

	for(PolyLineY = Ymin; PolyLineY <= Ymax; PolyLineY ++)
	{
		PolyLineXL = PolygonSideL[PolyLineY];
		PolyLineXR = PolygonSideR[PolyLineY];

		if(PolyLineXL > XMAX || PolyLineXR < XMIN)
		{
			// Needn't draw it
		}
		else
		{
			PolyLineU1 = PolygonTextureUL[PolyLineY];
			PolyLineV1 = PolygonTextureVL[PolyLineY];
			PolyLineU2 = PolygonTextureUR[PolyLineY];
			PolyLineV2 = PolygonTextureVR[PolyLineY];
			HLineTCKAD();
		}
	}
}

// -------------------------------------

/**
 * @brief 带透明色的减法混合纹理四边形
 */
void QuadTCKSB(
		long X1, long Y1,
		long X2, long Y2,
		long X3, long Y3,
		long X4, long Y4,
		long U1, long V1,
		long U2, long V2,
		long U3, long V3,
		long U4, long V4,
		LPDWORD TextureSource
	)
{
	long	counter, Ymin, Ymax;
	long	Xmax, Xmin;

	Ymin=Y1;
	Ymax=Y1;
	if(Y2<Ymin) Ymin=Y2;
	if(Y2>Ymax) Ymax=Y2;
	if(Y3<Ymin) Ymin=Y3;
	if(Y3>Ymax) Ymax=Y3;
	if(Y4<Ymin) Ymin=Y4;
	if(Y4>Ymax) Ymax=Y4;
	if(Ymax<YMIN || Ymin>YMAX) return;
	if(Ymin<YMIN) Ymin=YMIN;
	if(Ymax>YMAX) Ymax=YMAX;

	Xmin=X1;
	Xmax=X1;
	if(X2<Xmin) Xmin=X2;
	if(X2>Xmax) Xmax=X2;
	if(X3<Xmin) Xmin=X3;
	if(X3>Xmax) Xmax=X3;
	if(X4<Xmin) Xmin=X4;
	if(X4>Xmax) Xmax=X4;
	if(Xmax<XMIN || Xmin>XMAX) return;

	for(counter=Ymin; counter<=Ymax; counter++)
	{
		PolygonSideL[counter]=1000000000;
		PolygonSideR[counter]=-1000000000;
	}

	U1=(U1<<24); U2=(U2<<24); U3=(U3<<24); U4=(U4<<24);
	V1=(V1<<24); V2=(V2<<24); V3=(V3<<24); V4=(V4<<24);

	ScanEdgeUV(X1,Y1,X2,Y2,U1,V1,U2,V2);
	ScanEdgeUV(X2,Y2,X3,Y3,U2,V2,U3,V3);
	ScanEdgeUV(X3,Y3,X4,Y4,U3,V3,U4,V4);
	ScanEdgeUV(X4,Y4,X1,Y1,U4,V4,U1,V1);
	
	lpTextureSource = TextureSource;

	for(PolyLineY = Ymin; PolyLineY <= Ymax; PolyLineY ++)
	{
		PolyLineXL = PolygonSideL[PolyLineY];
		PolyLineXR = PolygonSideR[PolyLineY];

		if(PolyLineXL > XMAX || PolyLineXR < XMIN)
		{
			// Needn't draw it
		}
		else
		{
			PolyLineU1 = PolygonTextureUL[PolyLineY];
			PolyLineV1 = PolygonTextureVL[PolyLineY];
			PolyLineU2 = PolygonTextureUR[PolyLineY];
			PolyLineV2 = PolygonTextureVR[PolyLineY];
			HLineTCKSB();
		}
	}
}

// -------------------------------------

/**
 * @brief 带透明色的 Alpha 混合纹理四边形
 */
void QuadTCKAB(
		long X1, long Y1,
		long X2, long Y2,
		long X3, long Y3,
		long X4, long Y4,
		long U1, long V1,
		long U2, long V2,
		long U3, long V3,
		long U4, long V4,
		LPDWORD TextureSource,
		long	Alpha
	)
{
	long	counter, Ymin, Ymax;
	long	Xmax, Xmin;

	if(Alpha < 0) Alpha = 0;
	if(Alpha > 255) Alpha = 255;

	Ymin=Y1;
	Ymax=Y1;
	if(Y2<Ymin) Ymin=Y2;
	if(Y2>Ymax) Ymax=Y2;
	if(Y3<Ymin) Ymin=Y3;
	if(Y3>Ymax) Ymax=Y3;
	if(Y4<Ymin) Ymin=Y4;
	if(Y4>Ymax) Ymax=Y4;
	if(Ymax<YMIN || Ymin>YMAX) return;
	if(Ymin<YMIN) Ymin=YMIN;
	if(Ymax>YMAX) Ymax=YMAX;

	Xmin=X1;
	Xmax=X1;
	if(X2<Xmin) Xmin=X2;
	if(X2>Xmax) Xmax=X2;
	if(X3<Xmin) Xmin=X3;
	if(X3>Xmax) Xmax=X3;
	if(X4<Xmin) Xmin=X4;
	if(X4>Xmax) Xmax=X4;
	if(Xmax<XMIN || Xmin>XMAX) return;

	for(counter=Ymin; counter<=Ymax; counter++)
	{
		PolygonSideL[counter]=1000000000;
		PolygonSideR[counter]=-1000000000;
	}

	U1=(U1<<24); U2=(U2<<24); U3=(U3<<24); U4=(U4<<24);
	V1=(V1<<24); V2=(V2<<24); V3=(V3<<24); V4=(V4<<24);

	ScanEdgeUV(X1,Y1,X2,Y2,U1,V1,U2,V2);
	ScanEdgeUV(X2,Y2,X3,Y3,U2,V2,U3,V3);
	ScanEdgeUV(X3,Y3,X4,Y4,U3,V3,U4,V4);
	ScanEdgeUV(X4,Y4,X1,Y1,U4,V4,U1,V1);
	
	lpTextureSource = TextureSource;

	for(Xmin = 0; Xmin < 4; Xmin ++)
	{
		ALPHA1[Xmin] = (WORD)Alpha;
		ALPHA2[Xmin] = (WORD)(256 - Alpha);
	}

	for(PolyLineY = Ymin; PolyLineY <= Ymax; PolyLineY ++)
	{
		PolyLineXL = PolygonSideL[PolyLineY];
		PolyLineXR = PolygonSideR[PolyLineY];

		if(PolyLineXL > XMAX || PolyLineXR < XMIN)
		{
			// Needn't draw it
		}
		else
		{
			PolyLineU1 = PolygonTextureUL[PolyLineY];
			PolyLineV1 = PolygonTextureVL[PolyLineY];
			PolyLineU2 = PolygonTextureUR[PolyLineY];
			PolyLineV2 = PolygonTextureVR[PolyLineY];
			HLineTCKAB();
		}
	}
}

// -------------------------------------

/**
 * @brief 带透明色的亮度调节纹理四边形
 */
void QuadTCKBR(
		long X1, long Y1,
		long X2, long Y2,
		long X3, long Y3,
		long X4, long Y4,
		long U1, long V1,
		long U2, long V2,
		long U3, long V3,
		long U4, long V4,
		LPDWORD TextureSource,
		long	Bright
	)
{
	long	counter, Ymin, Ymax;
	long	Xmax, Xmin;

	if(Bright < 0)
	{
		Bright = 0;
	}
	if(Bright > 255)
	{
		QuadTCK(X1, Y1, X2, Y2, X3, Y3, X4, Y4, U1, V1, U2, V2, U3, V3, U4, V4, TextureSource);
		return;
	}

	Ymin=Y1;
	Ymax=Y1;
	if(Y2<Ymin) Ymin=Y2;
	if(Y2>Ymax) Ymax=Y2;
	if(Y3<Ymin) Ymin=Y3;
	if(Y3>Ymax) Ymax=Y3;
	if(Y4<Ymin) Ymin=Y4;
	if(Y4>Ymax) Ymax=Y4;
	if(Ymax<YMIN || Ymin>YMAX) return;
	if(Ymin<YMIN) Ymin=YMIN;
	if(Ymax>YMAX) Ymax=YMAX;

	Xmin=X1;
	Xmax=X1;
	if(X2<Xmin) Xmin=X2;
	if(X2>Xmax) Xmax=X2;
	if(X3<Xmin) Xmin=X3;
	if(X3>Xmax) Xmax=X3;
	if(X4<Xmin) Xmin=X4;
	if(X4>Xmax) Xmax=X4;
	if(Xmax<XMIN || Xmin>XMAX) return;

	for(counter=Ymin; counter<=Ymax; counter++)
	{
		PolygonSideL[counter]=1000000000;
		PolygonSideR[counter]=-1000000000;
	}

	U1=(U1<<24); U2=(U2<<24); U3=(U3<<24); U4=(U4<<24);
	V1=(V1<<24); V2=(V2<<24); V3=(V3<<24); V4=(V4<<24);

	ScanEdgeUV(X1,Y1,X2,Y2,U1,V1,U2,V2);
	ScanEdgeUV(X2,Y2,X3,Y3,U2,V2,U3,V3);
	ScanEdgeUV(X3,Y3,X4,Y4,U3,V3,U4,V4);
	ScanEdgeUV(X4,Y4,X1,Y1,U4,V4,U1,V1);
	
	lpTextureSource = TextureSource;

	for(Xmin = 0; Xmin < 4; Xmin ++)
	{
		ALPHA1[Xmin] = (WORD)Bright;
	}

	for(PolyLineY = Ymin; PolyLineY <= Ymax; PolyLineY ++)
	{
		PolyLineXL = PolygonSideL[PolyLineY];
		PolyLineXR = PolygonSideR[PolyLineY];

		if(PolyLineXL > XMAX || PolyLineXR < XMIN)
		{
			// Needn't draw it
		}
		else
		{
			PolyLineU1 = PolygonTextureUL[PolyLineY];
			PolyLineV1 = PolygonTextureVL[PolyLineY];
			PolyLineU2 = PolygonTextureUR[PolyLineY];
			PolyLineV2 = PolygonTextureVR[PolyLineY];
			HLineTCKBR();
		}
	}
}

