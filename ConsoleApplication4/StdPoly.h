/*  StdPoly.H - 多边形渲染模块
 *  
 *  本模块提供平面多边形和Gouraud着色多边形的渲染功能
 *  使用扫描线填充算法和Bresenham直线算法
 */

// ========================================
// 全局缓冲区 - 用于存储扫描线数据
// ========================================

// 每条扫描线的左右边界X坐标
long		PolygonSideL[RENDER_HEIGHT];	// 左边界数组
long		PolygonSideR[RENDER_HEIGHT];	// 右边界数组

// Gouraud着色用 - 每条扫描线左右边界的RGB颜色值（16.16定点数格式）
long		PolygonRColorL[RENDER_HEIGHT];	// 左边界红色
long		PolygonRColorR[RENDER_HEIGHT];	// 右边界红色
long		PolygonGColorL[RENDER_HEIGHT];	// 左边界绿色
long		PolygonGColorR[RENDER_HEIGHT];	// 右边界绿色
long		PolygonBColorL[RENDER_HEIGHT];	// 左边界蓝色
long		PolygonBColorR[RENDER_HEIGHT];	// 右边界蓝色

// 当前扫描线的颜色值（16.16定点数格式）
long 		PolyRColorL, PolyGColorL, PolyBColorL;	// 左边界RGB
long 		PolyRColorR, PolyGColorR, PolyBColorR;	// 右边界RGB

// 当前扫描线参数
long		PolyLineXL,		PolyLineXR;		// 左右X坐标
long		PolyLineY,		PolyColor;		// Y坐标和填充颜色

// ========================================
// 边扫描函数 - 使用Bresenham算法
// ========================================

/**
 * 扫描边 - 将边上的像素记录到扫描线边界数组
 * 
 * 使用Bresenham直线算法遍历边上的每个像素，
 * 更新每条扫描线(Y)的最左和最右X坐标
 * 
 * @param X1, Y1 - 边起点坐标
 * @param X2, Y2 - 边终点坐标
 */
void ScanEdge(long X1, long Y1, long X2, long Y2)
{
	long	deltax,deltay,halfx,halfy,dotn;
	long	x,y,dirx,diry,b;

	// 处理水平边特殊情况
	if(Y1==Y2 && Y1>=YMIN && Y1<=YMAX)
	{
		// 水平边：直接比较两个端点，更新该Y坐标的左右边界
		if(X1<PolygonSideL[Y1])
		{
			PolygonSideL[Y1]=X1;
		}
		if(X1>PolygonSideR[Y1])
		{
			PolygonSideR[Y1]=X1;
		}
		if(X2<PolygonSideL[Y1])
		{
			PolygonSideL[Y1]=X2;
		}
		if(X2>PolygonSideR[Y1])
		{
			PolygonSideR[Y1]=X2;
		}
		return;
	}

	// 确定X和Y的步进方向
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
	
	// Bresenham算法：当Y变化快于X时（陡峭边）
	if(deltax<deltay)
	{
		halfy=deltay>>1;		// 中点阈值
		dotn=deltay;
		do
		{
			// 只处理在有效Y范围内的像素
			if(y>=YMIN && y<=YMAX)
			{
				// 更新该扫描线的左右边界
				if(x<PolygonSideL[y])
				{
					PolygonSideL[y]=x;
				}
			    if(x>PolygonSideR[y])
				{
					PolygonSideR[y]=x;
				}
			}
			y+=diry;			// Y步进
			b+=deltax;			// 累加误差
			if(b>halfy)			// 误差超过阈值，X步进
			{
				b-=deltay;
				x+=dirx;
			}
		} while(dotn--);
    }
	// Bresenham算法：当X变化快于Y时（平缓边）
	else
	{
		halfx=deltax>>1;
		dotn=deltax;
		do
		{
			if(y>=YMIN && y<=YMAX)
			{
				if(x<PolygonSideL[y])
				{
					PolygonSideL[y]=x;
				}
			    if(x>PolygonSideR[y])
				{
					PolygonSideR[y]=x;
				}
			}
			x+=dirx;			// X步进
			b+=deltay;			// 累加误差
			if(b>halfx)			// 误差超过阈值，Y步进
			{
				b-=deltax;
				y+=diry;
			}
		}	while(dotn--);
	}
}

// ========================================

/**
 * 扫描边（带Gouraud着色）- 边扫描同时插值颜色
 * 
 * 在扫描边的同时，对RGB颜色进行线性插值，
 * 将颜色值也存储到扫描线边界数组中
 * 
 * @param X1, Y1 - 边起点坐标
 * @param X2, Y2 - 边终点坐标
 * @param C1, C2 - 边起点和终点的颜色（0xRRGGBB格式）
 */
void ScanEdgeGouraud(long X1, long Y1, long X2, long Y2, long C1, long C2)
{
	long	deltax,deltay,halfx,halfy,dotn;
	long	x,y,dirx,diry,b;
	long	R1, G1, B1;			// 起点颜色分量（16.16定点数）
	long	R2, G2, B2;			// 终点颜色分量（16.16定点数）
	long	Ra, Ga, Ba;			// 颜色步进值（16.16定点数）

	// 将32位颜色值分解为RGB三个分量，转换为16.16定点数格式
	_asm {
		MOV		EBX, C1
		MOV		EAX, EBX
		MOV		EDX, EBX
		AND		EAX, 0x000000FF		// 提取蓝色
		AND		EDX, 0x0000FF00		// 提取绿色
		SHL		EAX, 16				// 转为16.16定点数
		AND		EBX, 0x00FF0000		// 提取红色
		MOV		B1, EAX
		SHL		EDX, 8				// 绿色转为16.16
		MOV		G1, EDX
		MOV		R1, EBX

		MOV		EBX, C2
		MOV		EAX, EBX
		MOV		EDX, EBX
		AND		EAX, 0x000000FF
		AND		EDX, 0x0000FF00
		SHL		EAX, 16
		AND		EBX, 0x00FF0000
		MOV		B2, EAX
		SHL		EDX, 8
		MOV		G2, EDX
		MOV		R2, EBX
	}

	// 处理水平边特殊情况
	if(Y1==Y2 && Y1>=YMIN && Y1<=YMAX)
	{
		if (X1<PolygonSideL[Y1])
		{
			PolygonSideL[Y1]=X1;
			PolygonRColorL[Y1]=R1;
			PolygonGColorL[Y1]=G1;
			PolygonBColorL[Y1]=B1;
		}
		if (X1>PolygonSideR[Y1])
		{
			PolygonSideR[Y1]=X1;
			PolygonRColorR[Y1]=R1;
			PolygonGColorR[Y1]=G1;
			PolygonBColorR[Y1]=B1;
		}
		if (X2<PolygonSideL[Y1])
		{
			PolygonSideL[Y1]=X2;
			PolygonRColorL[Y1]=R2;
			PolygonGColorL[Y1]=G2;
			PolygonBColorL[Y1]=B2;
		}
		if (X2>PolygonSideR[Y1])
		{
			PolygonSideR[Y1]=X2;
			PolygonRColorR[Y1]=R2;
			PolygonGColorR[Y1]=G2;
			PolygonBColorR[Y1]=B2;
		}
		return;
	}

	// 确定步进方向
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
	
	// 陡峭边：Y变化快
	if(deltax<deltay)
	{
		// 计算颜色步进值（每步Y的颜色变化量）
		Ra=(R2-R1)/deltay;
		Ga=(G2-G1)/deltay;
		Ba=(B2-B1)/deltay;

		halfy=deltay>>1;
		dotn=deltay;
		do
		{
			if(y>=YMIN && y<=YMAX)
			{
				// 更新边界和颜色
				if(x<PolygonSideL[y])
				{
					PolygonSideL[y]=x;
					PolygonRColorL[y]=R1;
					PolygonGColorL[y]=G1;
					PolygonBColorL[y]=B1;
				}
			    if(x>PolygonSideR[y])
				{
					PolygonSideR[y]=x;
					PolygonRColorR[y]=R1;
					PolygonGColorR[y]=G1;
					PolygonBColorR[y]=B1;
				}
			}
			y+=diry;
			b+=deltax;
			// 颜色插值
			R1+=Ra;
			G1+=Ga;
			B1+=Ba;
			if(b>halfy)
			{
				b-=deltay;
				x+=dirx;
			}
		} while(dotn--);
    }
	// 平缓边：X变化快
	else
	{
		// 计算颜色步进值（每步X的颜色变化量）
		Ra=(R2-R1)/deltax;
		Ga=(G2-G1)/deltax;
		Ba=(B2-B1)/deltax;

		halfx=deltax>>1;
		dotn=deltax;
		do
		{
			if(y>=YMIN && y<=YMAX)
			{
				if(x<PolygonSideL[y])
				{
					PolygonSideL[y]=x;
					PolygonRColorL[y]=R1;
					PolygonGColorL[y]=G1;
					PolygonBColorL[y]=B1;
				}
			    if(x>PolygonSideR[y])
				{
					PolygonSideR[y]=x;
					PolygonRColorR[y]=R1;
					PolygonGColorR[y]=G1;
					PolygonBColorR[y]=B1;
				}
			}
			x+=dirx;
			b+=deltay;
			// 颜色插值
			R1+=Ra;
			G1+=Ga;
			B1+=Ba;

			if(b>halfx)
			{
				b-=deltax;
				y+=diry;
			}
		} while(dotn--);
	}
}

// ========================================
// 水平线绘制函数
// ========================================

/**
 * 绘制纯色水平线
 * 
 * 使用REP STOSD指令快速填充一行像素
 * 自动处理左右裁剪（XMIN/XMAX边界）
 */
void HLine(void)
{
	_asm {
		MOV		EBX, PolyLineXL		// 左边界X
		MOV		ECX, PolyLineXR		// 右边界X
		
		// 左裁剪：如果XL < XMIN，则XL = XMIN
		CMP		EBX, XMIN
		JGE		HLine_R
		MOV		EBX, XMIN

HLine_R:
	
		// 右裁剪：如果XR > XMAX，则XR = XMAX
		CMP		ECX, XMAX
		JLE		HLine_Ready
		MOV		ECX, XMAX

HLine_Ready:

		SUB		ECX, EBX			// ECX = 像素数量 - 1

		// 计算目标地址：lpRenderBuffer + LineStartOffset[Y] + XL * 4
		MOV		EAX, PolyLineY
		MOV		EDI, lpRenderBuffer
		MOV		EAX, [LineStartOffset+EAX*4]
		ADD		EDI, EAX

		INC		ECX					// 像素数量（包含两端）

		LEA		EDI, [EDI+EBX*4]	// 加上X偏移
		MOV		EAX, PolyColor		// 填充颜色

		REP		STOSD				// 重复存储DWORD，填充整行
	}
}

// ========================================

/**
 * 绘制Gouraud着色水平线
 * 
 * 在水平方向上对RGB颜色进行线性插值，实现平滑的颜色渐变效果
 * 使用16.16定点数进行颜色计算，保证精度
 * 自动处理左右裁剪
 */
void HLineGouraud(void)
{
	long	Ra, Ga, Ba;			// 颜色步进值（16.16定点数）

	_asm {
		// 计算水平线长度 DeltaX = XR - XL
		MOV		ECX, PolyLineXR
		SUB		ECX, PolyLineXL

		// 如果DeltaX为0，跳过步进值计算
		JZ		HLine_Gouraud_L_equal_R

		// 计算红色步进值 Ra = (R2 - R1) / DeltaX
		MOV		EAX, PolyRColorR
		SUB		EAX, PolyRColorL
		CDQ							// 符号扩展EAX到EDX:EAX
		IDIV	ECX
		MOV		Ra, EAX
		
		// 计算绿色步进值 Ga = (G2 - G1) / DeltaX
		MOV		EAX, PolyGColorR
		SUB		EAX, PolyGColorL
		CDQ
		IDIV	ECX
		MOV		Ga, EAX
		
		// 计算蓝色步进值 Ba = (B2 - B1) / DeltaX
		MOV		EAX, PolyBColorR
		SUB		EAX, PolyBColorL
		CDQ
		IDIV	ECX
		MOV		Ba, EAX
		
		// 偏移修正：加0.5（0x8000）用于四舍五入
		ADD		PolyRColorL,0x8000
		ADD		PolyGColorL,0x8000
		ADD		PolyBColorL,0x8000

HLine_Gouraud_L_equal_R:	

		// 左裁剪处理
		MOV		EAX, XMIN
		CMP		PolyLineXL, EAX
		JGE		HLINE_GOURAUD_L1

		MOV		EBX, PolyLineXL		// EBX = XL（原始值）
		MOV		PolyLineXL, EAX		// XL = XMIN
		SUB		EAX, EBX			// EAX = XMIN - XL（裁剪掉的像素数）
		SUB		ECX, EAX			// ECX = ECX - (XMIN - XL)
		MOV		ESI, EAX

		// 根据裁剪的像素数调整起始颜色值
		MOV		EAX, Ra
		IMUL	ESI					// EAX = Ra * 裁剪像素数
		ADD		PolyRColorL, EAX	// 调整红色起始值

		MOV		EAX, Ga
		IMUL	ESI
		ADD		PolyGColorL, EAX	// 调整绿色起始值

		MOV		EAX, Ba
		IMUL	ESI
		ADD		PolyBColorL, EAX	// 调整蓝色起始值

HLine_Gouraud_L1:

		// 右裁剪处理
		MOV		EAX, XMAX
		SUB		EAX, PolyLineXR		// EAX = XMAX - XR
		JGE		HLine_Gouraud_L2	// 如果XR <= XMAX，无需裁剪
		ADD		ECX, EAX			// ECX = ECX + (XMAX - XR)

HLine_Gouraud_L2:

		// 计算目标地址
		MOV		EAX, PolyLineY
		MOV		EDI, lpRenderBuffer
		MOV		EAX, [LineStartOffset + EAX * 4]
		ADD		EDI, EAX
		MOV		EAX, PolyLineXL
		LEA		EDI, [EDI + EAX * 4]

		INC		ECX					// 包含最后一个像素

		// 加载颜色值到寄存器
		MOV		EDX, PolyBColorL	// EDX = 蓝色（16.16）
		MOV		ESI, PolyGColorL	// ESI = 绿色（16.16）

HLine_Gouraud_Loop:

		// 组合RGB颜色值
		MOV		EAX, EDX
		SHR		EAX, 16				// EAX = 蓝色整数部分
		MOV		EBX, EAX

		MOV		EAX, ESI
		SHR		EAX, 8				// EAX = 绿色（右移8位，因为绿色在中间字节）
		MOV		BH, AH				// BH = 绿色值

		MOV		EAX, PolyRColorL
		MOV		AX, BX				// AX = GB（低16位）
		STOSD						// 存储32位颜色值到帧缓冲区
	
		// 颜色步进
		MOV		EBX, PolyRColorL
		ADD		EDX, Ba				// 蓝色 += 步进值
		ADD		EBX, Ra				// 红色 += 步进值
		ADD		ESI, Ga				// 绿色 += 步进值
		MOV		PolyRColorL, EBX

		DEC		ECX
		JNZ		HLine_Gouraud_Loop	// 循环直到ECX为0
	}
}

// ========================================
// 四边形绘制函数
// ========================================

/**
 * 绘制纯色填充四边形
 * 
 * 使用扫描线算法：
 * 1. 计算四边形的Y范围
 * 2. 初始化扫描线边界数组
 * 3. 扫描四条边，记录每条扫描线的左右边界
 * 4. 逐行绘制水平线
 * 
 * @param X1-X4, Y1-Y4 - 四个顶点坐标
 * @param Color - 填充颜色（0xRRGGBB格式）
 */
void Quad(
		long X1, long Y1,
		long X2, long Y2,
		long X3, long Y3,
		long X4, long Y4,
		DWORD Color
	)
{
	long	counter, Ymin, Ymax;
	long	Xmax, Xmin;

	// 计算Y范围（用于裁剪检查）
	Ymin=Y1;
	Ymax=Y1;
	if(Y2<Ymin) Ymin=Y2;
	if(Y2>Ymax) Ymax=Y2;
	if(Y3<Ymin) Ymin=Y3;
	if(Y3>Ymax) Ymax=Y3;
	if(Y4<Ymin) Ymin=Y4;
	if(Y4>Ymax) Ymax=Y4;
	if(Ymax<YMIN || Ymin>YMAX) return;	if(Ymin<YMIN) Ymin=YMIN;
	if(Ymax>YMAX) Ymax=YMAX;

	// 计算X范围（用于裁剪检查）
	Xmin=X1;
	Xmax=X1;
	if(X2<Xmin) Xmin=X2;
	if(X2>Xmax) Xmax=X2;
	if(X3<Xmin) Xmin=X3;
	if(X3>Xmax) Xmax=X3;
	if(X4<Xmin) Xmin=X4;
	if(X4>Xmax) Xmax=X4;
	if(Xmax<XMIN || Xmin>XMAX) return;

	// 初始化扫描线边界数组
	for(counter=Ymin; counter<=Ymax; counter++)
	{
		PolygonSideL[counter]=1000000000;	// 极大值（找最小）
		PolygonSideR[counter]=-1000000000;	// 极小值（找最大）
	}

	// 扫描四条边
	ScanEdge(X1,Y1,X2,Y2);
	ScanEdge(X2,Y2,X3,Y3);
	ScanEdge(X3,Y3,X4,Y4);
	ScanEdge(X4,Y4,X1,Y1);

	PolyColor = Color;

	// 逐行绘制
	for(PolyLineY = Ymin; PolyLineY <= Ymax; PolyLineY ++)
	{
		PolyLineXL = PolygonSideL[PolyLineY];
		PolyLineXR = PolygonSideR[PolyLineY];

		if(PolyLineXL > XMAX || PolyLineXR < XMIN)
		{
			// 完全在屏幕外，无需绘制
		}
		else
		{
			HLine();
		}
	}
}

// ========================================

/**
 * 绘制Gouraud着色四边形
 * 
 * 每个顶点可以有不同的颜色，内部使用双线性颜色插值
 * 实现平滑的光照效果
 * 
 * @param X1-X4, Y1-Y4 - 四个顶点坐标
 * @param C1-C4 - 四个顶点的颜色（0xRRGGBB格式）
 */
void QuadGouraud(
		long X1, long Y1,
		long X2, long Y2,
		long X3, long Y3,
		long X4, long Y4,
		long C1, long C2, long C3, long C4
	)
{
	long	counter, Ymin, Ymax;
	long	Xmax, Xmin;

	// 计算Y范围
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

	// 计算X范围
	Xmin=X1;
	Xmax=X1;
	if(X2<Xmin) Xmin=X2;
	if(X2>Xmax) Xmax=X2;
	if(X3<Xmin) Xmin=X3;
	if(X3>Xmax) Xmax=X3;
	if(X4<Xmin) Xmin=X4;
	if(X4>Xmax) Xmax=X4;
	if(Xmax<XMIN || Xmin>XMAX) return;

	// 初始化扫描线边界数组
	for(counter=Ymin; counter<=Ymax; counter++)
	{
		PolygonSideL[counter]=1000000000;
		PolygonSideR[counter]=-1000000000;
	}

	// 扫描四条边（带颜色插值）
	ScanEdgeGouraud(X1,Y1,X2,Y2,C1,C2);
	ScanEdgeGouraud(X2,Y2,X3,Y3,C2,C3);
	ScanEdgeGouraud(X3,Y3,X4,Y4,C3,C4);
	ScanEdgeGouraud(X4,Y4,X1,Y1,C4,C1);

	// 逐行绘制
	for(PolyLineY = Ymin; PolyLineY <= Ymax; PolyLineY ++)
	{
		PolyLineXL = PolygonSideL[PolyLineY];
		PolyLineXR = PolygonSideR[PolyLineY];

		if(PolyLineXL > XMAX || PolyLineXR < XMIN)
		{
			// 完全在屏幕外
		}
		else
		{
	 		// 从扫描线数组加载左右边界的颜色值
	 		PolyRColorL = PolygonRColorL[PolyLineY];
 			PolyGColorL = PolygonGColorL[PolyLineY];
 			PolyBColorL = PolygonBColorL[PolyLineY];
 			PolyRColorR = PolygonRColorR[PolyLineY];
 			PolyGColorR = PolygonGColorR[PolyLineY];
 			PolyBColorR = PolygonBColorR[PolyLineY];

			HLineGouraud();
		}
	}
}

// ========================================
