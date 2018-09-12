
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                            main.c
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

#include "type.h"
#include "stdio.h"
#include "const.h"
#include "keyboard.h"
#include "protect.h"
#include "string.h"
#include "fs.h"
#include "proc.h"
#include "tty.h"
#include "console.h"
#include "global.h"
#include "proto.h"
/*======================================================================*
                            kernel_main
 *======================================================================*/
PUBLIC int kernel_main() 
{
    disp_str("-----\"kernel_main\" begins-----\n");
    struct task* p_task;
    struct proc* p_proc= proc_table;
    char* p_task_stack = task_stack + STACK_SIZE_TOTAL;
    u16   selector_ldt = SELECTOR_LDT_FIRST;
    u8    privilege;
    u8    rpl;
    int   eflags;
    int   i, j;
    int   prio;

    // start the process
    for (i = 0; i < NR_TASKS+NR_PROCS; i++) 
    {
        if (i < NR_TASKS) 
        {
            /* task */
            p_task    = task_table + i;
            privilege = PRIVILEGE_TASK;
            rpl       = RPL_TASK;
            eflags    = 0x1202; /* IF=1, IOPL=1, bit 2 is always 1   1 0010 0000 0010(2)*/
            prio      = 15;     //set the priority to 15
        }
        else 
        {
            /* user process */
            p_task    = user_proc_table + (i - NR_TASKS);
            privilege = PRIVILEGE_USER;
            rpl       = RPL_USER;
            eflags    = 0x202; /* IF=1, bit 2 is always 1              0010 0000 0010(2)*/
            prio      = 5;     //set the priority to 5
        }

        strcpy(p_proc->name, p_task->name); /* set prio name */
        p_proc->pid = i;            /* set pid */

        p_proc->run_count = 0;

        p_proc->ldt_sel = selector_ldt;

        memcpy(&p_proc->ldts[0], &gdt[SELECTOR_KERNEL_CS >> 3], sizeof(struct descriptor));
        p_proc->ldts[0].attr1 = DA_C | privilege << 5;
        memcpy(&p_proc->ldts[1], &gdt[SELECTOR_KERNEL_DS >> 3], sizeof(struct descriptor));
        p_proc->ldts[1].attr1 = DA_DRW | privilege << 5;
        p_proc->regs.cs = (0 & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | rpl;
        p_proc->regs.ds = (8 & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | rpl;
        p_proc->regs.es = (8 & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | rpl;
        p_proc->regs.fs = (8 & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | rpl;
        p_proc->regs.ss = (8 & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | rpl;
        p_proc->regs.gs = (SELECTOR_KERNEL_GS & SA_RPL_MASK) | rpl;

        p_proc->regs.eip = (u32)p_task->initial_eip;
        p_proc->regs.esp = (u32)p_task_stack;
        p_proc->regs.eflags = eflags;

        p_proc->p_flags = 0;
        p_proc->p_msg = 0;
        p_proc->p_recvfrom = NO_TASK;
        p_proc->p_sendto = NO_TASK;
        p_proc->has_int_msg = 0;
        p_proc->q_sending = 0;
        p_proc->next_sending = 0;

        for (j = 0; j < NR_FILES; j++) p_proc->filp[j] = 0;

        p_proc->ticks = p_proc->priority = prio;
        p_proc->run_state = 1;
        p_task_stack -= p_task->stacksize;
        p_proc++;
        p_task++;
        selector_ldt += 1 << 3;
    }

    proc_table[4].run_state = 0;
    proc_table[5].run_state = 0;

    // init process
    k_reenter = 0;
    ticks = 0;

    p_proc_ready = proc_table;

    init_clock();
    init_keyboard();

    restart();

    while(1){}
}


/*****************************************************************************
 *                                get_ticks
 *****************************************************************************/
PUBLIC int get_ticks()
{
    MESSAGE msg;
    reset_msg(&msg);
    msg.type = GET_TICKS;
    send_recv(BOTH, TASK_SYS, &msg);
    return msg.RETVAL;
}

PUBLIC void addTwoString(char *to_str,char *from_str1,char *from_str2)
{
    int i=0,j=0;
    while(from_str1[i]!=0) to_str[j++]=from_str1[i++];
    i=0;
    while(from_str2[i]!=0) to_str[j++]=from_str2[i++];
    to_str[j]=0;
}

void shell(char *tty_name)
{
    int fd;
    //int isLogin = 0;
    char rdbuf[512];
    char cmd[512];
    char arg1[512];
    char arg2[512];
    char buf[1024];
    char temp[512];

    int fd_stdin  = open(tty_name, O_RDWR);
    assert(fd_stdin  == 0);
    int fd_stdout = open(tty_name, O_RDWR);
    assert(fd_stdout == 1);
    animation();  // the start animation
    char current_dirr[512] = "/";
    while (1) 
    {
        // clear the array ！
        clearArr(rdbuf, 512);
        clearArr(cmd, 512);
        clearArr(arg1, 512);
        clearArr(arg2, 512);
        clearArr(buf, 1024);
        clearArr(temp, 512);

        printf("root@localhost%s:~$ ", current_dirr);
        int r = read(fd_stdin, rdbuf, 512);
        if (strcmp(rdbuf, "") == 0) continue;
        int i = 0;
        int j = 0;
        while(rdbuf[i] != ' ' && rdbuf[i] != 0)
        {
            cmd[i] = rdbuf[i];
            i++;
        }
        i++;
        while(rdbuf[i] != ' ' && rdbuf[i] != 0)
        {
            arg1[j] = rdbuf[i];
            i++;
            j++;
        }
        i++;
        j = 0;
        while(rdbuf[i] != ' ' && rdbuf[i] != 0)
        {
            arg2[j] = rdbuf[i];
            i++;
            j++;
        }
        // clear
        rdbuf[r] = 0;

        if (strcmp(cmd, "process") == 0)
        {
            ProcessManage();
        }
        else if(strcmp(cmd, "welcome") == 0)
        {
            clear();
            animation();
        }
        else if (strcmp(cmd, "help") == 0)
        {
            help();
        }      
        else if (strcmp(cmd, "clear") == 0)
        {
            clear(); 
        }
        else if (strcmp(cmd, "ls") == 0) 
        {
            ls(current_dirr);
        }
        else if (strcmp(cmd, "about") == 0) 
        {
            printAbout();
        }
        else if(strcmp(rdbuf,"pause 4") == 0 )
        {
            proc_table[4].run_state = 0 ;
            ProcessManage();
        }
        else if(strcmp(rdbuf,"pause 5") == 0 )
        {
            proc_table[5].run_state = 0 ;
            ProcessManage();
        }
        else if(strcmp(rdbuf,"pause 6") == 0 )
        {
            proc_table[6].run_state = 0 ;
            ProcessManage();
        }
        else if(strcmp(rdbuf,"kill 4") == 0 )
        {
//            proc_table[4].p_flags = 1;
//            ProcessManage();
            printf("cant kill this process!\n");
        }
        else if(strcmp(rdbuf,"kill 5") == 0 )
        {
            proc_table[5].p_flags = 1;
            ProcessManage();
        }
        else if(strcmp(rdbuf,"kill 6") == 0 )
        {
            proc_table[6].p_flags = 1;
            ProcessManage();
        }
        else if(strcmp(rdbuf,"resume 4") == 0 )
        {
            proc_table[4].run_state = 1 ;
            ProcessManage();
        }
        else if(strcmp(rdbuf,"resume 5") == 0 )
        {
            proc_table[5].run_state = 1 ;
            ProcessManage();
        }
        else if(strcmp(rdbuf,"resume 6") == 0 )
        {
            proc_table[6].run_state = 1 ;
            ProcessManage();
        }
        else if(strcmp(rdbuf,"up 4") == 0 )
        {
            proc_table[4].priority = proc_table[4].priority*2;
            ProcessManage();
        }
        else if(strcmp(rdbuf,"up 5") == 0 )
        {
            proc_table[5].priority = proc_table[5].priority*2;
            ProcessManage();
        }
        else if(strcmp(rdbuf,"up 6") == 0 )
        {
            proc_table[6].priority = proc_table[6].priority*2;
            ProcessManage();
        }
        else if (strcmp(cmd, "touch") == 0)
        {
            if(arg1[0]!='/')
            {
                addTwoString(temp,current_dirr,arg1);
                memcpy(arg1,temp,512);                
            }

            fd = open(arg1, O_CREAT | O_RDWR);
            if (fd == -1)
            {
                printf("Failed to create file! Please check the filename!\n");
                continue ;
            }
            write(fd, buf, 1);
            printf("File created: %s (fd %d)\n", arg1, fd);
            close(fd);
        }
        else if (strcmp(cmd, "cat") == 0)
        {
            if(arg1[0]!='/')
            {
                addTwoString(temp,current_dirr,arg1);
                memcpy(arg1,temp,512);                
            }

            fd = open(arg1, O_RDWR);
            if (fd == -1)
            {
                printf("Failed to open file! Please check the filename!\n");
                continue ;
            }
            if (!verifyFilePass(arg1, fd_stdin))
            {
                printf("Authorization failed\n");
                continue;
            }
            read(fd, buf, 1024);
            close(fd);
            printf("%s\n", buf);
        }
        else if (strcmp(cmd, "vi") == 0)
        {
            if(arg1[0]==0)
            {
                printf("Failed to open file! Please input the filename!\n");
                continue;
            }
            if(arg1[0]!='/')
            {
                addTwoString(temp,current_dirr,arg1);
                memcpy(arg1,temp,512);                
            }

            fd = open(arg1, O_RDWR);
            if (fd == -1)
            {
                printf("Please check the filename!\n");
                continue ;
            }
            if (!verifyFilePass(arg1, fd_stdin))
            {
                printf("Authorization failed\n");
                continue;
            }
            int tail = read(fd_stdin, rdbuf, 512);
            rdbuf[tail] = 0;

            write(fd, rdbuf, tail+1);
            close(fd);
        }
        else if (strcmp(cmd, "rm") == 0) 
        {
            if ((strcmp(arg1, "-r") == 0) || (strcmp(arg1, "-R") == 0)) 
            {
                char files[512] = "";
                files[0] = 0;
                int len = 0;
                if (strlen(arg2) == 0) 
                {
                    printl("input error!");
                    continue;
                }
                if (arg2[0] != '/') 
                {
                    addTwoString(temp, current_dirr, arg2);
                    memcpy(arg2, temp, 512);
                }
                //printl("%s %s\n", arg1, arg2);
                find_all_path(files, arg2, &len);

                //printl("%d\n", len);
                
                // Unlink all the paths in the array files
                int index = 0;
                char current_path[32];
                char current_char = 0;
                char path_index = 0;
                while (current_char = files[index]) 
                {
                    if (current_char != ' ') 
                    {
                        current_path[path_index] = current_char;
                        path_index++;
                    } 
                    else 
                    {
                        current_path[path_index] = 0;
                        path_index = 0;

                        // Unlink the current path or file
                        unlink(current_path);
                    }

                    index++;
                }

                // Unlink the current directory
                unlink(arg2);
                
                continue;
            }
            if(arg1[0]!='/')
            {
                addTwoString(temp,current_dirr,arg1);
                memcpy(arg1,temp,512);                
            }
            
            int result;
            result = unlink(arg1);
            if (result == 0)
            {
                printf("%s File deleted!\n",arg1);
                continue;
            }
            else
            {
                printf("Failed to delete file! Please check the filename!\n");
                continue;
            }
        }
        else if (strcmp(cmd, "mv") == 0)
        {
            if(arg1[0]!='/')
            {
                addTwoString(temp,current_dirr,arg1);
                memcpy(arg1,temp,512);                
            }
            //get the content of file first
            fd = open(arg1, O_RDWR);
            if (fd == -1)
            {
                printf("File not exists! Please check the filename!\n");
                continue ;
            }
           
            int tail = read(fd, buf, 1024);
            close(fd);

            if(arg2[0]!='/')
            {
                addTwoString(temp,current_dirr,arg2);
                memcpy(arg2,temp,512);                
            }
            /*create the file*/
            fd = open(arg2, O_CREAT | O_RDWR);
            if (fd == -1)
            {
                //file exist
            }
            else
            {
                //file not exist or create file
                char temp2[1024];
                temp2[0] = 0;
                write(fd, temp2, 1);
                close(fd);
            }
            // set file to value
            fd = open(arg2, O_RDWR);
            write(fd, buf, tail+1);
            close(fd);
            // delete the file
            unlink(arg1);
        }   
        else if (strcmp(cmd, "encrypt") == 0)
        {
            fd = open(arg1, O_RDWR);
            if (fd == -1)
            {
                printf("File not exists! Please check the filename!\n");
                continue ;
            }
            if (!verifyFilePass(arg1, fd_stdin))
            {
                printf("Authorization failed\n");
                continue;
            }
            doEncrypt(arg1, fd_stdin);
        }
        else if (strcmp(cmd, "test") == 0)
        {
            doTest(arg1);
        }
        else if (strcmp(cmd, "minesweeper") == 0)
        {
        	game(fd_stdin);
        }
        else if (strcmp(cmd, "mkdir") == 0)
        {
            i = j =0;
            while(current_dirr[i]!=0)
            {
                arg2[j++] = current_dirr[i++];
            }
            i = 0;
            
            while(arg1[i]!=0)
            {
                arg2[j++]=arg1[i++];
            }
            arg2[j]=0;
            printf("%s\n", arg2);
            fd = mkdir(arg2);            
        }
        else if (strcmp(cmd, "cd") == 0)
        {
            if(strcmp(arg1, "..")==0)
            {
                memcpy(arg2, current_dirr, 512);
                j =0;
                int k=0;
                int count =0;
                while(arg2[k] !=0)
                {
                    if(arg2[k++]=='/')
                    {
                        count++;
                    }
                }
                int index=0;
                for(i=0;arg2[i]!=0;i++)
                {
                    if(arg2[i] == '/')
                    {
                        index++;
                    }
                    if(index < count-1)
                    {
                        arg1[j++] = arg2[i];
                    }
                }
                arg1[j++] ='/';
                arg1[j]=0;
            }
            else if(arg1[0]==0)
            {
                arg1[0] ='/';
                arg1[1]=0;
            }
            else
            { // not absolute path from root
                i = j =0;
                while(current_dirr[i]!=0)
                {
                    arg2[j++] = current_dirr[i++];
                }
                i = 0;
                while(arg1[i]!=0)
                {
                    arg2[j++]=arg1[i++];
                }
                arg2[j++] = '/';
                arg2[j]=0;
                memcpy(arg1, arg2, 512);
            }
            //printf("%s\n", arg1);
            fd = open(arg1, O_RDWR);

            if(fd == -1)
            {
                printf("The path not exists! Please check the pathname!\n");
            }
            else
            {
                memcpy(current_dirr, arg1, 512);
                //printf("Change dir %s success!\n", current_dirr);
            }
        }
        else if( strcmp(cmd, "snake")  == 0)
        {
            snakeGame();
        }

        else if( strcmp(cmd, "calculator")  == 0)
        {
            calculator();
        }

        else if( strcmp(cmd, "gobang")  == 0)
        {
            Gobang();
        }


        else printf("Command not found, please check!\n");
    }
}

/*======================================================================*
                               TestA
 *======================================================================*/

//A process
void TestA()
{
    shell("/dev_tty0");
    assert(0);
}

/*======================================================================*
                               TestB
 *======================================================================
*/
//B process
void TestB()
{
	shell("/dev_tty1");
	assert(0); /* never arrive here */
}
/*======================================================================*
                               TestC
 *======================================================================*/
//C process
void TestC()
{
	//shell("/dev_tty2");
	assert(0);
}

/*****************************************************************************
 *                                panic
 *****************************************************************************/
PUBLIC void panic(const char *fmt, ...) 
{
    int i;
    char buf[256];
    /* 4 is the size of fmt in the stack */
    va_list arg = (va_list)((char*)&fmt + 4);
    i = vsprintf(buf, fmt, arg);
    printl("%c !!panic!! %s", MAG_CH_PANIC, buf);
    /* should never arrive here */
    __asm__ __volatile__("ud2");
}

/*****************************************************************************
 *                                Custom Command
 *****************************************************************************/
char* findpass(char *src)
{
    char pass[128];
    int flag = 0;
    char *p1, *p2;

    p1 = src;
    p2 = pass;

    while (p1 && *p1 != ' ')
    {
        if (*p1 == ':') flag = 1;

        if (flag && *p1 != ':')
        {
            *p2 = *p1;
            p2++;
        }
        p1++;
    }
    *p2 = '\0';

    return pass;
}

void clearArr(char *arr, int length)
{
    int i;
    for (i = 0; i < length; i++) arr[i] = 0;
}

void printAbout()
{
    clear(); 
    if(current_console==0)
    {
        printf("=============================================================================\n");
        printf("                                     HUAJI OS                                \n");
        printf("                                Zou Xiaohan 1652746                          \n");
        printf("                            Life is short, you need HUAJI!                   \n");
        printf("=============================================================================\n");
    }
    else printf("[TTY #%d]\n", current_console);
}


void clear()
{	
    clear_screen(0,console_table[current_console].cursor);
    console_table[current_console].crtc_start = console_table[current_console].orig;
    console_table[current_console].cursor = console_table[current_console].orig;    
}

void doTest(char *path) 
{
    struct dir_entry *pde = find_entry(path);
    printl(pde->name);
    printl("\n");
    printl(pde->pass);
    printl("\n");
}

int verifyFilePass(char *path, int fd_stdin) 
{
    char pass[128];

    struct dir_entry *pde = find_entry(path);

    /*printl(pde->pass);*/

    if (strcmp(pde->pass, "") == 0) return 1;

    printl("Please input the file password: ");
    read(fd_stdin, pass, 128);

    if (strcmp(pde->pass, pass) == 0) return 1;

    return 0;
}

void doEncrypt(char *path, int fd_stdin) 
{
    //search the file
    /*struct dir_entry *pde = find_entry(path);*/

    char pass[128] = {0};

    printl("Please input the new file password: ");
    read(fd_stdin, pass, 128);

    if (strcmp(pass, "") == 0) 
    {
        /*printl("A blank password!\n");*/
        strcpy(pass, "");
    }
    //encrypt the file
    int i, j;

    char filename[MAX_PATH];
    memset(filename, 0, MAX_FILENAME_LEN);
    struct inode * dir_inode;

    if (strip_path(filename, path, &dir_inode) != 0) return 0;

    if (filename[0] == 0)   /* path: "/" */
        return dir_inode->i_num;
    /**
     * Search the dir for the file.
     */
    int dir_blk0_nr = dir_inode->i_start_sect;
    int nr_dir_blks = (dir_inode->i_size + SECTOR_SIZE - 1) / SECTOR_SIZE;
    int nr_dir_entries =
    dir_inode->i_size / DIR_ENTRY_SIZE;
    int m = 0;
    struct dir_entry * pde;
    for (i = 0; i < nr_dir_blks; i++) 
    {
        RD_SECT(dir_inode->i_dev, dir_blk0_nr + i);
        pde = (struct dir_entry *)fsbuf;
        for (j = 0; j < SECTOR_SIZE / DIR_ENTRY_SIZE; j++,pde++)
        {
            if (memcmp(filename, pde->name, MAX_FILENAME_LEN) == 0)
            {
                // delete the file
                strcpy(pde->pass, pass);
                WR_SECT(dir_inode->i_dev, dir_blk0_nr + i);
                return;
                /*return pde->inode_nr;*/
            }
            if (++m > nr_dir_entries) break;
        }
        if (m > nr_dir_entries) break;/* all entries have been iterated */
    }

}


void help() 
{
    printf("==============================================================================\n");
    printf("                                     HUAJI OS                                 \n");
    printf("                           Life is short, you need HUAJI!                     \n");
    printf("==============================================================================\n");
    printf("Usage: [command]   [flags]             [options]                              \n");
    printf("       welcome                       : show welcome message                   \n");
    printf("       clear                         : clear the screen                       \n");
    printf("       ls                            : list files in current directory        \n");
    printf("       touch       [filename]        : create a new file                      \n");
    printf("       cat         [filename]        : display content of the file            \n");
    printf("       vi          [filename]        : modify the content of the file         \n");
    printf("       rm          [filename]        : delete a file                          \n");
    printf("       cd          [pathname]        : change the directory                   \n");
    printf("       mkdir       [dirname]         : create a new directory                 \n");
    printf("       process                       : display all process-info and manage    \n");
    printf("       about                         : display the about of system            \n");
    printf("       minesweeper                   : start the minesweeper game             \n");
    printf("       snake                         : start the snake game                   \n");
    printf("       gobang                        : start the Gobang game                  \n");
    printf("       calculator                    : start the calculator                   \n");
    printf("==============================================================================\n");
}

void ProcessManage()
{
    int i;
    printf("=============================================================================\n");
    printf("         myID      |    name       |      priority    |     running        \n");
    printf("-----------------------------------------------------------------------------\n");
    for ( i = 0 ; i < NR_TASKS + NR_PROCS ; ++i ) 
    {
        if(proc_table[i].p_flags != 1)
            printf("          %d             %s               %d                 %d\n",
               proc_table[i].pid, proc_table[i].name, proc_table[i].priority, proc_table[i].run_state);
    }
    printf("=============================================================================\n");
    printf("=          Usage: pause  [pid]  you can pause one process                   =\n");
    printf("=          	      resume [pid]  you can resume one process                  =\n");
    printf("=                 kill   [pid]  kill the process                            =\n");
    printf("=                 up     [pid]  improve the process priority                =\n");
    printf("=============================================================================\n");
}


/*======================================================================*
                            welcome animation
 *======================================================================*/
void animation() 
{
    printf("===============================================================================\n");
    printf("   ooo      ooo  ooo    ooo      ooo       oooooooooo  oooooooooo              \n");
    printf("   ooo      ooo  ooo    ooo     ooooo      oooooooooo  oo  oo  oo              \n");
    printf("   ooo      ooo  ooo    ooo    ooo ooo         oo          oo      ooo   oooo  \n");
    printf("   oooooooooooo  ooo    ooo    oo   oo         oo          oo     o   o o      \n");
    printf("   ooo      ooo  ooo    ooo   ooooooooo     o  oo          oo     o   o  oooo  \n");
    printf("   ooo      ooo  ooo    ooo   oo     oo     oo oo      oo  oo  oo o   o      o \n");
    printf("   ooo      ooo   oooooooo   ooo     ooo     ooo       oooooooooo  ooo   oooo  \n");
    printf("===============================================================================\n");
    printf("\n\n\n\n");
    printf("                           Welcome to use HUAJI OS!                            \n");
    printf("                      Enter 'help' to show help message.                       \n");
    printf("\n\n\n\n");

}


/*======================================================================*
                            Minesweeper Game
 *======================================================================*/
unsigned int _seed2 = 0xDEADBEEF;

void srand(unsigned int seed)
{
	_seed2 = seed;
}

int rand() 
{
	int next = _seed2;
	int result;

	next *= 1103515245;
	next += 12345;
	result = (next / 65536) ;

	next *= 1103515245;
	next += 12345;
	result <<= 10;
	result ^= (next / 65536) ;

	next *= 1103515245;
	next += 12345;
	result <<= 10;
	result ^= (next / 65536) ;

	_seed2 = next;

	return result>0 ? result : -result;
}

void show_mat(int *mat,int *mat_state, int touch_x,int touch_y,int display)
{
	int x, y;
	for (x = 0; x < 10; x++)
    {
		printf("  %d", x);
	}
	printf("\n");
	for (x = 0; x < 10; x++)
    {
		printf("---");
	}
	for (x = 0; x < 10; x++)
    {
		printf("\n%d|", x);
		for (y = 0; y < 10; y++)
        {
			if (mat_state[x * 10 + y] == 0)
            {				
				if (x == touch_x && y == touch_y) printf("*  ");
				else if (display!=0 && mat[x * 10 + y] == 1) printf("#  ");
				else printf("-  ");	
			}
			else if (mat_state[x * 10 + y] == -1)
            {
				printf("O  ");
			}
			else
            {
				printf("%d  ", mat_state[x * 10 + y]);
			}
			
		}
	}
	printf("\n");
}

void init_game(int *mat, int mat_state[100])
{
    printf("===============================================================================\n");
    printf("                                    minesweeper                                \n");
    printf("                                  Enter e to quit.                             \n");
    printf("===============================================================================\n");
    printf("\n\n");

	int sum = 0;
	int x, y;
	for (x = 0; x < 100; x++) mat[x] = mat_state[x] = 0;
	while (sum < 15)
    {
		x = rand() % 10;
		y = rand() % 10;
		if (mat[x * 10 + y] == 0)
        {
			sum++;
			mat[x * 10 + y] = 1;
		}
	}
	show_mat(mat,mat_state,-1,-1,0);
}

int check(int x, int y, int *mat)
{
	int i, j,now_x,now_y,result = 0;
	for (i = -1; i <= 1; i++)
    {
		for (j = -1; j <= 1; j++)
        {
			if (i == 0 && j == 0) continue;
			now_x = x + i;
			now_y = y + j;
			if (now_x >= 0 && now_x < 10 && now_y >= 0 && now_y <= 9)
            {
				if (mat[now_x * 10 + now_y] == 1) result++;
			}
		}
	}
	return result;
}

void dfs(int x, int y, int *mat, int *mat_state,int *left_coin)
{
	int i, j, now_x, now_y,temp;
	if (mat_state[x*10+y] != 0) return;
	(*left_coin)--;
	temp = check(x, y,mat);
	if (temp != 0)
    {
		mat_state[x * 10 + y] = temp;
	}
	else
    {
		mat_state[x * 10 + y] = -1;
		for (i = -1; i <= 1; i++)
        {
			for (j = -1; j <= 1; j++)
            {
				if (i == 0 && j == 0) continue;
				now_x = x + i;
				now_y = y + j;
				if (now_x >= 0 && now_x < 10 && now_y >= 0 && now_y <= 9)
                {				
					dfs(now_x, now_y,mat,mat_state,left_coin);
				}
			}
		}
	}
}

void game(int fd_stdin)
{
	int mat[100] = { 0 };
	int mat_state[100] = { 0 };
	char keys[128];
	int x, y, left_coin = 100,temp;
	int flag = 1;

	while (flag == 1)
    {
		init_game(mat, mat_state);
		left_coin = 100;
		printf("-------------------------------------------\n\n");
		printf("Input next x and y: ");

		while (left_coin != 15)
        {

		    clearArr(keys, 128);
            int r = read(fd_stdin, keys, 128);
            if(keys[0] == 'e') break;
            if(keys[0]>'9'||keys[0]<'0'||keys[1]!=' '||keys[2]>'9'||keys[2]<'0'||keys[3]!=0)
            {
                printf("Please input again!\n");
    			continue;
            } 
            x = keys[0]-'0';
            y = keys[2]-'0';
			if (x < 0 || x>9 || y < 0 || y>9)
            {
				printf("Please input again!\n");
				continue;
			}

			if (mat[x * 10 + y] == 1)
            {
				break;
			}
			else
            {
				dfs(x, y, mat, mat_state, &left_coin);
				if (left_coin <= 15) break;
				show_mat(mat, mat_state, -1, -1, 0);
				printf("-------------------------------------------\n\n");
				printf("Input next x and y: ");
			}
		}
		if (mat[x * 10 + y] == 1)
        {
			printf("\n\nFAIL!\n");
			show_mat(mat, mat_state, x, y, 1);
		}
		else
        {
			printf("\n\nSUCCESS!\n");
			show_mat(mat, mat_state, -1, -1, 1);
		}

		printf("Do you want to continue?(yes ot no)\n");
		clearArr(keys, 128);
        int r = read(fd_stdin, keys, 128);
        //  printf("%s\n",keys);
        if (keys[0]=='n' && keys[1]=='o' && keys[2]==0)
        {
        	//flag = 0;
        //	printf("%s\n",keys);
            break;
        }
	}	
}


/*======================================================================*
                            snake game
 *======================================================================*/

/**
 *  listen the keybord
 * @param key
 * @return
 */
int quitSnake = 0;  

PUBLIC void judgeInpt(u32 key) 
{
    char output[2] = {'\0', '\0'};
    if (!(key & FLAG_EXT)) 
    {
        output[0] = key & 0xFF;
        if(output[0] == 'a') changeToLeft();
        if(output[0] == 's') changeToDown();
        if(output[0] == 'd') changeToRight();
        if(output[0] == 'w') changeToUp();
        //if(output[0] == 'e') quitSnake = 1;
    }
}

int listenerStart = 0;

struct Snake
{   //every node of the snake 
    int x, y;  
    int now;   //0,1,2,3 means left right up down   
}Snake[15*35];  //Snake[0] is the head，and the other nodes are recorded in inverted order，eg: Snake[1] is the tail


//change the direction of circle
void changeToLeft()
{
    if(listenerStart == 1)
    {
        Snake[0].now = 0;
        listenerStart = 0;
    }
}

void changeToDown()
{
    if(listenerStart == 1)
    {
        Snake[0].now = 3;
        listenerStart = 0;
    }
}

void changeToRight()
{
    if(listenerStart == 1)
    {
        Snake[0].now = 1;
        listenerStart = 0;
    }
}
void changeToUp()
{
    if(listenerStart == 1)
    {
        Snake[0].now = 2;
        listenerStart = 0;
    }
}

const int mapH = 15;
const int mapW = 35;
char sHead = '@';
char sBody = 'O';
char sFood = '$';
char sNode = '.';    
char Map[15][35]; // the map of snake
int food[15][2] = {{5, 13},{6, 10}, {17, 15}, {8, 9}, {3, 4}, {1,12}, {0, 2}, {5, 23},
                   {15, 13},{16, 10}, {7, 15}, {8, 19}, {3, 14}, {11,12}, {10, 2}};
int foodNum = 0;
int eat = -1;
int win = 15;  // the length of win
 
int sLength = 1;
int overOrNot = 0;
int dx[4] = {0, 0, -1, 1};  
int dy[4] = {-1, 1, 0, 0}; 

void initGame(); 
void initFood();
void show();
void move();
void checkBorder();
void checkHead(int x, int y);
void action();
void showGameSuccess();
void showGameSuccess();
/**
 * enter the snake game
 */
void snakeGame()
{
    clear();
    initGame();  
    show(); 
}
/**
 * init game
 */
void initGame() 
{
    printf("===============================================================================\n");
    printf("                                        Snake                                  \n");
    printf("===============================================================================\n");
    printf("                                                                               \n");
    printf("                     press 'a''s''d''w' to control snake's direction.          \n");
    printf("                                                                               \n");
    printf("===============================================================================\n\n");

    int i, j;  
    int headx = 5;
    int heady = 10;
    memset(Map, '.', sizeof(Map));   //init map with '.'
    Map[headx][heady] = sHead;  
    Snake[0].x = headx;  
    Snake[0].y = heady;  
    Snake[0].now = -1;
    initFood();   //init target 
    for(i = 0; i < mapH; i++)
    {
        for(j = 0; j < mapW; j++) printf("%c", Map[i][j]);  
        printf("\n");  
    } 
   
    listenerStart = 1;
    while(listenerStart);
}

/**
 * the food location
 */
void initFood()
{
    int fx, fy;
    int tick;  
    while(1)
    {
        tick = get_ticks();
        fx = tick%mapH;
        fy = tick%mapW;     
        if(Map[fx][fy] == '.')
        {
            eat++;
            Map[fx][fy] = sFood;  
            break;  
        }
        foodNum++;
    }
}

/**
 * show game situation
 */
void show()
{
    int i, j; 
    printf("Load snake game ...");
    while(1)
    {
        listenerStart = 1;

        if(eat < 5)
        {
            milli_delay(8000);
        }
        else if(eat < 10)
        {
            milli_delay(6000);
        }
        else
        {
            milli_delay(5000);
        }
        move();  
        if(overOrNot || quitSnake) 
        {
            showGameOver();
            milli_delay(50000);
            clear();
            break;  
        } 
        if(eat == win)
        {
            showGameSuccess();
            milli_delay(50000);
            clear();
            break;
        }
        clear();
        for(i = 0; i < mapH; i++)   
        {   
            for(j = 0; j < mapW; j++)printf("%c", Map[i][j]);  
            printf("\n");  
        }  

        printf("           Have fun!\n");
        printf("       You have ate:%d\n",eat);
    }  
}
/**
 * snake move function
 */
void move()
{
    int i, x, y;  
    int t = sLength;
    x = Snake[0].x;  
    y = Snake[0].y;  
    Snake[0].x = Snake[0].x + dx[Snake[0].now]; 
    Snake[0].y = Snake[0].y + dy[Snake[0].now];  

    Map[x][y] = '.'; 
    checkBorder(); 
    checkHead(x, y);   
    if(sLength == t)  //did not eat
        for(i = 1; i < sLength; i++)
        {
            if(i == 1) Map[Snake[i].x][Snake[i].y] = '.'; //tail 
     
            if(i == sLength-1)  //the node after the head 
            {  
                Snake[i].x = x;  
                Snake[i].y = y;  
                Snake[i].now = Snake[0].now;  
            }  
            else 
            {  
                Snake[i].x = Snake[i+1].x;  
                Snake[i].y = Snake[i+1].y;  
                Snake[i].now = Snake[i+1].now;  
            }  
            Map[Snake[i].x][Snake[i].y] = sBody;  
        }  
}
/**
 *
 */
void checkBorder()
{
    if(Snake[0].x < 0 || Snake[0].x >= mapH || Snake[0].y < 0 || Snake[0].y >= mapW)
    {
        printl("game over!");
        overOrNot = 1;  
    }
    
}
/**
 *
 * @param x
 * @param y
 */
void checkHead(int x, int y)
{
    if(Map[Snake[0].x][Snake[0].y] == '.') Map[Snake[0].x][Snake[0].y] = sHead ;  
    else if(Map[Snake[0].x][Snake[0].y] == sFood) 
    {
        Map[Snake[0].x][Snake[0].y] = sHead ;    
        Snake[sLength].x = x;                //new node
        Snake[sLength].y = y;  
        Snake[sLength].now = Snake[0].now;  
        Map[Snake[sLength].x][Snake[sLength].y] = sBody;   
        sLength++;  
        initFood();  
    }  
    else
    { 
        overOrNot = 1; 
    }
}
/**
 *
 */
void showGameOver()
{
    printf("===============================================================================\n");
    printf("                                  Game Over                                    \n");
    printf("                          will exit in 3 seconds...                            \n");
    printf("===============================================================================\n");
}

/**
 *
 */
void showGameSuccess()
{
    printf("===============================================================================\n");
    printf("                               Congratulation!                                 \n");
    printf("                           will exit in 3 seconds...                           \n");
    printf("===============================================================================\n");
}


void calculator()
{
    printf("===============================================================================\n");
    printf("                                     Calculator                                \n");
    printf("===============================================================================\n");
    printf("                                                                               \n");
    printf("                                   Enter e to quit                             \n");
    printf("                                                                               \n");
    printf("===============================================================================\n\n");
    while(1)
    {   
        char result;

        char bufr[128];
        read(0, bufr, 128);


        switch(bufr[1])
        {

            case '+':result=(bufr[0] - '0')+(bufr[2] - '0');break;

            case '-':result=(bufr[0] - '0')-(bufr[2] - '0');break;

            case '*':result=(bufr[0] - '0')*(bufr[2] - '0');break;

            case '/':result=(bufr[0] - '0')/(bufr[2] - '0');break;
        
        }
        if(bufr[0]=='e') break;
        else printf("%d %c %d = %d\n",(bufr[0] - '0'),bufr[1],(bufr[2] - '0'),result);
    }
}



/*========================================================================*
                            Gobang game
 *=========================================================================*/

#define N  9
int chessboard[N + 1][N + 1] = { 0 };

//用来记录轮到玩家1还是玩家2
int whoseTurn = 0;

void initGame(void);
void printChessboard(void);
void playChess(void);
int GobangJudge(int, int);
int stop=0;

void Gobang()
{
    //初始化游戏
    printf("===============================================================================\n");
    printf("                                       Gobang                                  \n");
    printf("===============================================================================\n");
    printf("                                                                               \n");
    printf("                      Enter 'e' to quit, enter x and y to put chess.           \n");
    printf("                                                                               \n");
    printf("===============================================================================\n\n");
    //清空棋盘
    int i, j;
    for (i = 0; i <= N; i++)
        for (j = 0; j <= N; j++)
        {
            chessboard[i][j] = 0;
        }
    printChessboard();

    whoseTurn = 0;
    stop=0;
    chessboard[N + 1][N + 1] =0;
    printf("Player1:");
    while (1)
    {
        //玩家轮流下子
        whoseTurn++;
        playChess();
        if(stop==1)break;
    }
    stop=0;
    return 0;
}

void printChessboard(void)
{
    int i, j;

    for (i = 0; i <= N; i++)
    {
        for (j = 0; j <= N; j++)
        {
            if (0 == i) printf("%3d", j);
            else if (j == 0) printf("%3d", i);
            else if (1 == chessboard[i][j]) printf("  X");
            else if (2 == chessboard[i][j]) printf("  O");
            else printf("  -");
        }
        printf("\n");
    }
}

void playChess(void)
{
    int i, j;
    char bufr[128];
    read(0, bufr, 128);

    if (1 == whoseTurn % 2)
    {
        
        i = bufr[0] - '0';
        j = bufr[2] - '0';
        chessboard[i][j] = 1;
    }
    if (0 == whoseTurn % 2)
    {
        
        i = bufr[0] - '0';
        j = bufr[2] - '0';
        chessboard[i][j] = 2;
    }

    if(bufr[0]=='e') stop=1;
    else
    {
        printChessboard();
        if (1 == whoseTurn % 2) printf("Player2:");
        else printf("Player1:");
    }
    if (GobangJudge(i, j))
    {
        if (1 == whoseTurn % 2) printf("Player1 win!\n");
        else printf("Player2 win!\n");
        stop=1;
    }
    
}

int GobangJudge(int x, int y)
{
    int i, j;
    int t = 2 - whoseTurn % 2;

    for (i = x - 4, j = y; i <= x; i++)
    {
        if (i >= 1 && i <= N - 4 && t == chessboard[i][j] && t == chessboard[i + 1][j] && t == chessboard[i + 2][j] && t == chessboard[i + 3][j] && t == chessboard[i + 4][j])
            return 1;
    }
    for (i = x, j = y - 4; j <= y; j++)
    {
        if (j >= 1 && j <= N - 4 && t == chessboard[i][j] && t == chessboard[i][j + 1] && t == chessboard[i][j + 1] && t == chessboard[i][j + 3] && t == chessboard[i][j + 4])
            return 1;
    }
    for (i = x - 4, j = y - 4; i <= x, j <= y; i++, j++)
    {
        if (i >= 1 && i <= N - 4 && j >= 1 && j <= N - 4 && t == chessboard[i][j] && t == chessboard[i + 1][j + 1] && t == chessboard[i + 2][j + 2] && t == chessboard[i + 3][j + 3] && t == chessboard[i + 4][j + 4])
            return 1;
    }
    for (i = x + 4, j = y - 4; i >= 1, j <= y; i--, j++)
    {
        if (i >= 1 && i <= N - 4 && j >= 1 && j <= N - 4 && t == chessboard[i][j] && t == chessboard[i - 1][j + 1] && t == chessboard[i - 2][j + 2] && t == chessboard[i - 3][j + 3] && t == chessboard[i - 4][j + 4])
            return 1;
    }

    return 0;
}