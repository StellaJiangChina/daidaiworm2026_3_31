/*
 *
 *               DaiDai 源代码
 *               ==================
 *
 *    版权所有 (C) FAN YIPENG (樊一鹏), 1999-2000
 *
 *                     http://freemind.163.net
 *
 * [文件信息]
 *    版本号      : 1.0.04
 *    发布日期    : 1999-12-05
 *
 * [说明]
 *    因为时间匆忙，所以没加注释，还请多理解。
 *
 *    主要函数说明：
 *    - InitGame(void)   游戏初始化
 *    - GameLoop(void)   游戏主体
 *    - ExitGame(void)   退出处理
 *
 *    调用到的函数大家凭名字就能知道是干什么的了。
 *
 *    注意：用 "_" 开头的图形函数未做裁剪，大家使用时务必小心。
 *    实在吃不准就用没有 "_" 开头的那套图形函数吧。
 *
 *    另外，游戏所需的其它资源都在以前发放的 worm1004.exe 游戏包中的 DAT 目录下。
 *
 *    如果有什么问题，请去我主页上的论坛留言，我会定期去那里回答大家的问题。
 *
 *    如果觉得我作得不好，愿意指点一二的，亦非常欢迎，先谢了 :)
 *
 *    愿大家能把国产游戏做得更好！
 *
 * [游戏简介]
 *    这是一个经典的贪食蛇/蠕虫对战游戏，玩家控制一条蠕虫在屏幕上移动，
 *    吃各种颜色的食物来获得分数。不同颜色的食物有不同的效果：
 *    - 红色: 加速
 *    - 黄色: 发射金光
 *    - 绿色: Beans变化
 *    - 蓝色: 下雨
 *    - 紫色: 变瘦
 *    - 白色: +30分
 */

// 链接 DirectX 相关库
#pragma comment(lib, "dinput8.lib")    // DirectInput 库 - 处理键盘输入
#pragma comment(lib, "dxguid.lib")     // DirectX GUID 库
#pragma comment(lib, "vfw32.lib")      // Video for Windows 库 - 处理 AVI 视频
#pragma comment(lib, "winmm.lib")     // Windows 多媒体库 - 处理时间函数
#pragma comment(lib, "dsound.lib")    // DirectSound 库 - 处理声音
#pragma comment(lib, "ddraw.lib")     // DirectDraw 库 - 处理2D图形

/*======================================*/

// ExePath: 存储游戏可执行文件的完整路径
char    ExePath[256];

/*======================================*/

#include "STDINIT.H"    // 包含游戏引擎的初始化头文件

/*======================================*/

// 全局得分变量
long    Score,         // 当前得分
        HiScore = 0;   // 历史最高分

// Windows 注册表相关头文件
#include <winreg.h>
#include <direct.h>

// 注册到注册表中的游戏信息字符串
char    CRstr[]="(C)FAN YIPENG(樊一鹏)";              // 版权信息
char    HPstr[]="HTTP://GAMEVISION.YEAH.NET";         // 主页地址
char    VRstr[]="1.0.04";                             // 版本号

/*--------------------------------------------------------------
 * Regedit - 向Windows注册表写入游戏信息
 * 功能: 
 *   1. 从命令行获取游戏可执行文件路径
 *   2. 提取游戏所在目录
 *   3. 将游戏信息写入注册表
 *   4. 切换到游戏目录
 *--------------------------------------------------------------*/
void Regedit(void)
{
    char    WinExe[256];      // 存储可执行文件完整路径
    char    *pbuf;           // 指向命令行参数的指针
    int     i;               // 循环计数器
    HKEY    hkey;            // 注册表键句柄

    // 获取命令行字符串
    pbuf = GetCommandLineA();

    // 复制命令行参数（跳过开头的引号）
    strcpy_s(WinExe, pbuf + 1);
    
    // 查找并去除命令行末尾的引号
    for(i = (int)strlen(WinExe); i >= 0; i --)
    {
        if(WinExe[i] == '"')
        {
            WinExe[i] = 0;
            i = -10000;
        }
    }

    // 获取程序运行路径
    strcpy_s(ExePath, WinExe);
    for(i = (int)strlen(WinExe); i >= 0; i --)
    {
        if(WinExe[i] == '\\')
        {
            ExePath[i+1] = 0;    // 保留反斜杠，截断路径
            i = -10000;
        }
    }

    // 在注册表中创建游戏的配置项
    // 路径: HKEY_USERS\.DEFAULT\Software\GAMEVISION\DAIDAI WORM
    RegCreateKeyA(HKEY_USERS, ".DEFAULT\\Software\\GAMEVISION\\DAIDAI WORM", &hkey);
    RegSetValueExA(hkey, "COPYRIGHT", 0, REG_SZ, (BYTE *)CRstr, strlen(CRstr)+1);  // 版权
    RegSetValueExA(hkey, "HOMEPAGE", 0, REG_SZ, (BYTE *)HPstr, strlen(HPstr)+1);    // 主页
    RegSetValueExA(hkey, "VERSION", 0, REG_SZ, (BYTE *)VRstr, strlen(VRstr)+1);    // 版本
    RegSetValueExA(hkey, "PATH", 0, REG_SZ, (BYTE *)ExePath, strlen(ExePath)+1);    // 路径
    RegSetValueExA(hkey, "COMMAND", 0, REG_SZ, (BYTE *)WinExe, strlen(WinExe)+1);  // 命令
    RegCloseKey(hkey);

    // 将路径转换为大写（某些系统需要）
    if(ExePath[0] >= 'a' && ExePath[0] <= 'z') 
        ExePath[0] = ExePath[0] - 'a' + 'A';
    
    // 切换到游戏所在驱动器和工作目录
    _chdrive(ExePath[0] - 'A' + 1);  // 切换驱动器
    _chdir(ExePath);                  // 切换目录
}

/*--------------------------------------------------------------
 * LoadScore - 从注册表加载历史最高分
 *--------------------------------------------------------------*/
void LoadScore(void)
{
    DWORD   Type,       // 注册表值类型（未使用）
            Size = 4;   // 值的大小
    HKEY    hkey;       // 注册表键句柄

    RegCreateKeyA(HKEY_USERS, ".DEFAULT\\Software\\GAMEVISION\\DAIDAI WORM", &hkey);
    RegQueryValueExA(hkey, "HISCORE", NULL, &Type, (BYTE *)&HiScore, &Size);  // 查询最高分
    RegCloseKey(hkey);
}

/*--------------------------------------------------------------
 * SaveScore - 将当前最高分保存到注册表
 *--------------------------------------------------------------*/
void SaveScore(void)
{
    HKEY    hkey;       // 注册表键句柄

    RegCreateKeyA(HKEY_USERS, ".DEFAULT\\Software\\GAMEVISION\\DAIDAI WORM", &hkey);
    RegSetValueExA(hkey, "HISCORE", 0, REG_DWORD, (BYTE *)&HiScore, 4);  // 保存最高分
    RegCloseKey(hkey);
}

/*======================================*/

// 纹理数据缓冲区
DWORD   FOODTXM[0x10000];       // 食物纹理
DWORD   BRIGHTTXM[0x10000];     // 高亮/发光纹理

// 图片缓冲区
DWORD   PicBack[400 * 300 + 2];        // 背景图片缓冲区 (400x300分辨率)

LPDWORD PicMouse;             // 鼠标光标图片

LPDWORD PicWormBody;          // 蠕虫身体图片
LPDWORD PicWormBox;           // 蠕虫盒子/障碍物图片
LPDWORD PicWormShadow;        // 蠕虫阴影图片

LPDWORD PicWormB[8];          // 蠕虫身体变形图片数组（8帧动画）
LPDWORD PicWormD[8];          // 蠕虫死亡/消散动画图片数组

LPDWORD PicTitle;             // 标题图片
LPDWORD PicTitle2;            // 标题背景图片

LPDWORD PicHelpInfo;          // 帮助信息图片

LPDWORD PicAdd5;              // 加5分特效图片
LPDWORD PicAdd10;             // 加10分特效图片
LPDWORD PicAdd15;             // 加15分特效图片
LPDWORD PicAdd30;             // 加30分特效图片

LPDWORD PicScore;             // 分数显示背景图片

// =====================================

// 背景帧计数器
long    BG_CNT = 0;

// =====================================

/*--------------------------------------------------------------
 * LoadBG - 加载背景帧
 * 功能: 从 AVI 视频中加载一帧作为背景
 *--------------------------------------------------------------*/
void LoadBG(void)
{
    LPDWORD temp;

    // 保存当前渲染缓冲区指针
    temp = lpRenderBuffer;

    // 加载背景视频帧
    LoadVideoFrame(BG_CNT);
    
    // 切换到特效渲染页面
    SetRenderPage(SFX2PAGE);
    
    // 翻转整个视频缓冲区
    FilpWholeVideoBuffer();

    // 将当前帧保存到背景图片缓冲区
    _GetImage(0, 0, 400, 300, PicBack);

    // 恢复渲染缓冲区
    lpRenderBuffer = temp;
}

// =====================================

/*--------------------------------------------------------------
 * InitGame - 游戏初始化函数
 * 功能:
 *   1. 注册游戏信息
 *   2. 加载背景视频
 *   3. 加载所有纹理和图片资源
 *   4. 加载所有声音资源
 *   5. 加载最高分记录
 *--------------------------------------------------------------*/
void InitGame(void)
{
    Regedit();    // 注册游戏信息

    // 初始化 AVI 文件系统
    AVIFileInit(); 

    // 加载背景视频
    if(OpenVideoStream((char*)"DAT\\bg.avi") == FALSE)
    {
        // 如果背景视频加载失败，报告错误并返回
        InitFail((char*)"[BG.AVI] Load Fail");
        return;
    }

    LoadBG();    // 加载第一帧背景

    // 加载纹理文件
    LoadTextureFile(FOODTXM, (char*)"DAT\\food.txm");      // 食物纹理
    LoadTextureFile(BRIGHTTXM, (char*)"DAT\\bright.txm"); // 发光纹理

    // 加载标题和帮助图片
    PicTitle = LoadPPMImageFile((char*)"DAT\\title.ppm");       // 标题
    PicTitle2 = LoadPPMImageFile((char*)"DAT\\titleb.ppm");     // 标题背景

    PicHelpInfo = LoadPPMImageFile((char*)"DAT\\helpinfo.ppm"); // 帮助信息

    // 加载蠕虫相关图片
    PicWormBody = LoadPPMImageFile((char*)"DAT\\wormbody.ppm");     // 蠕虫身体
    PicWormBox = LoadPPMImageFile((char*)"DAT\\wormbox.ppm");       // 障碍物
    PicWormShadow = LoadPPMImageFile((char*)"DAT\\wormshadow.ppm"); // 阴影

    // 加载蠕虫变形动画帧 (wormb1 ~ wormb8)
    PicWormB[0] = LoadPPMImageFile((char*)"DAT\\wormb1.ppm");
    PicWormB[1] = LoadPPMImageFile((char*)"DAT\\wormb2.ppm");
    PicWormB[2] = LoadPPMImageFile((char*)"DAT\\wormb3.ppm");
    PicWormB[3] = LoadPPMImageFile((char*)"DAT\\wormb4.ppm");
    PicWormB[4] = LoadPPMImageFile((char*)"DAT\\wormb5.ppm");
    PicWormB[5] = LoadPPMImageFile((char*)"DAT\\wormb6.ppm");
    PicWormB[6] = LoadPPMImageFile((char*)"DAT\\wormb7.ppm");
    PicWormB[7] = LoadPPMImageFile((char*)"DAT\\wormb8.ppm");

    // 加载蠕虫消散/死亡动画帧 (wormd1 ~ wormd8)
    PicWormD[0] = LoadPPMImageFile((char*)"DAT\\wormd1.ppm");
    PicWormD[1] = LoadPPMImageFile((char*)"DAT\\wormd2.ppm");
    PicWormD[2] = LoadPPMImageFile((char*)"DAT\\wormd3.ppm");
    PicWormD[3] = LoadPPMImageFile((char*)"DAT\\wormd4.ppm");
    PicWormD[4] = LoadPPMImageFile((char*)"DAT\\wormd5.ppm");
    PicWormD[5] = LoadPPMImageFile((char*)"DAT\\wormd6.ppm");
    PicWormD[6] = LoadPPMImageFile((char*)"DAT\\wormd7.ppm");
    PicWormD[7] = LoadPPMImageFile((char*)"DAT\\wormd8.ppm");

    // 加载分数特效图片
    PicAdd5 = LoadPPMImageFile((char*)"DAT\\add5.ppm");    // +5分
    PicAdd10 = LoadPPMImageFile((char*)"DAT\\add10.ppm");  // +10分
    PicAdd15 = LoadPPMImageFile((char*)"DAT\\add15.ppm");  // +15分
    PicAdd30 = LoadPPMImageFile((char*)"DAT\\add30.ppm");  // +30分

    PicScore = LoadPPMImageFile((char*)"DAT\\score.ppm");  // 分数显示背景

    // 加载鼠标光标并设置
    PicMouse = LoadPPMImageFile((char*)"DAT\\mouse.ppm");
    SetMouseImage(PicMouse);

    // 加载声音资源 (PCM 格式)
    
    // 环境音效
    LoadRawSndData(0, (char*)"DAT\\LOOP.PCM", 1, 11025, 16);       // 循环水声
    LoadRawSndData(1, (char*)"DAT\\DROP.PCM", 1, 11025, 16);      // 水滴声
    LoadRawSndData(2, (char*)"DAT\\EAT.PCM", 1, 11025, 16);      // 吃东西声

    LoadRawSndData(3, (char*)"DAT\\FREEZE.PCM", 1, 22050, 16);   // 冻结声

    LoadRawSndData(4, (char*)"DAT\\POPO.PCM", 1, 11025, 16);     // 泡泡破裂声
    LoadRawSndData(5, (char*)"DAT\\DIE.PCM", 1, 11025, 16);      // 死亡声

    LoadRawSndData(6, (char*)"DAT\\THUNDER1.PCM", 1, 11025, 16); // 雷声1
    LoadRawSndData(7, (char*)"DAT\\RAINLOOP.PCM", 2, 11025, 16); // 雨声循环
    LoadRawSndData(8, (char*)"DAT\\THUNDER2.PCM", 1, 11025, 16); // 雷声2

    LoadRawSndData(10, (char*)"DAT\\BEAT.PCM", 1, 11025, 16);    // 心跳声

    LoadRawSndData(11, (char*)"DAT\\FADE.PCM", 1, 11025, 16);    // 淡入淡出声

    LoadRawSndData(12, (char*)"DAT\\LASER.PCM", 1, 22050, 16);   // 激光声
    LoadRawSndData(13, (char*)"DAT\\WARP.PCM", 1, 22050, 16);    // 扭曲声

    LoadRawSndData(21, (char*)"DAT\\SPEEDUP.PCM", 1, 11025, 16); // 加速声
    LoadRawSndData(22, (char*)"DAT\\SPEEDOWN.PCM", 1, 11025, 16);// 减速声

    // 背景音乐
    LoadRawSndData(90, (char*)"DAT\\MUSIC.PCM", 1, 22050, 8);    // 主音乐
    LoadRawSndData(91, (char*)"DAT\\START.PCM", 1, 22050, 16);  // 开始音效
    LoadRawSndData(92, (char*)"DAT\\SELECT.PCM", 1, 11025, 16); // 选择音效

    // 各种环境音效 (ENV1 ~ ENV14)
    LoadRawSndData(101, (char*)"DAT\\ENV1.PCM", 1, 11025, 16);
    LoadRawSndData(102, (char*)"DAT\\ENV2.PCM", 1, 11025, 16);
    LoadRawSndData(103, (char*)"DAT\\ENV3.PCM", 1, 11025, 16);
    LoadRawSndData(104, (char*)"DAT\\ENV4.PCM", 1, 11025, 16);
    LoadRawSndData(105, (char*)"DAT\\ENV5.PCM", 1, 11025, 16);
    LoadRawSndData(106, (char*)"DAT\\ENV6.PCM", 1, 11025, 16);
    LoadRawSndData(107, (char*)"DAT\\ENV7.PCM", 1, 11025, 16);
    LoadRawSndData(108, (char*)"DAT\\ENV8.PCM", 1, 11025, 16);
    LoadRawSndData(109, (char*)"DAT\\ENV9.PCM", 1, 11025, 16);
    LoadRawSndData(110, (char*)"DAT\\ENV10.PCM", 1, 11025, 16);
    LoadRawSndData(111, (char*)"DAT\\ENV11.PCM", 1, 11025, 16);
    LoadRawSndData(112, (char*)"DAT\\ENV12.PCM", 1, 11025, 16);
    LoadRawSndData(113, (char*)"DAT\\ENV13.PCM", 1, 11025, 16);
    LoadRawSndData(114, (char*)"DAT\\ENV14.PCM", 1, 11025, 16);

    // 从注册表加载历史最高分
    LoadScore();
}

// =====================================

/*--------------------------------------------------------------
 * ExitGame - 游戏退出清理函数
 * 功能: 释放所有加载的图片资源，关闭视频文件
 *--------------------------------------------------------------*/
void ExitGame(void)
{
    int i;

    // 释放蠕虫动画图片
    for(i=0;i<8;i++)
    {
        ClosePicFile(PicWormB[i]);    // 变形动画
        ClosePicFile(PicWormD[i]);    // 死亡动画
    }

    // 释放标题和帮助图片
    ClosePicFile(PicTitle2);          // 标题背景
    ClosePicFile(PicTitle);           // 标题
    ClosePicFile(PicHelpInfo);       // 帮助信息

    // 释放分数特效图片
    ClosePicFile(PicAdd5);
    ClosePicFile(PicAdd10);
    ClosePicFile(PicAdd15);
    ClosePicFile(PicAdd30);

    ClosePicFile(PicScore);          // 分数背景

    // 释放蠕虫相关图片
    ClosePicFile(PicWormShadow);      // 阴影
    ClosePicFile(PicWormBox);         // 障碍物
    ClosePicFile(PicWormBody);        // 身体
    ClosePicFile(PicMouse);           // 鼠标

    // 关闭视频文件
    CloseVideoFile();
}

// =====================================

// 游戏状态机
long    GameStatus = -1;    // -12~99: 不同游戏状态
// long    GameStatus = 0;   // 调试用：直接开始游戏
long    SceneBright = 0;    // 场景亮度 (0-256)

// 天气和时间系统
long    DayTime = 0;       // 一天中的时间 (0-3500)
long    MoonFull = 1;      // 月亮是否满月
long    RainTime = 0;      // 下雨持续时间

// 金光特效相关变量
long    GoldenLightTime = 0,     // 金光持续时间
        GoldenLightDirect = 0;   // 金光方向
long    GoldenLightX, GoldenLightY,      // 金光位置
        GoldenLightDX, GoldenLightDY;    // 金光移动方向

// =====================================

// 帧间隔时间
long    FramePassTime = 0;

// =====================================

// 声音音量控制
long    WaterSoundVolume = 0;    // 水声音量
long    RainSoundVolume = 0;     // 雨声音量

// =====================================

// 蠕虫游戏相关常量
#define MAXWORMLEN    256         // 蠕虫最大长度
#define WORMSIZE      10         // 蠕虫每个节的大小（像素）

// 蠕虫数据结构
long    WormX[MAXWORMLEN],       // 蠕虫各节X坐标
        WormY[MAXWORMLEN];       // 蠕虫各节Y坐标
long    WormD[MAXWORMLEN],       // 蠕虫各节移动方向 (0=上, 1=右, 2=下, 3=左)
        WormLife[MAXWORMLEN];     // 蠕虫各节生命周期（用于死亡动画）

// 食物和特效网格
// 将屏幕划分为 WORMSIZE x WORMSIZE 的网格
BYTE    FoodXY[RENDER_WIDTH / WORMSIZE][RENDER_HEIGHT / WORMSIZE];    // 食物类型 (0-4=普通, 5=加分, 0xFE=障碍, 0xFD=活体, 0xFF=空)
BYTE    EfctXY[RENDER_WIDTH / WORMSIZE][RENDER_HEIGHT / WORMSIZE];    // 特效类型

short   FoodZ[RENDER_WIDTH / WORMSIZE][RENDER_HEIGHT / WORMSIZE];    // 食物Z坐标（用于入水动画）
short   EfctZ[RENDER_WIDTH / WORMSIZE][RENDER_HEIGHT / WORMSIZE];    // 特效Z坐标

// 搜索队列（用于Beans变化效果）
BYTE    SearchX[2048];           // 搜索队列X坐标
BYTE    SearchY[2048];            // 搜索队列Y坐标
long    SearchStart = 0,         // 搜索队列起始位置
        SearchEnd = 0;            // 搜索队列结束位置

// 蠕虫状态
long    WormLength = 0;          // 当前蠕虫长度
long    WormHoldCounter = 0;     // 蠕虫长度保持计数器（吃东西时暂停增长）

long    WormSpeed = 2;           // 蠕虫移动速度
long    WormSpeedTime = 0;       // 加速/减速持续时间

long    WormStartCounter;        // 蠕虫初始增长计数器
long    WormDieCounter;          // 死亡动画计数器
long    WormEyeStatus = 0;       // 蠕虫眼睛状态（0-6，影响眼睛显示）

long    GameTime1, GameTime2;    // 游戏时间控制

// 吃东西相关
long    EatTime = 0,             // 连续吃同色食物的次数
        EatColor;                 // 当前吃的食物颜色

long    FoodCounter;             // 剩余待投放食物数量

// =====================================

/*--------------------------------------------------------------
 * GoldenLight - 金光特效处理函数
 * 功能: 
 *   - 生成旋转的金色光芒效果
 *   - 碰到食物时会将其变为加分食物(5)
 *   - 播放扭曲音效
 *--------------------------------------------------------------*/
void GoldenLight(void)
{
    long    dx, dy, a;
    long    x1, x2, x3, x4, y1, y2, y3, y4;

    // 如果金光时间已结束，直接返回
    if(GoldenLightTime == 0) return;

    // 如果金光还没有移动方向，增加时间
    if(GoldenLightDX == 0 && GoldenLightDY == 0)
    {
        GoldenLightTime +=2;
    }

    // 超时后重置
    if(GoldenLightTime > 50) GoldenLightTime = 0;

    // 移动金光位置
    GoldenLightX += GoldenLightDX;
    GoldenLightY += GoldenLightDY;

    // 如果金光超出屏幕，结束特效
    if(GoldenLightX < 0 || GoldenLightX >= RENDER_WIDTH ||
       GoldenLightY < 0 || GoldenLightY >= RENDER_HEIGHT)
    {
        GoldenLightTime = 0;
        return;
    }
    
    // 检查金光位置是否有食物，如果有则变为加分食物
    a = FoodXY[GoldenLightX / WORMSIZE][GoldenLightY / WORMSIZE];
    if(a == 0xFE || a < 5)
    {
        FoodXY[GoldenLightX / WORMSIZE][GoldenLightY / WORMSIZE] = 5;
        GoldenLightDX = 0;
        GoldenLightDY = 0;
        // 播放扭曲音效
        if(lpSoundData[13])
        {
            lpSoundData[13]->Stop();
            lpSoundData[13]->SetPan((GoldenLightX-200)*5);
            lpSoundData[13]->SetCurrentPosition(0);
            lpSoundData[13]->Play(NULL, NULL, 0);
        }
    }

    // 计算金光旋转四边形的四个顶点
    dx = GoldenLightX + 4;
    dy = GoldenLightY + 4;

    a = GoldenLightTime + 10;

    x1 = -a; y1 = -a;
    x2 = a; y2 = -a;
    x3 = a; y3 = a;
    x4 = -a; y4 = a;

    // 旋转金光方向
    GoldenLightDirect += 64;
    if(GoldenLightDirect > 255) GoldenLightDirect -= 256;
    
    // 对四个顶点进行旋转
    RotatePoint(&x1, &y1, (BYTE)GoldenLightDirect);
    RotatePoint(&x2, &y2, (BYTE)GoldenLightDirect);
    RotatePoint(&x3, &y3, (BYTE)GoldenLightDirect);
    RotatePoint(&x4, &y4, (BYTE)GoldenLightDirect);
    
    // 绘制旋转的金色四边形
    QuadTCKAD(dx+x1,dy+y1,dx+x2,dy+y2,dx+x3,dy+y3,dx+x4,dy+y4,
              0,64,63,64,63,127,0,127,BRIGHTTXM);
}

// =====================================

/*--------------------------------------------------------------
 * PutInFood - 在随机位置投放一个食物
 * 参数: Color - 食物颜色 (0-4)
 * 功能: 
 *   - 在随机位置投放食物
 *   - 避开蠕虫头部附近区域
 *   - 最多尝试1000次
 *--------------------------------------------------------------*/
void PutInFood(int Color)
{
    long    dx, dy;
    long    foodx, foody;
    long    cnt = 0;

food_loop:
    // 生成随机位置
    foodx = rand() % (RENDER_WIDTH / WORMSIZE);
    foody = rand() % (RENDER_HEIGHT / WORMSIZE);

    cnt++;    // 增加尝试次数

    // 超过最大尝试次数，放弃
    if(cnt > 1000) return;

    // 获取蠕虫头部网格坐标
    dx = WormX[0] / WORMSIZE;
    dy = WormY[0] / WORMSIZE;

    // 如果该位置已有食物，重新选择
    if(FoodXY[foodx][foody] != 0xFF) goto food_loop;

    // 如果在蠕虫头部5格范围内，重新选择（防止食物太近）
    if(abs(foodx-dx)<5 || abs(foody-dy)<5) goto food_loop;

    // 放置食物
    FoodXY[foodx][foody] = Color;
    FoodZ[foodx][foody] = 63;    // 设置Z值，开始入水动画
}

// =====================================

/*--------------------------------------------------------------
 * BeanChange - Beans 变化效果
 * 功能:
 *   - 随机改变搜索队列中一些位置的食物
 *   - 播放淡入淡出音效
 *   - 用于绿色食物产生的特殊效果
 *--------------------------------------------------------------*/
void BeanChange(void)
{
    long    i, j, cnt;
    long    x, y;

    // 计算搜索队列中的有效数据数量
    cnt = SearchEnd - SearchStart;
    if(cnt < 0) cnt += (RENDER_WIDTH / WORMSIZE) * (RENDER_HEIGHT / WORMSIZE);

    // 随机决定改变方式
    if(rand() & 0x01 && cnt >= 9)
    {
        // 方式1: 改变5个位置
        for(i = 0; i < 5; i ++)
        {
            j = SearchStart + (i << 1);
            if(j >= 2048) j = 0;

            x = SearchX[j];
            y = SearchY[j];

            // 随机设置为0-4号食物
            EfctXY[x][y] = rand()%5;
            EfctZ[x][y] = 0;

            // 第一个播放音效
            if(i == 0)
            {
                if(lpSoundData[11])
                {
                    lpSoundData[11]->Stop();
                    lpSoundData[11]->SetPan((x-20)*50);
                    lpSoundData[11]->SetCurrentPosition(0);
                    lpSoundData[11]->Play(NULL, NULL, 0);
                }
            }
        }
        // 更新搜索队列
        for(i = 0; i < 5; i ++)
        {
            SearchX[(SearchStart + 8 - i) & 0x7FF] = SearchX[(SearchStart + 7 - i * 2) & 0x7FF];
            SearchY[(SearchStart + 8 - i) & 0x7FF] = SearchY[(SearchStart + 7 - i * 2) & 0x7FF];
        }

        SearchStart += 5;
        if(SearchStart >= 2048) SearchStart = 0;
    }
    else
    {
        // 方式2: 改变少量位置
        if(cnt > 5) cnt = 5;

        for(i = 0; i < cnt; i ++)
        {
            x = SearchX[SearchStart];
            y = SearchY[SearchStart];
        
            SearchStart ++;
            if(SearchStart >= 2048) SearchStart = 0;

            // 随机设置为0-4号食物
            EfctXY[x][y] = rand()%5;
            EfctZ[x][y] = 0;

            // 第一个播放音效
            if(i == 0)
            {
                if(lpSoundData[11])
                {
                    lpSoundData[11]->Stop();
                    lpSoundData[11]->SetPan((x-20)*50);
                    lpSoundData[11]->SetCurrentPosition(0);
                    lpSoundData[11]->Play(NULL, NULL, 0);
                }
            }
        }
    }
}

// =====================================

/*--------------------------------------------------------------
 * WormDie - 蠕虫死亡处理
 * 功能:
 *   - 设置游戏状态为死亡
 *   - 更新最高分
 *   - 播放死亡动画和音效
 *--------------------------------------------------------------*/
void WormDie(void)
{
    WormDieCounter = 0;    // 重置死亡动画计数器

    GameStatus = 2;        // 设置为死亡状态

    // 更新最高分
    if(HiScore < Score)
    {
        HiScore = Score;
        SaveScore();
    }

    WormEyeStatus = 6;     // 眼睛死亡状态
    
    FramePassTime = 350;   // 死亡动画持续时间

    // 播放死亡音效
    WormHoldCounter = 31;
    if(lpSoundData[5])
    {
        lpSoundData[5]->SetPan((WormX[0]-200)*5);
        lpSoundData[5]->Play(NULL, NULL, 0);
    }

    // 停止心跳声
    if(lpSoundData[10])
    {
        lpSoundData[10]->Stop();
        lpSoundData[10]->SetCurrentPosition(0);
    }
}

// =====================================

/*--------------------------------------------------------------
 * CheckWormLength - 检查并处理蠕虫长度
 * 功能:
 *   - 当蠕虫长度达到24时开始心跳
 *   - 当蠕虫长度达到25时开始吐出身上的食物
 *   - 蠕虫太长时会自动变短
 *--------------------------------------------------------------*/
void CheckWormLength(void)
{
    long    i, x, y;

    // 长度达到24时，开始心跳声
    if(WormLength == 24)
    {
        if(lpSoundData[10])
        {
            lpSoundData[10]->SetPan((WormX[0]-200)*5);
            lpSoundData[10]->Play(NULL, NULL, 1);
        }
    }

    // 长度达到25时，吐出多余的食物
    if(WormLength >= 25)
    {
        // 停止心跳
        if(lpSoundData[10])
        {
            lpSoundData[10]->Stop();
            lpSoundData[10]->SetCurrentPosition(0);
        }

        // 播放冻结音效
        if(lpSoundData[3])
        {
            lpSoundData[3]->SetPan((WormX[5]-200)*5);
            lpSoundData[3]->Play(NULL, NULL, 0);
        }

        // 如果不是在保持期，开始吐出食物
        if(WormHoldCounter <= 0)
        {
            for(i = 5; i < WormLength; i ++)
            {
                x = WormX[i] / WORMSIZE;
                y = WormY[i] / WORMSIZE;

                // 将蠕虫身体变为障碍物(0xFE)
                FoodXY[x][y] = 0xFE;

                // 添加到搜索队列
                SearchX[SearchEnd] = (BYTE)x;
                SearchY[SearchEnd] = (BYTE)y;

                SearchEnd ++;
                if(SearchEnd >= 2048)
                    SearchEnd = 0;
            }
            WormLength = 5;         // 重置长度
            WormHoldCounter = 0;    // 重置保持计数器
        }
    }
}

// =====================================

/*--------------------------------------------------------------
 * WormGrowth - 蠕虫增长
 * 功能: 在蠕虫尾部添加一个新的节
 *--------------------------------------------------------------*/
void WormGrowth(void)
{
    WormHoldCounter = WORMSIZE / WormSpeed;    // 设置保持时间

    // 在尾部添加新节（复制最后一节的位置）
    WormX[WormLength] = WormX[WormLength - 1];
    WormY[WormLength] = WormY[WormLength - 1];

    WormLength++;    // 长度加1
}

// =====================================

/*--------------------------------------------------------------
 * WormThin - 蠕虫变瘦效果
 * 功能: 将蠕虫长度减半，并释放出食物
 * 用于紫色食物的效果
 *--------------------------------------------------------------*/
void WormThin(void)
{
    int     i, len;

    len = (WormLength) >> 1;    // 计算新长度（原长度的一半）

    // 将后半部分变为特效
    for(i = len; i < WormLength; i ++)
    {
        FoodXY[WormX[i] / WORMSIZE][WormY[i] / WORMSIZE] = 0xFF;
        EfctZ[WormX[i] / WORMSIZE][WormY[i] / WORMSIZE] = (i - len) << 1;
        EfctXY[WormX[i] / WORMSIZE][WormY[i] / WORMSIZE] = 201;  // 特效类型201
    }

    WormLength = len;    // 更新长度

    // 停止心跳声
    if(lpSoundData[10])
    {
        lpSoundData[10]->Stop();
        lpSoundData[10]->SetCurrentPosition(0);
    }
}

// =====================================

/*--------------------------------------------------------------
 * WormMoveStep - 蠕虫移动一步
 * 功能:
 *   - 处理加速/减速效果
 *   - 检测并处理食物碰撞
 *   - 处理方向控制和移动
 *   - 处理吃到自己的身体
 *--------------------------------------------------------------*/
void WormMoveStep(void)
{
    long    i, j;

    // ==================== 速度变化处理 ====================
    
    // 减速处理
    if(WormSpeedTime < 0)
    {
        // 添加减速涟漪效果
        AddRipple(WormX[0]+4, WormY[0]+5, 2, 10 * WormSpeedTime - (rand() & 0x1F));
        WormSpeedTime ++;
        if(lpSoundData[22])
        {
            lpSoundData[22]->SetPan((WormX[0]-200)*5);
        }
        if(WormSpeedTime == 0)
        {
            WormSpeed = 2;    // 恢复正常速度
        }
    }
    else
    // 加速处理
    if(WormSpeedTime > 0)
    {
        // 添加加速涟漪效果
        AddRipple(WormX[1]+4, WormY[1]+5, 3, 20 * (40 - WormSpeedTime) + (rand() & 0x1F));
        WormSpeedTime --;
        if(lpSoundData[21])
        {
            lpSoundData[21]->SetPan((WormX[0]-200)*5);
        }
        if(WormSpeedTime == 0)
        {
            WormSpeed = 5;    // 设置为加速速度
        }
    }

    // ==================== 食物碰撞检测 ====================
    
    // 只有当蠕虫头位于网格中心时才检测
    if(WormX[0] % WORMSIZE == 0 && WormY[0] % WORMSIZE == 0)
    {
        // 获取头部所在网格的食物类型
        i = FoodXY[WormX[0] / WORMSIZE][WormY[0] / WORMSIZE];

        // ========== 空地处理（触发减速） ==========
        if(WormSpeedTime > 0 || (WormSpeed > 2 && WormSpeedTime >= 0))
        {
            if(i == 0)
            {
                WormSpeedTime = - 32;    // 开始减速

                if(lpSoundData[22])
                {
                    lpSoundData[21]->Stop();
                    lpSoundData[22]->Stop();
                    lpSoundData[22]->SetPan((WormX[0]-200)*5);
                    lpSoundData[22]->SetCurrentPosition(0);
                    lpSoundData[22]->Play(NULL, NULL, 0);
                }
            }
        }

        // ========== 加分食物(白色，+30分) ==========
        if(i == 5)
        {
            Score += 30;    // 加30分
            EfctXY[WormX[0] / WORMSIZE][WormY[0] / WORMSIZE] = 30;  // 显示+30特效
            EfctZ[WormX[0] / WORMSIZE][WormY[0] / WORMSIZE] = 0;

            // 播放吃东西音效
            if(lpSoundData[2])
            {
                lpSoundData[2]->Stop();
                lpSoundData[2]->SetPan((WormX[0]-200)*5);
                lpSoundData[2]->SetCurrentPosition(0);
                lpSoundData[2]->Play(NULL, NULL, 0);
            }

            // 投放新食物
            PutInFood(rand()%5);
        }

        // ========== 普通食物(0-4号) ==========
        if(i < 5)
        {
            Score += 5;    // 加5分

            // 如果在加速状态，额外加5分
            if(WormSpeed > 2)
            {
                Score += 5;
                EfctXY[WormX[0] / WORMSIZE][WormY[0] / WORMSIZE] = 10;  // 显示+10特效
                EfctZ[WormX[0] / WORMSIZE][WormY[0] / WORMSIZE] = 0;
            }

            // ========== 记录吃的食物颜色 ==========
            if(EatTime < 1)
            {
                EatColor = i;
                EatTime = 1;
            }
            else
            {
                // 如果连续吃同色食物
                if(i == EatColor)
                {
                    EatTime ++;
                    
                    // 连续吃5个同色食物，触发特殊效果
                    if(EatTime >= 5)
                    {
                        EatTime = 0;
                        switch(EatColor)
                        {
                            // 红色: 加速
                            case 0:
                                WormSpeedTime = 40;

                                if(lpSoundData[21])
                                {
                                    lpSoundData[21]->Stop();
                                    lpSoundData[21]->SetPan((WormX[0]-200)*5);
                                    lpSoundData[21]->SetCurrentPosition(0);
                                    lpSoundData[21]->Play(NULL, NULL, 0);
                                }
                                break;

                            // 黄色: 金光特效
                            case 1:
                                GoldenLightTime = 1;
                                GoldenLightDirect = 0;
                                
                                GoldenLightX = WormX[0];
                                GoldenLightY = WormY[0];
                                
                                GoldenLightDX = 0;
                                GoldenLightDY = 0;

                                // 根据移动方向设置金光初始方向
                                switch(WormD[0])
                                {
                                    case 0: GoldenLightDY = -10; break;    // 上
                                    case 1: GoldenLightDX = 10; break;     // 右
                                    case 2: GoldenLightDY = 10; break;     // 下
                                    case 3: GoldenLightDX = -10; break;    // 左
                                }

                                // 播放激光音效
                                if(lpSoundData[12])
                                {
                                    lpSoundData[12]->Stop();
                                    lpSoundData[12]->SetPan((WormX[0]-200)*5);
                                    lpSoundData[12]->SetCurrentPosition(0);
                                    lpSoundData[12]->Play(NULL, NULL, 0);
                                }

                                break;

                            // 绿色: Beans变化
                            case 2:
                                BeanChange();
                                break;

                            // 蓝色: 下雨
                            case 3:
                                RainTime = 500;
                                break;

                            // 紫色: 变瘦
                            case 4:
                                WormThin();
                                break;
                        }
                    }
                }
                else
                {
                    // 换颜色，重新开始计数
                    EatColor = i;
                    EatTime = 1;
                }
            }

            // 播放吃东西音效
            if(lpSoundData[2])
            {
                lpSoundData[2]->Stop();
                lpSoundData[2]->SetPan((WormX[0]-200)*5);
                lpSoundData[2]->SetCurrentPosition(0);
                lpSoundData[2]->Play(NULL, NULL, 0);
            }

            // 投放新食物
            PutInFood(rand()%5);

            // 清除当前位置的食物
            FoodXY[WormX[0] / WORMSIZE][WormY[0] / WORMSIZE] = 0xFF;

            // ========== 下雨时额外加分 ==========
            if(RainTime > 20 && RainTime < 415)
            {
                Score += 10;
                if(WormSpeed <= 2)
                {
                    EfctXY[WormX[0] / WORMSIZE][WormY[0] / WORMSIZE] = 11;  // +10特效
                    EfctZ[WormX[0] / WORMSIZE][WormY[0] / WORMSIZE] = 0;
                }
                else
                {
                    EfctXY[WormX[0] / WORMSIZE][WormY[0] / WORMSIZE] = 12;  // +15特效
                    EfctZ[WormX[0] / WORMSIZE][WormY[0] / WORMSIZE] = 0;
                }
            }

            // ========== 蠕虫增长 ==========
            if(WormHoldCounter<=0)
            {
                WormGrowth();
            }
            else
            {
                WormStartCounter ++;
            }
        }
        else
        // ========== 碰到自己的身体或障碍物 ==========
        if(i == 0xFE || i == 0xFD)        // 0xFE = 灰色身体(障碍物) 0xFD = 活体
        {
            WormDie();    // 死亡
            return;
        }

        // ========== 更新食物网格 ==========
        FoodXY[WormX[0] / WORMSIZE][WormY[0] / WORMSIZE] = 0xFD;    // 标记为活体
        FoodXY[WormX[WormLength - 1] / WORMSIZE][WormY[WormLength - 1] / WORMSIZE] = 0xFF;  // 尾部变空

        // ========== 更新身体各节方向 ==========
        for(i=WormLength; i>0; i--)
        {
            WormD[i] = WormD[i-1];
        }

        // ========== 方向控制 ==========
        if(WormX[0] % WORMSIZE == 0 && WormY[0] % WORMSIZE == 0)
        {
            j = GetKey();
            
            // 上 (不能向反方向走)
            if(j == KEY_UP)
            {
                if(WormD[0] != 2)
                {
                    WormD[0] = 0;
                }
            }
            else
            // 右
            if(j == KEY_RIGHT)
            {
                if(WormD[0] != 3)
                {
                    WormD[0] = 1;
                }
            }
            else
            // 下
            if(j == KEY_DOWN)
            {
                if(WormD[0] != 0)
                {
                    WormD[0] = 2;
                }
            }
            else
            // 左
            if(j == KEY_LEFT)
            {
                if(WormD[0] != 1)
                {
                    WormD[0] = 3;
                }
            }
        }
    }

    // ========== 移动头部 ==========
    switch(WormD[0])
    {
        case 0: WormY[0] -= WormSpeed; break;    // 上
        case 1: WormX[0] += WormSpeed; break;    // 右
        case 2: WormY[0] += WormSpeed; break;    // 下
        case 3: WormX[0] -= WormSpeed; break;    // 左
    }

    // ========== 屏幕边界处理（穿墙） ==========
    if(WormX[0] < 0) WormX[0] += RENDER_WIDTH;
    if(WormY[0] < 0) WormY[0] += RENDER_HEIGHT;
    if(WormX[0] > RENDER_WIDTH - WORMSIZE) WormX[0] -= RENDER_WIDTH;
    if(WormY[0] > RENDER_HEIGHT - WORMSIZE) WormY[0] -= RENDER_HEIGHT;

    // 心跳声随蠕虫位置变化
    if(WormLength == 24)
    {
        if(lpSoundData[10])
        {
            lpSoundData[10]->SetPan((WormX[0]-200)*5);
        }
    }

    // ========== 移动身体 ==========
    for(i=1; i<WormLength; i++)
    {
        // 如果是最后一节且在保持期
        if(i >= WormLength - 1 && WormHoldCounter > 0) 
        {
            WormHoldCounter --;
            i = WormLength;
        }
        else
        {
            // 跟随前一节的方向移动
            switch(WormD[i])
            {
                case 0: WormY[i] -= WormSpeed; break;    // 上
                case 1: WormX[i] += WormSpeed; break;    // 右
                case 2: WormY[i] += WormSpeed; break;    // 下
                case 3: WormX[i] -= WormSpeed; break;    // 左
            }

            // 穿墙处理
            if(WormX[i] < 0) WormX[i] += RENDER_WIDTH;
            if(WormY[i] < 0) WormY[i] += RENDER_HEIGHT;
            if(WormX[i] > RENDER_WIDTH - WORMSIZE) WormX[i] -= RENDER_WIDTH;
            if(WormY[i] > RENDER_HEIGHT - WORMSIZE) WormY[i] -= RENDER_HEIGHT;
        }
    }

    // 检查蠕虫长度
    CheckWormLength();
}

// =====================================

/*--------------------------------------------------------------
 * 全局临时分数显示缓冲区
 *--------------------------------------------------------------*/
DWORD   PicScoreTemp[205*11+2];

/*--------------------------------------------------------------
 * ShowScore - 显示分数
 * 功能:
 *   - 在屏幕左上角显示当前分数和最高分
 *   - 显示连续吃同色食物的计数
 *--------------------------------------------------------------*/
void ShowScore(void)
{
    char        Buf[128];      // 格式化字符串缓冲区
    LPDWORD     temp;           // 保存渲染缓冲区指针
    int         x = 6, y = 5, i;

    // 保存渲染缓冲区
    temp = lpRenderBuffer;
    
    // 切换到特效页面2
    SetRenderPage(SFX2PAGE);

    // 绘制分数背景
    PutImage(3,3,PicScore);

    // 格式化分数字符串
    sprintf_s(Buf,"HISCORE %5ld   SCORE %5ld", HiScore, Score);
    
    // 将空格替换为0
    for(i=8; i<12; i++)
    {
        if(Buf[i] == ' ')
        {
            Buf[i] = '0';
        }
        if(Buf[i+14] == ' ')
        {
            Buf[i+14] = '0';
        }
    }
    
    // 设置颜色并输出文字
    SetColor(0);    // 黑色
    _OutTextXY(x, y, Buf);

    // ========== 显示连续吃同色食物计数 ==========
    if(EatTime < 4 || (EatTime == 4 && ((DayTime & 0x07) < 4)))
    {
        x = 174;
        for(i=0; i<EatTime; i++)
        {
            // 绘制对应颜色的食物图标
            QuadTCK(x,y,x+6,y,x+6,y+6,x,y+6,
                    0+EatColor*10,0,9+EatColor*10,0,9+EatColor*10,9,0+EatColor*10,9,FOODTXM);
            x += 8;
        }
    }

    // 保存分数显示区域
    _GetImage(3, 3, 205, 11, PicScoreTemp);

    // 恢复渲染缓冲区
    lpRenderBuffer = temp;

    x = WormX[0]; y = WormY[0];    // 获取蠕虫头位置

    // ========== 根据蠕虫位置调整分数显示 ==========
    if(WormDieCounter == 0)
    {
        if(y < 100)
        {
            // 蠕虫在上方，透明度随高度变化
            PutImageAB(3, 3, PicScoreTemp, (y << 1));
        }
        else
        if(y > 200)
        {
            // 蠕虫在下方
            PutImageAB(3, 3, PicScoreTemp, 200 - ((y - 200) << 1));
        }
        else
        {
            // 蠕虫在中间，正常显示
            PutImageAB(3, 3, PicScoreTemp, 200);
        }
    }
    else
    {
        // 死亡状态下，根据帧时间和蠕虫位置显示
        i = 200;
        if(y < 100)
        {
            i = (y << 1);
        }
        else
        if(y > 200)
        {
            i = 200 - ((y - 200) << 1);
        }
        if(i < 200 && FramePassTime < 320) i += (320 - FramePassTime);
        PutImageAB(3, 3, PicScoreTemp, i);
    }
}

// =====================================

/*--------------------------------------------------------------
 * DrawBackground - 绘制背景
 *--------------------------------------------------------------*/
void DrawBackground(void)
{
    // 根据场景亮度绘制背景
    PutImageBR(0, 0, PicBack, SceneBright);
}

// =====================================

/*--------------------------------------------------------------
 * DrawEfct - 绘制特效（在水面以下）
 * 功能:
 *   - 处理食物从水面下浮起的动画
 *   - 处理蠕虫身体消散特效
 *--------------------------------------------------------------*/
void DrawEfct(void)
{
    long    dx, dy;
    long    k, z;

    // 遍历整个网格
    for(dy=0; dy<RENDER_HEIGHT; dy+=WORMSIZE)
    for(dx=0; dx<RENDER_WIDTH; dx+=WORMSIZE)
    {
        k = EfctXY[dx / WORMSIZE][dy / WORMSIZE];

        if(k < 0xFF)
        {
            z = EfctZ[dx / WORMSIZE][dy / WORMSIZE];

            switch(k)
            {
                // ========== 食物特效(0-4) ==========
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                    QuadTCKAB(dx,dy,dx+9,dy,dx+9,dy+9,dx,dy+9,
                              0+k*10,0,9+k*10,0,9+k*10,9,0+k*10,9,FOODTXM,(z<<2));
                    EfctZ[dx / WORMSIZE][dy / WORMSIZE] = z + 1;
                    if(z == 63)
                    {
                        // 特效结束，变为实际食物
                        EfctXY[dx / WORMSIZE][dy / WORMSIZE] = 0xFF;
                        FoodXY[dx / WORMSIZE][dy / WORMSIZE] = (BYTE)k;
                        FoodZ[dx / WORMSIZE][dy / WORMSIZE] = 0;
                    }
                    break;

                // ========== 身体消散特效(201) ==========
                case 201:
                    EfctZ[dx / WORMSIZE][dy / WORMSIZE] = z + 1;
                    AddRipple(dx + 4, dy + 5, 3, rand()&0xFF);
                    if(z >= 8)
                    {
                        // 播放泡泡破裂声
                        if(lpSoundData[4])
                        {
                            lpSoundData[4]->Stop();
                            lpSoundData[4]->SetPan((dx-200)*5);
                            lpSoundData[4]->SetCurrentPosition(0);
                            lpSoundData[4]->Play(NULL, NULL, 0);
                        }
                        EfctXY[dx / WORMSIZE][dy / WORMSIZE] = 0xFF;
                    }
                    else
                    {
                        // 显示身体图片
                        PutImageCKAD(dx, dy, PicWormBody);
                    }
                    break;
            }
        }
    }
}

// =====================================

/*--------------------------------------------------------------
 * DrawEfctAbove - 绘制水面以上的特效
 * 功能: 绘制加分数字飘动效果
 *--------------------------------------------------------------*/
void DrawEfctAbove(void)
{
    long    dx, dy;
    long    k, z;

    for(dy=0; dy<RENDER_HEIGHT; dy+=WORMSIZE)
    for(dx=0; dx<RENDER_WIDTH; dx+=WORMSIZE)
    {
        k = EfctXY[dx / WORMSIZE][dy / WORMSIZE];

        if(k < 0xFF)
        {
            z = EfctZ[dx / WORMSIZE][dy / WORMSIZE];

            switch(k)
            {
                // ========== +5分特效 ==========
                case 10:
                    if(z < 16)
                        PutImageCK(dx-2,dy-z, PicAdd5);
                    else
                        PutImageCKAB(dx-2,dy-z, PicAdd5, 256-((z-16)<<4));
                    if(z < 32) 
                    {
                        EfctZ[dx / WORMSIZE][dy / WORMSIZE] = z + 1;
                    }
                    else
                    {
                        EfctXY[dx / WORMSIZE][dy / WORMSIZE] = 0xFF;
                    }
                    break;
                    
                // ========== +10分特效 ==========
                case 11:
                    if(z < 16)
                        PutImageCK(dx-4,dy-z, PicAdd10);
                    else
                        PutImageCKAB(dx-4,dy-z, PicAdd10, 256-((z-16)<<4));
                    if(z < 32) 
                    {
                        EfctZ[dx / WORMSIZE][dy / WORMSIZE] = z + 1;
                    }
                    else
                    {
                        EfctXY[dx / WORMSIZE][dy / WORMSIZE] = 0xFF;
                    }
                    break;
                    
                // ========== +15分特效 ==========
                case 12:
                    if(z < 16)
                        PutImageCK(dx-4,dy-z, PicAdd15);
                    else
                        PutImageCKAB(dx-4,dy-z, PicAdd15, 256-((z-16)<<4));
                    if(z < 32) 
                    {
                        EfctZ[dx / WORMSIZE][dy / WORMSIZE] = z + 1;
                    }
                    else
                    {
                        EfctXY[dx / WORMSIZE][dy / WORMSIZE] = 0xFF;
                    }
                    break;
                    
                // ========== +30分特效 ==========
                case 30:
                    if(z < 16)
                        PutImageCK(dx-4,dy-z, PicAdd30);
                    else
                        PutImageCKAB(dx-4,dy-z, PicAdd30, 256-((z-16)<<4));
                    if(z < 32) 
                    {
                        EfctZ[dx / WORMSIZE][dy / WORMSIZE] = z + 1;
                    }
                    else
                    {
                        EfctXY[dx / WORMSIZE][dy / WORMSIZE] = 0xFF;
                    }
                    break;
            }
        }
    }
}

// =====================================

/*--------------------------------------------------------------
 * DrawShadow - 绘制阴影
 * 功能: 绘制食物和蠕虫的阴影
 *--------------------------------------------------------------*/
void DrawShadow(void)
{
    long    dx, dy;
    long    j;

    // 绘制食物阴影
    for(dy=0; dy<RENDER_HEIGHT; dy+=WORMSIZE)
    for(dx=0; dx<RENDER_WIDTH; dx+=WORMSIZE)
    {
        // 普通食物阴影
        if(FoodXY[dx / WORMSIZE][dy / WORMSIZE] != 0xFF)
        {
            if(FoodXY[dx / WORMSIZE][dy / WORMSIZE] <= 5)
            {
                j = FoodZ[dx / WORMSIZE][dy / WORMSIZE];
                if(j < 6)
                    // 浅水：使用阴影图片
                    PutImageCKSB(dx + 4 + j, dy + 4 + j, PicWormShadow);
                else
                    // 深水：使用深水颜色图片
                    PutImageCKSB(dx + 4 + j, dy + 4 + j, PicWormD[7-((63-j)>>3)]);
            }
            else
            if(FoodXY[dx / WORMSIZE][dy / WORMSIZE] == 0xFE)
            {
                // 障碍物阴影
                PutImageCKSB(dx + 4, dy + 4, PicWormShadow);
            }
        }
        
        // 身体消散特效阴影
        if(EfctXY[dx / WORMSIZE][dy / WORMSIZE] == 201)
        {
            PutImageCKSB(dx + 4, dy + 4, PicWormShadow);
        }
    }
}

// =====================================

/*--------------------------------------------------------------
 * DrawFoodUnderWater - 绘制水面以下的食物
 * 功能: 
 *   - 绘制在水中的食物，带有水波折射效果
 *   - 处理食物入水动画
 *--------------------------------------------------------------*/
void DrawFoodUnderWater(void)
{
    long    dx, dy;
    long    j, k;

    for(dy=0; dy<RENDER_HEIGHT; dy+=WORMSIZE)
    for(dx=0; dx<RENDER_WIDTH; dx+=WORMSIZE)
    {
        if(FoodXY[dx / WORMSIZE][dy / WORMSIZE] != 0xFF)
        {
            if(FoodXY[dx / WORMSIZE][dy / WORMSIZE] <= 6)
            {
                j = FoodZ[dx / WORMSIZE][dy / WORMSIZE];
                k = FoodXY[dx / WORMSIZE][dy / WORMSIZE];
                k = k * WORMSIZE;
                
                // 水滴落入水中的声音
                if(j == 5)
                {
                    if(lpSoundData[1])
                    {
                        lpSoundData[1]->Stop();
                        lpSoundData[1]->SetCurrentPosition(0);
                        lpSoundData[1]->SetPan((dx-200)*5);
                        lpSoundData[1]->Play(NULL, NULL, 0);
                    }
                }
                
                if(j < 6)
                {
                    // 绘制带透明度的食物（模拟水中折射）
                    QuadTCKAB(dx-j,dy-j,dx+9+j,dy-j,dx+9+j,dy+9+j,dx-j,dy+9+j,
                              0+k,0,9+k,0,9+k,9,0+k,9,FOODTXM, 255-(j<<2));

                    if(j > 0)
                    {
                        // Z值减小，模拟食物沉入水中
                        FoodZ[dx / WORMSIZE][dy / WORMSIZE] --;
                        AddRipple(dx+4, dy+5, 3, -((rand()&0x3F)+0xFF));
                    }
                }
            }
            else
            if(FoodXY[dx / WORMSIZE][dy / WORMSIZE] == 0xFE)
            {
                // 绘制障碍物
                PutImageCK(dx,dy,PicWormBox);
            }
        }
    }
}

// =====================================

/*--------------------------------------------------------------
 * DrawEye - 绘制蠕虫眼睛
 * 功能:
 *   - 根据蠕虫移动方向绘制眼睛
 *   - 根据周围环境改变眼睛状态（看向食物方向）
 *--------------------------------------------------------------*/
void DrawEye(void)
{
    long    dx, dy;
    long    u1, u2, u3, u4, v1, v2, v3, v4;
    long    ax, ay;       // 前方方向向量
    long    axl, ayl,     // 左前方方向向量
            axr, ayr;     // 右前方方向向量
    long    i;

    // 获取头部网格坐标
    dx = WormX[0] / WORMSIZE;
    dy = WormY[0] / WORMSIZE;

    // 更新眼睛闪烁状态
    switch(WormEyeStatus)
    {
        case 1:
        case 2:
        case 3:
            WormEyeStatus = 0;
            break;
        case 4:
            WormEyeStatus = 2;
            break;
        case 5:
            WormEyeStatus = 3;
            break;
    }

    // 根据移动方向设置向量
    switch(WormD[0])
    {
        case 0:    // 上
            ax = 0; ay = -1;
            axl = -1; ayl = 0; axr = 1; ayr = 0;
            break;
        case 1:    // 右
            ax = 1; ay = 0;
            axl = 0; ayl = -1; axr = 0; ayr = 1;
            break;
        case 2:    // 下
            ax = 0; ay = 1;
            axl = 1; ayl = 0; axr = -1; ayr = 0;
            break;
        case 3:    // 左
            ax = -1; ay = 0;
            axl = 0; ayl = 1; axr = 0; ayr = -1;
            break;
    }

    i = 0;

    // ========== 检测周围食物并决定眼睛看向的方向 ==========
    if(FoodXY[dx+ax][dy+ay]>0xFD)
    {
        // 前方有障碍，检查周围
        if(FoodXY[dx+ax+axl][dy+ay+ayl]<0x70 && dx+ax+axl>=0 && dx+ax+axl<RENDER_WIDTH/WORMSIZE && dy+ay+ayl>=0 && dy+ay+ayl<RENDER_HEIGHT/WORMSIZE)
        {
            WormEyeStatus = 3;    // 向左看
        }
        else
        if(FoodXY[dx+ax+axr][dy+ay+ayr]<0x70 && dx+ax+axr>=0 && dx+ax+axr<RENDER_WIDTH/WORMSIZE && dy+ay+ayr>=0 && dy+ay+ayr<RENDER_HEIGHT/WORMSIZE)
        {
            WormEyeStatus = 2;    // 向右看
        }
        else
        if(FoodXY[dx+axl][dy+ayl]<0x70 && dx+axl>=0 && dx+axl<RENDER_WIDTH/WORMSIZE && dy+ayl>=0 && dy+ayl<RENDER_HEIGHT/WORMSIZE)
        {
            WormEyeStatus = 5;    // 向左后看
        }
        else
        if(FoodXY[dx+axr][dy+ayr]<0x70 && dx+axr>=0 && dx+axr<RENDER_WIDTH/WORMSIZE && dy+ayr>=0 && dy+ayr<RENDER_HEIGHT/WORMSIZE)
        {
            WormEyeStatus = 4;    // 向右后看
        }
        else
        if(FoodXY[dx-ax+axl][dy-ay+ayl]<0x70 && dx-ax+axl>=0 && dx-ax+axl<RENDER_WIDTH/WORMSIZE && dy-ay+ayl>=0 && dy-ay+ayl<RENDER_HEIGHT/WORMSIZE)
        {
            WormEyeStatus = 5;    // 向左后看
        }
        else
        if(FoodXY[dx-ax+axr][dy-ay+ayr]<0x70 && dx-ax+axr>=0 && dx-ax+axr<RENDER_WIDTH/WORMSIZE && dy-ay+ayr>=0 && dy-ay+ayr<RENDER_HEIGHT/WORMSIZE)
        {
            WormEyeStatus = 4;    // 向右后看
        }
    }

    // 如果正在增长中且位置不在网格中心，眼睛呈惊恐状
    if(WormHoldCounter && WormLength >= 5 && (WormX[0] % WORMSIZE != 0 || WormY[0] % WORMSIZE != 0))
    {
        WormEyeStatus = 1;
    }

    dx = WormX[0]; dy = WormY[0];

    // 设置纹理坐标（根据方向）
    switch(WormD[0])
    {
        case 0:    // 上
            u1 = 9; v1 = 19; u2 = 0; v2 = 19;
            u3 = 0; v3 = 10; u4 = 9; v4 = 10;
            break;
        case 1:    // 右
            u1 = 9; v1 = 10; u2 = 9; v2 = 19;
            u3 = 0; v3 = 19; u4 = 0; v4 = 10;
            break;
        case 2:    // 下
            u1 = 0; v1 = 10; u2 = 9; v2 = 10;
            u3 = 9; v3 = 19; u4 = 0; v4 = 19;
            break;
        case 3:    // 左
            u1 = 0; v1 = 19; u2 = 0; v2 = 10;
            u3 = 9; v3 = 10; u4 = 9; v4 = 19;
            break;
    }

    // 绘制眼睛
    switch(WormEyeStatus)
    {
        case 0:
            QuadTCK(dx,dy,dx+9,dy,dx+9,dy+9,dx,dy+9,
                u1,v1,u2,v2,u3,v3,u4,v4,FOODTXM);
            break;
        case 1:
            QuadTCK(dx,dy,dx+9,dy,dx+9,dy+9,dx,dy+9,
                u1+10,v1,u2+10,v2,u3+10,v3,u4+10,v4,FOODTXM);
            break;
        case 2:
            QuadTCK(dx,dy,dx+9,dy,dx+9,dy+9,dx,dy+9,
                u1+20,v1,u2+20,v2,u3+20,v3,u4+20,v4,FOODTXM);
            break;
        case 3:
            QuadTCK(dx,dy,dx+9,dy,dx+9,dy+9,dx,dy+9,
                u1+30,v1,u2+30,v2,u3+30,v3,u4+30,v4,FOODTXM);
            break;
        case 4:
            QuadTCK(dx,dy,dx+9,dy,dx+9,dy+9,dx,dy+9,
                u1+40,v1,u2+40,v2,u3+40,v3,u4+40,v4,FOODTXM);
            break;
        case 5:
            QuadTCK(dx,dy,dx+9,dy,dx+9,dy+9,dx,dy+9,
                u1+50,v1,u2+50,v2,u3+50,v3,u4+50,v4,FOODTXM);
            break;
        case 6:    // 死亡状态
            QuadTCK(dx-2,dy-2,dx+9+2,dy-2,dx+9+2,dy+9+2,dx-2,dy+9+2,
                u1,v1,u2,v2,u3,v3,u4,v4,FOODTXM);
            break;
    }
}

// =====================================

/*--------------------------------------------------------------
 * DrawWormShadow - 绘制蠕虫阴影
 *--------------------------------------------------------------*/
void DrawWormShadow(void)
{
    long    i;

    // 绘制所有节的阴影
    for(i=0; i<WormLength; i++)
    {
        PutImageCKSB(WormX[i]+4, WormY[i]+4, PicWormShadow);
    }
}

// =====================================

/*--------------------------------------------------------------
 * DrawWorm - 绘制蠕虫（正常状态）
 *--------------------------------------------------------------*/
void DrawWorm(void)
{
    long    i, j;

    // 绘制蠕虫身体
    j = 9;
    if(WormLength < j) j = WormLength;

    // 绘制头部
    PutImageCKAD(WormX[0], WormY[0], PicWormBody);

    // 绘制身体前9节（使用变形图片）
    for(i = 1; i < j; i ++)
    {
        PutImageCKAD(WormX[i], WormY[i], PicWormB[i-1]);
    }

    // 再次绘制整个身体（叠加效果）
    for(i = 0; i < WormLength; i ++)
    {
        PutImageCKAD(WormX[i], WormY[i], PicWormBody);
    }

    // 绘制眼睛
    DrawEye();
}

// =====================================

/*--------------------------------------------------------------
 * DrawWormShadow2 - 绘制蠕虫阴影（死亡状态）
 *--------------------------------------------------------------*/
void DrawWormShadow2(void)
{
    long    i;

    // 只有当蠕虫生命值小于8时才绘制
    if(WormLife[WormLength - 1] < 8)
    {
        for(i = 0; i < WormLength; i ++)
        {
            if(WormLife[i] > 0)
                PutImageSB(WormX[i] + 4, WormY[i] + 4, PicWormD[WormLife[i]-1]);
            else
                PutImageSB(WormX[i] + 4, WormY[i] + 4, PicWormShadow);
        }
    }
}

// =====================================

/*--------------------------------------------------------------
 * DrawWorm2 - 绘制蠕虫（死亡/消散动画）
 * 功能:
 *   - 处理蠕虫死亡的消散动画
 *   - 从尾部开始逐渐消散
 *--------------------------------------------------------------*/
void DrawWorm2(void)
{
    long    i;

    // 如果正在保持期，使用普通绘制
    if(WormHoldCounter > 0)
    {
        DrawWorm();
        WormHoldCounter --;
        return;
    }

    // 开始死亡消散动画
    if(WormDieCounter < 8)
    {
        WormLife[0] = WormDieCounter;
        WormDieCounter ++;
    }

    // 处理消散动画
    if(WormLife[WormLength - 1] < 8)
    {
        // 从尾部开始向前传播消散状态
        for(i = WormLength - 1; i >= 1; i --)
        {
            if(WormLife[i - 1] >= 3 && WormLife[i] < 8)
                WormLife[i] ++;
        }

        for(i = 0; i < WormLength; i ++)
        {
            // 第一帧播放泡泡声
            if(WormLife[i] == 1)
            {
                if(lpSoundData[4])
                {
                    lpSoundData[4]->Stop();
                    lpSoundData[4]->SetPan((WormX[i]-200)*5);
                    lpSoundData[4]->SetCurrentPosition(0);
                    lpSoundData[4]->Play(NULL, NULL, 0);
                }
            }

            // 添加涟漪效果
            if(WormLife[i] >= 2)
                AddRipple(WormX[i] + 4, WormY[i] + 5, 2, rand()&0x1FF);

            // 根据生命值绘制不同状态
            if(WormLife[i] > 0)
                PutImageCKAD(WormX[i],WormY[i], PicWormB[WormLife[i] - 1]);
            else
                PutImageCKAD(WormX[i],WormY[i], PicWormBody);
        }
    }
}

// =====================================

/*--------------------------------------------------------------
 * DrawFoodAboveWater - 绘制水面以上的食物
 * 功能: 绘制浮出水面的食物动画
 *--------------------------------------------------------------*/
void DrawFoodAboveWater(void)
{
    long    dx, dy;
    long    j, k;

    for(dy=0; dy<RENDER_HEIGHT; dy+=WORMSIZE)
    for(dx=0; dx<RENDER_WIDTH; dx+=WORMSIZE)
    {
        k = FoodXY[dx / WORMSIZE][dy / WORMSIZE];
        if(k<=5)
        {
            j = FoodZ[dx / WORMSIZE][dy / WORMSIZE];
            k = k * WORMSIZE;

            // 只有当Z值大于等于6时才在水面以上显示
            if(j >= 6)
            {
                QuadTCKAB(dx-j,dy-j,dx+9+j,dy-j,dx+9+j,dy+9+j,dx-j,dy+9+j,
                        0+k,0,9+k,0,9+k,9,0+k,9,FOODTXM, 255-(j<<2));
                
                // 逐渐浮出水面
                if(j > 0) 
                    FoodZ[dx / WORMSIZE][dy / WORMSIZE] -= (1 + ((63-FoodZ[dx / WORMSIZE][dy / WORMSIZE])>>4));
            }
        }
    }
}

// =====================================

/*--------------------------------------------------------------
 * DrawNightLight - 绘制夜间光照效果
 * 功能: 在夜间模式下，蠕虫周围有一个发光区域
 *--------------------------------------------------------------*/
void DrawNightLight(void)
{
    long    dx, dy, a;

    if(SceneBright<250)
    {
        dx = WormX[0]+4; dy = WormY[0]+4;
        a= (256 - SceneBright) >> 1;
        QuadTCKAD(dx-a,dy-a,dx+1+a,dy-a,dx+1+a,dy+1+a,dx-a,dy+1+a,
            0,0,63,0,63,63,0,63,BRIGHTTXM);
    }
}

// =====================================

/*--------------------------------------------------------------
 * DrawNightLight2 - 绘制夜间光照效果（死亡状态）
 *--------------------------------------------------------------*/
void DrawNightLight2(void)
{
    long    dx, dy, a;

    if(SceneBright<250 && WormHoldCounter > 0)
    {
        dx = WormX[0]+4; dy = WormY[0]+4;
        a = (256 - SceneBright) >> 1;
        a = ((a * WormHoldCounter) >> 5);
        QuadTCKAD(dx-a,dy-a,dx+1+a,dy-a,dx+1+a,dy+1+a,dx-a,dy+1+a,
            0,0,63,0,63,63,0,63,BRIGHTTXM);
    }
}

// =====================================

/*--------------------------------------------------------------
 * DrawSun - 绘制日出日落效果
 * 功能: 
 *   - 在黄昏时分显示太阳/月亮升起和落下的动画
 *   - 根据时间显示满月或弯月
 *--------------------------------------------------------------*/
void DrawSun(void)
{
    long    dx, dy, a;

    // 每2700帧月亮形态翻转一次
    if(DayTime==2700)        
        MoonFull = ~MoonFull;

    // 黄昏时分显示太阳/月亮
    if(DayTime>2700 && DayTime<2700+500)
    {
        dy = 2950 + 100 - DayTime;
        dx = 200;
        
        a= 32;

        if(MoonFull)
            // 满月
            QuadTCKAB(dx-a,dy-a,dx+a,dy-a,dx+a,dy+a,dx-a,dy+a,
                64,0,127,0,127,63,64,63,BRIGHTTXM,(150-abs(dy-150)));
        else
            // 弯月
            QuadTCKAB(dx-a,dy-a,dx+a,dy-a,dx+a,dy+a,dx-a,dy+a,
                64,64,127,64,127,127,64,127,BRIGHTTXM,(150-abs(dy-150)));
    }
}

// =====================================

/*--------------------------------------------------------------
 * DrawScene - 绘制主场景（正常状态）
 * 渲染顺序（从后到前）:
 *   1. 背景
 *   2. 阴影
 *   3. 蠕虫阴影
 *   4. 夜间光照
 *   5. 水下食物
 *   6. 蠕虫
 *   7. 特效
 *   8. 太阳/月亮
 *   9. 金光特效
 *   10. 水波效果
 *   11. 水上食物和特效
 *   12. 分数显示
 *--------------------------------------------------------------*/
void DrawScene(void)
{
    SetRenderPage(BACKPAGE);

    DrawBackground();              // 绘制背景

    DrawShadow();                  // 绘制阴影
    DrawWormShadow();              // 绘制蠕虫阴影

    DrawNightLight();              // 夜间光照

    DrawFoodUnderWater();          // 水下食物

    DrawWorm();                    // 绘制蠕虫

    DrawEfct();                    // 绘制水下特效

    DrawSun();                     // 太阳/月亮

    GoldenLight();                 // 金光特效

    // 应用水波效果
    SetRenderPage(MAINPAGE);
    RippleSpread();
    RenderRipple();
    SetClipBox(1, 1, RENDER_WIDTH - 2, RENDER_HEIGHT - 2);

    DrawEfctAbove();               // 水上特效
    DrawFoodAboveWater();          // 水上食物

    ShowScore();                   // 显示分数
    
    ResetClipBox();
}

// =====================================

/*--------------------------------------------------------------
 * DrawScene2 - 绘制主场景（死亡状态）
 *--------------------------------------------------------------*/
void DrawScene2(void)
{
    SetRenderPage(BACKPAGE);

    DrawBackground();              // 绘制背景

    DrawShadow();                  // 绘制阴影
    DrawWormShadow2();             // 绘制死亡状态阴影

    DrawNightLight2();             // 夜间光照

    DrawFoodUnderWater();          // 水下食物
    
    DrawWorm2();                   // 绘制死亡动画

    DrawEfct();                    // 绘制特效

    DrawSun();                     // 太阳/月亮

    GoldenLight();                 // 金光特效

    // 应用水波效果
    SetRenderPage(MAINPAGE);
    RippleSpread();
    RenderRipple();
    SetClipBox(1, 1, RENDER_WIDTH - 2, RENDER_HEIGHT - 2);

    DrawEfctAbove();               // 水上特效
    DrawFoodAboveWater();          // 水上食物

    ShowScore();                   // 显示分数

    ResetClipBox();
}

// =====================================

/*--------------------------------------------------------------
 * Rain - 下雨效果处理
 * 功能:
 *   - 管理下雨的持续时间
 *   - 播放雨声和雷声
 *   - 添加雨滴涟漪效果
 *   - 随机投放蓝色食物
 *--------------------------------------------------------------*/
void Rain(void)
{
    // 下雨持续中
    if(RainTime > 0)
    {
        RainTime --;

        // 开始下雨时播放雷声
        if(RainTime == 499)
        {
            if(lpSoundData[6])
            {
                lpSoundData[6]->Play(NULL, NULL, 0);
            }
        }

        // 开始雨声循环
        if(RainTime == 450)
        {
            RainSoundVolume = -10000;
            if(lpSoundData[7])
            {
                lpSoundData[7]->SetVolume(RainSoundVolume);
                lpSoundData[7]->Play(NULL, NULL, 1);
            }
        }

        // 雨声渐强
        if(RainTime < 450 && RainTime > 100)
        {
            if(RainSoundVolume<0)
            {
                RainSoundVolume += 400;
                if(lpSoundData[7])
                {
                    lpSoundData[7]->SetVolume(RainSoundVolume);
                }
            }
        }

        // 下雨中：添加涟漪和随机投放食物
        if(RainTime < 440)
        {
            // 随机投放蓝色食物
            if(RainTime > 100 && (RainTime & 0x3F) == 0)
                PutInFood(3);

            // 添加雨滴涟漪
            for(int i=0;i<2;i++)
                AddRipple(rand() % RENDER_WIDTH, rand() % RENDER_HEIGHT, 3, -rand()&0xFF);

            // 随机播放雷声
            if(RainTime>150 && rand() < 1500)
            {
                if(lpSoundData[8])
                {
                    lpSoundData[8]->Play(NULL, NULL, 0);
                }
            }
        }

        // 雨停前渐弱
        if(RainTime < 50)
        {
            if(RainSoundVolume > -10000)
            {
                RainSoundVolume -= 200;
                if(lpSoundData[7])
                {
                    lpSoundData[7]->SetVolume(RainSoundVolume);
                }
            }
        }

        // 雨停
        if(RainTime == 0)
        {
            if(lpSoundData[7])
            {
                lpSoundData[7]->Stop();
            }
        }
    }
}

// =====================================

/*--------------------------------------------------------------
 * TimePass - 时间流逝处理
 * 功能:
 *   - 管理一天的昼夜循环
 *   - 控制场景亮度变化
 *   - 切换背景帧
 *--------------------------------------------------------------*/
void TimePass(void)
{
    // 时间递增，0-3500循环
    DayTime ++;
    if(DayTime >= 3500) DayTime = 0;

    // 黄昏时变暗
    if(DayTime > 3500 - 16)
    {
        if(SceneBright > 0) SceneBright -= 4;
    }

    // 新的一天开始，切换背景帧
    if(DayTime == 0)
    {
        BG_CNT ++;
        if(BG_CNT >= EndVideoFrame) BG_CNT=0;
        LoadBG();
    }

    // 黎明时变亮
    if(DayTime <= 256)
    {
        if(SceneBright < 64) 
            SceneBright += 4;
        else
        if(SceneBright < 256) 
            SceneBright ++;
    }

    // 傍晚时变暗
    if(DayTime > 2500 - 256)
    {
        if(SceneBright > 64) SceneBright --;
    }
}

// =====================================

// 菜单选项索引
long    menuitem = 0;

/*--------------------------------------------------------------
 * GameLoop - 游戏主循环
 * 功能: 
 *   - 状态机驱动的主循环
 *   - 处理所有游戏状态 (从-12到99)
 *   - 管理游戏逻辑、渲染和输入
 *--------------------------------------------------------------*/
void GameLoop(void)
{
    long    dx, dy;
    long    i, j;

    // ========== 帧率控制 ==========
    // 时间控制：每帧间隔约40ms (25fps)
    if((GameTime1 = timeGetTime())<GameTime2) return;
    GameTime2 = GameTime1 + 40;

    // ========== 状态机 ==========
    switch(GameStatus)
    {
        // ==================== 帮助信息淡入 ====================
        case -12:
            SetClipBox(1, 1, RENDER_WIDTH - 2, RENDER_HEIGHT - 2);
            PutImage(0, 0, PicTitle2);    // 绘制标题背景
            // 绘制菜单高亮框
            SetClipBox(63, 136+28*menuitem, 221, 136+28*menuitem+27);
            PutImage(63, 136, PicTitle);
            // 淡入帮助信息
            SetClipBox(1, 1, RENDER_WIDTH - 2, RENDER_HEIGHT - 2);
            PutImageAB(0, 0, PicHelpInfo, 256 - SceneBright);

            if(SceneBright < 256)
                SceneBright += 32;
            else
            {
                GameStatus = -2;
                PutImage(0, 0, PicTitle2);
                SetClipBox(63, 136+28*menuitem, 221, 136+28*menuitem+27);
                PutImage(63, 136, PicTitle);
                ResetKeyBuffer();
            }

            RefreshScreen();
            break;

        // ==================== 帮助信息显示 ====================
        case -11:
            SetClipBox(1, 1, RENDER_WIDTH - 2, RENDER_HEIGHT - 2);
            PutImage(0,0,PicHelpInfo);    // 绘制帮助信息

            i = GetKey();

            // 按任意键返回
            if(i == KEY_UP || i == KEY_DOWN || i == KEY_ENTER)
            {
                SceneBright = 0;
                GameStatus = -12;
            }
            RefreshScreen();
            break;

        // ==================== 帮助信息淡出 ====================
        case -10:
            if(SceneBright > 0)
                SceneBright -= 32;
            else
                GameStatus = -11;

            SetClipBox(1, 1, RENDER_WIDTH - 2, RENDER_HEIGHT - 2);
            PutImage(0, 0, PicTitle2);
            PutImageAB(0, 0, PicHelpInfo, 256 - SceneBright);

            RefreshScreen();
            break;

        // ==================== 标题画面淡入 ====================
        case -1:
            // 播放背景音乐
            if(SceneBright <= 3)
            if(lpSoundData[90])
            {
                lpSoundData[90]->SetVolume(0);
                lpSoundData[90]->Play(NULL, NULL, 1);
            }

            i = GetKey();

            // 按任意键开始淡入
            if(i == KEY_UP || i == KEY_DOWN || i == KEY_ENTER)
            {
                SceneBright = 256;
            }

            if(SceneBright < 256)
                SceneBright += 3;
            
            SetClipBox(1, 1, RENDER_WIDTH - 2, RENDER_HEIGHT - 2);
            PutImageBR(0,0,PicTitle2, SceneBright);    // 淡入标题背景

            // 绘制菜单高亮
            SetClipBox(63, 136+28*menuitem, 221, 136+28*menuitem+27);
            PutImageBR(63, 136, PicTitle, SceneBright);

            RefreshScreen();

            // 淡入完成，进入菜单
            if(SceneBright >= 256)
            {
                BG_CNT = 0;
                LoadBG();
                ResetKeyBuffer();
                SceneBright = 256;
                GameStatus = -2;
            }
            break;

        // ==================== 主菜单 ====================
        case -2:
            // 绘制菜单
            SetClipBox(63, 136, 221, 222);
            PutImage(0,0,PicTitle2);

            SetClipBox(63, 136+28*menuitem, 221, 136+28*menuitem+27);
            PutImage(63, 136, PicTitle);

            SetClipBox(1, 1, RENDER_WIDTH - 2, RENDER_HEIGHT - 2);
            RefreshScreen();

            i = GetKey();

            // 上
            if(i == KEY_UP)
            {
                if(lpSoundData[92])
                {
                    lpSoundData[92]->SetCurrentPosition(0);
                    lpSoundData[92]->Play(NULL, NULL, 0);
                }
                menuitem --;
                if(menuitem < 0) menuitem += 3;
            }
            else
            // 下
            if(i == KEY_DOWN)
            {
                if(lpSoundData[92])
                {
                    lpSoundData[92]->SetCurrentPosition(0);
                    lpSoundData[92]->Play(NULL, NULL, 0);
                }
                menuitem ++;
                if(menuitem > 2) menuitem -= 3;
            }
            else
            // 回车：选择菜单项
            if(i == KEY_ENTER)
            {
                SetClipBox(63, 136, 221, 222);
                PutImage(0,0,PicTitle2);
                SetClipBox(1, 1, RENDER_WIDTH - 2, RENDER_HEIGHT - 2);

                // 播放选择音效
                if(lpSoundData[91])
                {
                    lpSoundData[91]->SetCurrentPosition(0);
                    lpSoundData[91]->Play(NULL, NULL, 0);
                }

                GameTime2 = timeGetTime() + 100;

                // 根据选择进入不同状态
                switch(menuitem)
                {
                    case 0:    // 开始游戏
                        SceneBright = 256;
                        GameStatus = -3;
                        break;

                    case 1:    // 帮助
                        SceneBright = 256;
                        GameStatus = -10;
                        break;

                    case 2:    // 退出
                        FramePassTime = 256;
                        GameStatus = 99;
                        break;
                }
            }
            break;

        // ==================== 游戏开始淡出 ====================
        case -3:
            if(SceneBright > 0)
            {
                // 音乐渐弱
                if(lpSoundData[90])
                {
                    lpSoundData[90]->SetVolume(-((256-SceneBright)*18));
                }
                SceneBright -= 4;
                PutImage(0, 0, PicTitle2);
                PutImageAB(0, 0, PicBack, 256 - SceneBright);
            }
            else
            {
                // 开始游戏
                GameStatus = 0;
                if(lpSoundData[90])
                {
                    lpSoundData[90]->Stop();
                    lpSoundData[90]->SetVolume(0);
                    lpSoundData[90]->SetCurrentPosition(0);
                }
            }

            RefreshScreen();
            break;

        // ==================== 游戏初始化 ====================
        case 0:
            // 初始化随机数
            j = timeGetTime() & 0x0FFF;
            for(i=0;i<j;i++) dx = rand();

            // 设置时间
            GameTime1 = timeGetTime();
            GameTime2 = GameTime1;

            // 清空食物缓冲区
            for(dy=0; dy<RENDER_HEIGHT/WORMSIZE; dy++)
            for(dx=0; dx<RENDER_WIDTH/WORMSIZE; dx++)
            {
                FoodXY[dx][dy] = 0xFF;
                FoodZ[dx][dy] = 0;

                EfctXY[dx][dy] = 0xFF;
                EfctZ[dx][dy] = 0;
            }

            // 重置蠕虫数据
            for(i=0; i<MAXWORMLEN; i++)
            {
                WormD[i] = 0;
                WormLife[i] = 0;
            }

            // 设置初始增长计数器
            WormStartCounter = 4;

            // 设置蠕虫初始位置
            WormLength = 1;
            WormHoldCounter = 0;
            WormEyeStatus = 0;

            for(i=0; i<WormLength; i++)
            {
                WormX[i] = 200;
                WormY[i] = 290 + i * 10;
            }

            WormSpeed = 2;
            WormSpeedTime = 0;

            // 投放初始食物
            FoodCounter = 4;
            PutInFood(FoodCounter --);

            SearchStart = 0;
            SearchEnd = 0;

            // 开始水声
            WaterSoundVolume = -10000;
            if(lpSoundData[0])
            {
                lpSoundData[0]->SetVolume(WaterSoundVolume);
                lpSoundData[0]->Play(NULL, NULL, 1);
            }

            DayTime = 256;
            SceneBright = 256;
            EatTime = 0; 

            Score = 0;

            GameStatus = 1;
            BG_CNT = 0;
            LoadBG();

            ClearSFXBuffer();
            ResetKeyBuffer();
            break;

        // ==================== 游戏进行中 ====================
        case 1:
            // 空格键暂停
            if(KEYSTATUSFLAG[KEY_SPACE])
            {
                if(GetKey() == KEY_SPACE)
                {
                    GameStatus = 3;

                    if(lpSoundData[91])
                    {
                        lpSoundData[91]->SetCurrentPosition(0);
                        lpSoundData[91]->Play(NULL, NULL, 0);
                    }
                }
            }

            // 投放食物
            if(WormHoldCounter == 1 && FoodCounter >= 0)
            {
                PutInFood(FoodCounter --);
            }

            // 水声跟随蠕虫位置
            if(lpSoundData[0])
            {
                lpSoundData[0]->SetPan((WormX[1]-200)*5);
            }

            // 水声渐强
            if(WaterSoundVolume < 0)
            {
                WaterSoundVolume += 400;
                if(lpSoundData[0])
                {
                    lpSoundData[0]->SetVolume(WaterSoundVolume);
                }
            }

            // 时间流逝
            TimePass();

            // 蠕虫移动
            WormMoveStep();

            // 蠕虫初始增长
            if(WormStartCounter>0 && WormHoldCounter<=0)
            {    
                WormStartCounter --;
                WormGrowth();
            }

            // 方向控制（涟漪反馈）
            if((KEYSTATUSFLAG[KEY_UP] && WormD[0] != 2) ||
               (KEYSTATUSFLAG[KEY_DOWN] && WormD[0] != 0) ||
               (KEYSTATUSFLAG[KEY_LEFT] && WormD[0] != 1) ||
               (KEYSTATUSFLAG[KEY_RIGHT] && WormD[0] != 3))
            {
                AddRipple(WormX[1]+4, WormY[1]+5, 2, rand()&0x7F);
            }

            // 下雨效果
            Rain();

            // 环境音效（鸟叫等）
            if(SceneBright > 200 && RainTime <= 0 && rand() < (WormLength << 4))
            {
                i = rand() % 14;
                if(lpSoundData[101 + i])
                {
                    lpSoundData[101 + i]->SetPan((rand()%4000)-2000);
                    lpSoundData[101 + i]->SetVolume(-(rand()%2000));
                    lpSoundData[101 + i]->Play(NULL, NULL, 0);
                }
            }

            DrawScene();    // 绘制场景

            RefreshScreen();    // 刷新屏幕
            break;

        // ==================== 死亡动画 ====================
        case 2:
            // 等待按回车或动画结束
            if(FramePassTime > 0  && GetKey() != KEY_ENTER)
            {
                if(FramePassTime < SceneBright) SceneBright = FramePassTime;
                FramePassTime --;
            }
            else
            {
                // 停止所有声音
                for(i=0;i<256;i++)
                {
                    if(lpSoundData[i])
                        lpSoundData[i]->Stop();
                }
                SceneBright = 0;
                RainTime = 0;
                GameStatus = -1;    // 返回主菜单
            }

            // 时间流逝
            TimePass();

            // 下雨效果
            Rain();

            DrawScene2();    // 绘制死亡场景

            RefreshScreen();
            break;

        // ==================== 暂停 ====================
        case 3:
            if(KEYSTATUSFLAG[KEY_SPACE])
            {
                if(GetKey() == KEY_SPACE)
                {
                    GameStatus = 1;    // 继续游戏

                    if(lpSoundData[91])
                    {
                        lpSoundData[91]->SetCurrentPosition(0);
                        lpSoundData[91]->Play(NULL, NULL, 0);
                    }
                }
            }
            RefreshScreen();
            break;

        // ==================== 退出动画 ====================
        case 99:
            if(FramePassTime > 0)
            {
                FramePassTime -= 6;
                if(lpSoundData[90])
                {
                    lpSoundData[90]->SetVolume(-((256-FramePassTime)*18));
                }
            }
            else
            {
                bLive = FALSE;    // 退出游戏
            }

            if(FramePassTime < SceneBright) SceneBright = FramePassTime;

            SetClipBox(1, 1, RENDER_WIDTH - 2, RENDER_HEIGHT - 2);
            PutImageBR(0,0,PicTitle2, SceneBright);

            RefreshScreen();
            break;
    }
}

/* 待完成功能:
 *   3. 天气系统 - 根据状态随机变化
 *   5. 背景 - 更多背景变化
 */
