
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
				  console.c
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
							Forrest Yu, 2005
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/*
	回车键: 把光标移到第一列
	换行键: 把光标前进到下一行
*/

#include "type.h"
#include "const.h"
#include "protect.h"
#include "string.h"
#include "proc.h"
#include "tty.h"
#include "console.h"
#include "global.h"
#include "keyboard.h"
#include "proto.h"

PRIVATE void set_cursor(unsigned int position);
PRIVATE void set_video_start_addr(u32 addr);
PRIVATE void flush(CONSOLE *p_con);

PRIVATE int ESC_start; // 0为未进入ESC，1为进入ESC还没回车，2为输入并且回车了
// （注意，有可能按了ESC后，不输入不换行，直接再按一次ESC，这时ESC_start==1，仍然需要归零）
PRIVATE u8 *ESC_start_pos;

/*======================================================================*
			   初始化屏幕，需要在这里清空启动时的输出（需求1）
 *======================================================================*/
PUBLIC void init_screen(TTY *p_tty)
{
	ESC_start = 0;

	int nr_tty = p_tty - tty_table;
	p_tty->p_console = console_table + nr_tty;

	// int v_mem_size = V_MEM_SIZE >> 1;
	int v_mem_size = (V_MEM_SIZE << 4); /* 显存总大小 (in WORD) ，这里调整得大一点，以满足需求8*/

	int con_v_mem_size = v_mem_size / NR_CONSOLES;
	p_tty->p_console->original_addr = nr_tty * con_v_mem_size;
	p_tty->p_console->v_mem_limit = con_v_mem_size;
	p_tty->p_console->current_start_addr = p_tty->p_console->original_addr;

	/* 默认光标位置在最开始处 */
	p_tty->p_console->cursor = p_tty->p_console->original_addr;

	if (nr_tty == 0)
	{
		/* 第一个控制台沿用原来的光标位置 */
		p_tty->p_console->cursor = disp_pos / 2;
		disp_pos = 0;
	}
	else
	{
		out_char(p_tty->p_console, nr_tty + '0');
		out_char(p_tty->p_console, '#');
	}

	set_cursor(p_tty->p_console->cursor);
	clean_console(p_tty->p_console);
}

/*======================================================================*
			   is_current_console
*======================================================================*/
PUBLIC int is_current_console(CONSOLE *p_con)
{
	return (p_con == &console_table[nr_current_console]);
}

/*======================================================================*
			   输出单个字符。需要处理空格、换行、tab、ESC等各种特殊情况
 *======================================================================*/
PUBLIC void out_char(CONSOLE *p_con, char ch)
{
	u8 *p_vmem = (u8 *)(V_MEM_BASE + p_con->cursor * 2);

	if (ESC_start == 2 && ch != 27) // 按回车后，屏蔽除 Esc 之外 任何输入。
	{
		return;
	}

	switch (ch)
	{
	case 27:
		if (ESC_start == 0) // 按ESC，进入查找模式，记录当前光标位置，并把ESC_start设为1。
		{
			ESC_start_pos = p_vmem;
			ESC_start++;
		}
		else // 再次按ESC，一定是退出查找状态
		{
			ESC_start = 0;
			backnomal(p_con);
			flush(p_con);
		}
		break;
	case '\t':
		if (p_con->cursor <
			p_con->original_addr + p_con->v_mem_limit - 4)
		{ // 制表符不会自动显示为4个空格的。这里填入4个制表符，便于后面删除之类的时候和空格、换行区分
			for (int i = 0; i < 4; i++)
			{
				*p_vmem++ = '\t';
				*p_vmem++ = DEFAULT_UNSHOW_COLOR;
				p_con->cursor++;
			}
		}
		break;
	case '\n':
		if (p_con->cursor < p_con->original_addr +
								p_con->v_mem_limit - SCREEN_WIDTH)
		{
			if (ESC_start == 1)
			{ // 已经按过ESC，再按回车，则切换到ESC_start == 2的状态，匹配并停止接受输入
				ESC_start++;
				findmodel(p_con); // 进入匹配函数。
			}
			else
			{ // 正常的换行
				u32 newline = p_con->original_addr + SCREEN_WIDTH *
														 ((p_con->cursor - p_con->original_addr) /
															  SCREEN_WIDTH +
														  1);
				while (p_con->cursor < newline)
				{ // 与制表符同理，将当前光标与下一行开头之前全部填充为换行符，便于后面删除等处理
					*p_vmem++ = '\n';
					*p_vmem++ = DEFAULT_UNSHOW_COLOR;
					p_con->cursor++;
				}
			}
		}
		break;
	case '\b': // 退格backspace
		if (ESC_start == 1 && ESC_start_pos >= p_vmem)
		{ // 进入ESC后，不应该向前删除之前输入的白字内容。
			break;
		}
		if (p_con->cursor > p_con->original_addr)
		{ // tty中有内容，根据当前字符决定怎么删除。
			switch (*(p_vmem - 2))
			{
			case '\t':
				for (int i = 0; i < 4; i++, p_vmem -= 2)
				{
					delete_char(p_con, p_vmem);
				}
				break;
			case '\n':
				for (int i = 0; *(p_vmem - 2) == '\n' && i < SCREEN_WIDTH; p_vmem -= 2, i++)
				{//退格至多退80个，也就是一次最多退一行
					delete_char(p_con, p_vmem);
				}
				break;
			default:
				delete_char(p_con, p_vmem);
				break;
			}
		}
		break;
	default:
		if (p_con->cursor <
			p_con->original_addr + p_con->v_mem_limit - 1)
		{
			*p_vmem++ = ch;
			*p_vmem++ = (ESC_start==0 ? DEFAULT_CHAR_COLOR : DEFAULT_REDCHAR_COLOR); //需要讨论颜色。
			p_con->cursor++;
		}
		break;
	}

	while (p_con->cursor >= p_con->current_start_addr + SCREEN_SIZE)
	{ // 如果光标超出当前界面，自动滚动屏幕
		scroll_screen(p_con, SCR_DN);
	}

	flush(p_con);
}

/*======================================================================*
						   flush
*======================================================================*/
PRIVATE void flush(CONSOLE *p_con)
{
	set_cursor(p_con->cursor);
	set_video_start_addr(p_con->current_start_addr);
}

/*======================================================================*
				设置光标位置
 *======================================================================*/
PRIVATE void set_cursor(unsigned int position)
{
	disable_int();
	out_byte(CRTC_ADDR_REG, CURSOR_H);
	out_byte(CRTC_DATA_REG, (position >> 8) & 0xFF);
	out_byte(CRTC_ADDR_REG, CURSOR_L);
	out_byte(CRTC_DATA_REG, position & 0xFF);
	enable_int();
}

/*======================================================================*
			  set_video_start_addr
 *======================================================================*/
PRIVATE void set_video_start_addr(u32 addr)
{
	disable_int();
	out_byte(CRTC_ADDR_REG, START_ADDR_H);
	out_byte(CRTC_DATA_REG, (addr >> 8) & 0xFF);
	out_byte(CRTC_ADDR_REG, START_ADDR_L);
	out_byte(CRTC_DATA_REG, addr & 0xFF);
	enable_int();
}

/*======================================================================*
			   select_console
 *======================================================================*/
PUBLIC void select_console(int nr_console) /* 0 ~ (NR_CONSOLES - 1) */
{
	if ((nr_console < 0) || (nr_console >= NR_CONSOLES))
	{
		return;
	}

	nr_current_console = nr_console;

	set_cursor(console_table[nr_console].cursor);
	set_video_start_addr(console_table[nr_console].current_start_addr);
}

/*======================================================================*
			   scroll_screen
 *----------------------------------------------------------------------*
 滚屏.
 *----------------------------------------------------------------------*
 direction:
	SCR_UP	: 向上滚屏
	SCR_DN	: 向下滚屏
	其它	: 不做处理
 *======================================================================*/
PUBLIC void scroll_screen(CONSOLE *p_con, int direction)
{
	if (direction == SCR_UP)
	{
		if (p_con->current_start_addr > p_con->original_addr)
		{
			p_con->current_start_addr -= SCREEN_WIDTH;
		}
	}
	else if (direction == SCR_DN)
	{
		if (p_con->current_start_addr + SCREEN_SIZE <
			p_con->original_addr + p_con->v_mem_limit)
		{
			p_con->current_start_addr += SCREEN_WIDTH;
		}
	}
	else
	{
	}

	set_video_start_addr(p_con->current_start_addr);
	set_cursor(p_con->cursor);
}

/*======================================================================*
				清屏
 *======================================================================*/
PUBLIC void clean_console(CONSOLE *p_con)
{
	p_con->cursor = p_con->original_addr; // 光标设置为起始位置
	u8 *p_vmem = (u8 *)(V_MEM_BASE + p_con->cursor * 2);
	while (p_vmem < V_MEM_BASE + (p_con->original_addr + p_con->v_mem_limit) * 2 + 2)
	{
		*p_vmem++ = ' '; // 从起始位置开始，将显示的内容替换为空格
		*p_vmem++ = DEFAULT_CHAR_COLOR;
	}
	set_cursor(p_con->cursor);
	while(p_con->current_start_addr > p_con->original_addr){//清屏后，要跟着向上滚动屏幕到开始位置。
		scroll_screen(p_con, SCR_UP);
	}
}

/*======================================================================*
			   再次按下ESC时触发，搜索模式结束，恢复颜色，删除搜索字段
 *======================================================================*/
PUBLIC void backnomal(CONSOLE *p_con)
{
	u8 *p = (u8 *)(V_MEM_BASE + p_con->original_addr * 2); // 起始位置
	u8 *p_vmem = (u8 *)(V_MEM_BASE + p_con->cursor * 2);   // 当前显示地址的pos
	while (p < ESC_start_pos)	//从开头、到ESC开始的位置，只清空颜色（匹配上的部分），但不删除
	{
		if (*p != '\t' && *p != '\n')
		{
			*(p + 1) = DEFAULT_CHAR_COLOR;
		}
		p = p + 2;
	}
	while (p < p_vmem)
	{
		p += 2;
		delete_char(p_con, p);
	}
}

/*======================================================================*
			   搜索与匹配
 *======================================================================*/
PUBLIC void findmodel(CONSOLE *p_con)
{														   // 搜索并标红
	u8 *p = (u8 *)(V_MEM_BASE + p_con->original_addr * 2); // 起始位置
	u8 *p_vmem = (u8 *)(V_MEM_BASE + p_con->cursor * 2);   // 当前显示地址的pos
	while (p < ESC_start_pos)
	{//搜索范围：从开头到开始ESC的位置。
		int same = 1;
		int len = p_vmem - ESC_start_pos;
		for(int i = 0; i < len; i += 2)
		{
			if (*(p + i) != *(ESC_start_pos + i))
			{
				same = 0;
				break;
			}
		}
		if (same)
		{//能匹配上，则从当前位置开始，把能匹配上的位置标红，同时向前推进指针
			for (int i = 0; i < len; i += 2, p = p + 2)
			{
				if (*p != '\t')
				{
					*(p + 1) = DEFAULT_REDCHAR_COLOR;
				}
			}
		}
		else
		{//匹配不上，则指针向前推进，继续检查匹配情况。
			p = p + 2;
		}
	}
}

/*======================================================================*
			   退格，删除字符
 *======================================================================*/
PUBLIC void delete_char(CONSOLE *p_con, u8 *p_vmem)
{
	p_con->cursor--;
	*(p_vmem - 2) = ' ';
	*(p_vmem - 1) = DEFAULT_CHAR_COLOR;
}