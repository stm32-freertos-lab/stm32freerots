/*
 * Project: N|Watch
 * Author: Zak Kemble, contact@zakkemble.co.uk
 * Copyright: (C) 2013 by Zak Kemble
 * License: GNU GPL v3 (see License.txt)
 * Web: http://blog.zakkemble.co.uk/diy-digital-wristwatch/
 */

#include "common.h"

static const int faces[6][4] = {{0,1,2,3},{1,5,6,2},{5,4,7,6},{4,0,3,7},{0,4,5,1},{3,2,6,7}};
static const int nvert = 8;
static const int nfaces = 6;

/* 复用 watchface.c 的 Flash 顶点数据，保留原旋转动画。 */



  uint8_t  iAng = 0;
  int Cnt, Cntv;
	uint8_t vert[8][3];
	uint8_t x1=0, x2=0, y1=0, y2=0; //,statu=0;

//OLED的显存
//存放格式如下.
//[0]0 1 2 3 ... 127	
//[1]0 1 2 3 ... 127	
//[2]0 1 2 3 ... 127	
//[3]0 1 2 3 ... 127	
//[4]0 1 2 3 ... 127	
//[5]0 1 2 3 ... 127	
//[6]0 1 2 3 ... 127	
//[7]0 1 2 3 ... 127 		   
byte OLED_GRAM[512];	 


static bool down(void);
static bool up(void);
static bool select(void);
static display_t draw(void);


void __SysTick(void);
void OLED_DrawPoint(uint8_t x,uint8_t y,uint8_t t);

void OLED_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);

#if !NWATCH_UI_ONLY
extern const tune_t TUNE[];
extern const tune_t STAY[];

#define IDX_MAX    2  //歌曲数量

static byte idx=0;

static void chose_tone(byte en)
{
	static byte _idx=1;
	if(idx!=_idx||en)
	{
		_idx=idx;
		
		switch(idx)
		{
			case 0:tune_play(TUNE, VOL_ALARM, PRIO_ALARM);break;
			case 1:tune_play(STAY, VOL_ALARM, PRIO_ALARM);break;
		}
	}
	
	switch(idx)
	{
		case 0:draw_string(PSTR(">SUMMER"), false, 64, 14);break;
		case 1:draw_string(PSTR(">STAY  "), false, 64, 14);break;
	}
	
}

#endif

void tunemakerOpen()
{
	//menu_close();

	display_setDrawFunc(draw);
	buttons_setFunc(BTN_1,	up);
	buttons_setFunc(BTN_2,	select);
	buttons_setFunc(BTN_3,	down);
	
#if !NWATCH_UI_ONLY
	chose_tone(1);
#endif
	beginAnimation2(NULL);
}

static bool down()
{
#if !NWATCH_UI_ONLY
	idx--;
	if(idx == 255) idx= 1;
#endif
	
  iAng--;
	if(iAng >= 180) iAng= 179;
	return true;
}

static bool up()
{
#if !NWATCH_UI_ONLY
	idx++;
	if(idx == IDX_MAX) idx= 0;
#endif
	
	 iAng++;
		if(iAng >= 180) iAng = 0;
	return true;
}

static bool select()
{
#if !NWATCH_UI_ONLY
	tune_stop(PRIO_HIGH);
#endif
	animation_start(back, ANIM_MOVE_OFF);  //带关闭动画的退出
	 
	return true;
}

static display_t draw()
{
	
	// Infinite loop
	     
				__SysTick();
					
	  		// Get vertices for the current angle
				//获取当前角度的顶点
	  		for(Cntv = 0; Cntv < nvert; ++Cntv){
	  				vert[Cntv][0] = cubeVertices[iAng*nvert + Cntv][0];
	  			  	vert[Cntv][1] = cubeVertices[iAng*nvert + Cntv][1];
	  			  	vert[Cntv][2] = cubeVertices[iAng*nvert + Cntv][2];
	  			  	}
	  		// Draw wireframe for each face of the cube
				//为立方体的每个面绘制线框
							//一个面有四条边，画六个面就是 4x6 =24次就可以完整显示
	  		for(Cnt = 0;Cnt < nfaces; ++Cnt){

	  			x1 = vert[faces[Cnt][0]][0]-34;
	  			y1 = vert[faces[Cnt][0]][1]-1;
	  			x2 = vert[faces[Cnt][1]][0]-34;
	  			y2 = vert[faces[Cnt][1]][1]-1;
         OLED_DrawLine(x1, y1, x2, y2);
					
	  			x1 = vert[faces[Cnt][1]][0]-34;
	  			y1 = vert[faces[Cnt][1]][1]-1;
	  			x2 = vert[faces[Cnt][2]][0]-34;
	  			y2 = vert[faces[Cnt][2]][1]-1;
					OLED_DrawLine(x1, y1, x2, y2);

	  			x1 = vert[faces[Cnt][2]][0]-34;
	  			y1 = vert[faces[Cnt][2]][1]-1;
	  			x2 = vert[faces[Cnt][3]][0]-34;
	  			y2 = vert[faces[Cnt][3]][1]-1;
					OLED_DrawLine(x1, y1, x2, y2);


	  			x1 = vert[faces[Cnt][3]][0]-34;
	  			y1 = vert[faces[Cnt][3]][1]-1;
	  			x2 = vert[faces[Cnt][0]][0]-34;
	  			y2 = vert[faces[Cnt][0]][1]-1;
					OLED_DrawLine(x1, y1, x2, y2);

	  		}

			// Draw platform

	    draw_bitmap(0, 0, (const byte *)OLED_GRAM, 64, 64, NOINVERT, 0);
			memset(&OLED_GRAM, 0x00, FRAME_BUFFER_SIZE/2);
      draw_string_P(PSTR("Cube "), false, 64, 28);
			draw_string_P(PSTR("Rotation"), false, 64, 40);

#if !NWATCH_UI_ONLY
			chose_tone(0);  //选择歌曲  
#endif
	    ///LED0=!LED0;	 
	

	return DISPLAY_DONE;
}





void __SysTick(void)
{
	
		iAng++;
		if(iAng == 180) iAng = 0;
	
}


//画点 
//x:0~63
//y:0~63
//t:1 填充 0,清空				   
void OLED_DrawPoint(uint8_t x,uint8_t y,uint8_t t)
{	
	//对应 byte OLED_GRAM[512];	  // x= 64 ,y=8
	//变换一哈形式
	uint8_t pos,bx,temp=0;
	if(x>63||y>63)return;//超出范围了.
	pos=y/8;
	bx=y%8;
	temp=1<<(bx);
	 if(t) OLED_GRAM[64*pos+x] |= temp;  // 做到的效果是竖着存放 竖着为y ，横着为x
	  else OLED_GRAM[64*pos+x]&=~temp;	

	
	
	
	
	//对应 byte OLED_GRAM[8][64];	 
	//这一段可以使用了，但是有点问题  方向反了。
	//需要了解如何把图像x轴镜像   
//		uint8_t pos,bx,temp=0;
//	if(x>63||y>63)return;//超出范围了.
//	pos=7-y/8;
//	bx=y%8;
//	temp=1<<(7-bx);
//	if(t)OLED_GRAM[pos][x]|=temp;
//	else OLED_GRAM[pos][x]&=~temp;	
	
	
}


//画线
//x1,y1:起点坐标
//x2,y2:终点坐标  
void OLED_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
	uint16_t t; 
	int xerr=0,yerr=0,delta_x,delta_y,distance; 
	int incx,incy,uRow,uCol; 
	delta_x=x2-x1; //计算坐标增量 
	delta_y=y2-y1; 
	uRow=x1; 
	uCol=y1; 
	if(delta_x>0)incx=1; //设置单步方向 
	else if(delta_x==0)incx=0;//垂直线 
	else {incx=-1;delta_x=-delta_x;} 
	if(delta_y>0)incy=1; 
	else if(delta_y==0)incy=0;//水平线 
	else{incy=-1;delta_y=-delta_y;} 
	if( delta_x>delta_y)distance=delta_x; //选取基本增量坐标轴 
	else distance=delta_y; 
	for(t=0;t<=distance+1;t++ )//画线输出 
	{  
		OLED_DrawPoint(uRow,uCol,1);//画点 
		xerr+=delta_x ; 
		yerr+=delta_y ; 
		if(xerr>distance) 
		{ 
			xerr-=distance; 
			uRow+=incx; 
		} 
		if(yerr>distance) 
		{ 
			yerr-=distance; 
			uCol+=incy; 
		} 
	}  
} 


