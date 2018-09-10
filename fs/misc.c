/*************************************************************************//**
 *****************************************************************************
 * @file   misc.c
 * @brief
 * @author Forrest Y. Yu
 * @date   2008
 *****************************************************************************
 *****************************************************************************/

/* Orange'S FS */

#include "type.h"
#include "stdio.h"
#include "const.h"
#include "protect.h"
#include "string.h"
#include "fs.h"
#include "proc.h"
#include "tty.h"
#include "console.h"
#include "global.h"
#include "keyboard.h"
#include "proto.h"
#include "hd.h"
#include "fs.h"

PUBLIC struct dir_entry * find_entry(char *path)
{
    int i, j;

    char filename[MAX_PATH];
    memset(filename, 0, MAX_FILENAME_LEN);
    struct inode * dir_inode;
    if (strip_path(filename, path, &dir_inode) != 0)
        return 0;

    if (filename[0] == 0)   /* path: "/" */
        return dir_inode;

    /**
     * Search the dir for the file.
     */
    int dir_blk0_nr = dir_inode->i_start_sect;
    int nr_dir_blks = (dir_inode->i_size + SECTOR_SIZE - 1) / SECTOR_SIZE;
    int nr_dir_entries =
      dir_inode->i_size / DIR_ENTRY_SIZE; /**
                           * including unused slots
                           * (the file has been deleted
                           * but the slot is still there)
                           */
    int m = 0;
    struct dir_entry * pde;
    for (i = 0; i < nr_dir_blks; i++) {
        RD_SECT(dir_inode->i_dev, dir_blk0_nr + i);
        pde = (struct dir_entry *)fsbuf;
        for (j = 0; j < SECTOR_SIZE / DIR_ENTRY_SIZE; j++,pde++) {
            if (memcmp(filename, pde->name, MAX_FILENAME_LEN) == 0)
                return pde;
            if (++m > nr_dir_entries)
                break;
        }
        if (m > nr_dir_entries) /* all entries have been iterated */
            break;
    }

    /* file not found */
    return 0;
}

/*****************************************************************************
 *                                search_file
 *****************************************************************************/
/**
 * Search the file and return the inode_nr.
 *
 * @param[in] path The full path of the file to search.
 * @return         Ptr to the i-node of the file if successful, otherwise zero.
 *
 * @see open()
 * @see do_open()
 *****************************************************************************/
PUBLIC int search_file(char * path)
{
    int i, j;

    char filename[MAX_PATH];
    memset(filename, 0, MAX_FILENAME_LEN);
    struct inode * dir_inode;
    if (strip_path(filename, path, &dir_inode) != 0)
        return 0;

    if (filename[0] == 0)   /* path: "/" */
        return dir_inode->i_num;

    /**
     * Search the dir for the file.
     */
    int dir_blk0_nr = dir_inode->i_start_sect;
    int nr_dir_blks = (dir_inode->i_size + SECTOR_SIZE - 1) / SECTOR_SIZE;
    int nr_dir_entries =
      dir_inode->i_size / DIR_ENTRY_SIZE; /**
                           * including unused slots
                           * (the file has been deleted
                           * but the slot is still there)
                           */
    int m = 0;
    struct dir_entry * pde;
    for (i = 0; i < nr_dir_blks; i++) {
        RD_SECT(dir_inode->i_dev, dir_blk0_nr + i);
        pde = (struct dir_entry *)fsbuf;
        for (j = 0; j < SECTOR_SIZE / DIR_ENTRY_SIZE; j++,pde++) {
            if (memcmp(filename, pde->name, MAX_FILENAME_LEN) == 0)
                return pde->inode_nr;
            if (++m > nr_dir_entries)
                break;
        }
        if (m > nr_dir_entries) /* all entries have been iterated */
            break;
    }

    /* file not found */
    return 0;
}

/*****************************************************************************
 *                                strip_path
 *****************************************************************************/
/**
 * Get the basename from the fullpath.
 *
 * In Orange'S FS v1.0, all files are stored in the root directory.
 * There is no sub-folder thing.
 *
 * This routine should be called at the very beginning of file operations
 * such as open(), read() and write(). It accepts the full path and returns
 * two things: the basename and a ptr of the root dir's i-node.
 *
 * e.g. After stip_path(filename, "/blah", ppinode) finishes, we get:
 *      - filename: "blah"
 *      - *ppinode: root_inode
 *      - ret val:  0 (successful)
 *
 * Currently an acceptable pathname should begin with at most one `/'
 * preceding a filename.
 *
 * Filenames may contain any character except '/' and '\\0'.
 *
 * @param[out] filename The string for the result.
 * @param[in]  pathname The full pathname.
 * @param[out] ppinode  The ptr of the dir's inode will be stored here.
 *
 * @return Zero if success, otherwise the pathname is not valid.
 *****************************************************************************/
PUBLIC int strip_path(char * filename, const char * pathname,
              struct inode** ppinode)
{
    const char * s = pathname;
    char * t = filename;
    if (s == 0)
        return -1;

    if (*s == '/')
        s++;
    struct inode *pinode_now = root_inode, *ptemp;
    struct dir_entry * pde;
    int dir_blk0_nr, nr_dir_blks, nr_dir_entries, m;
    int i, j;
    while(*s){
        if(*s == '/'){
            /*if(*(++s)==0)
                break;*/
            int flag = 0;
            dir_blk0_nr = pinode_now->i_start_sect;
            nr_dir_blks = (pinode_now->i_size + SECTOR_SIZE - 1) / SECTOR_SIZE;
            nr_dir_entries =
            pinode_now->i_size / DIR_ENTRY_SIZE; 
            m = 0;
            pde = 0;
            *t = 0;
            //printl("filename-strip: %s\n",filename);
            for (i = 0; i < nr_dir_blks && flag==0; i++) {
                RD_SECT(pinode_now->i_dev, dir_blk0_nr + i);
                pde = (struct dir_entry *)fsbuf;
                //printl("%d  %d\n",SECTOR_SIZE , DIR_ENTRY_SIZE );
                for (j = 0; j < SECTOR_SIZE / DIR_ENTRY_SIZE; j++,pde++) {
                    if (strcmp(filename, pde->name) == 0){
                          ptemp = get_inode(pinode_now->i_dev, pde->inode_nr);
                         if(ptemp->i_mode == I_DIRECTORY){
                                  pinode_now = ptemp;
                                  flag = 1;
                                   break;
                         }
                        //return pde->inode_nr;
                    }
                    if (++m > nr_dir_entries)
                        return -1;
                }
                if (m > nr_dir_entries || flag==0) {
                    // printl("flag==0\n");
                    return -1;
                }
            }
            if(flag == 0){
                return -1;
            }
            t = filename;
            s++;
        }
        else{
            *t++ = *s++;
            if (t - filename >= MAX_FILENAME_LEN)
                break;
        }
    }
    *t = 0;
    *ppinode = pinode_now;
  //  printl("size:%d\n",pinode_now->i_size);
    return 0;
}
PUBLIC int isNull(char * filename, char * pathname){
    int i, j;
    struct inode * dir_inode;
    const char *pathtmp = pathname;
    int l=0;
    char path[30];
    while(*pathtmp){
        path[l++] = *pathtmp++;
    }
    path[l++] = '/';
    path[l] = 0;
    strip_path(filename, path, &dir_inode);
    int dir_blk0_nr = dir_inode->i_start_sect;
    int nr_dir_blks = (dir_inode->i_size + SECTOR_SIZE - 1) / SECTOR_SIZE;
    int nr_dir_entries = dir_inode->i_size / DIR_ENTRY_SIZE;
    int m = 0;

    struct dir_entry * pde;
    for (i = 0; i < nr_dir_blks; i++){
        RD_SECT(dir_inode->i_dev, dir_blk0_nr + i);
        pde = (struct dir_entry *)fsbuf;
        for (j = 0; j < SECTOR_SIZE / DIR_ENTRY_SIZE; j++, pde++){
                /*struct inode *n = find_inode(pde->inode_nr);*/
                if(strlen(pde->name)>0){
                     return 1;
                }
        }
    }
    return 0;
}

PUBLIC int find_all_path(char* filename, char * pathname, int *len)
{
    int i, j;
    struct inode * dir_inode;
    const char *tmp = pathname;
    const char *pathtmp = pathname;
    char fn[12];
    int l=0;
    char path[30];
    while(*pathtmp){
        path[l++] = *pathtmp++;
    }
    path[l++] = '/';
    path[l] = 0;

    l = 0;
    while(*tmp){
        if(*tmp != '/'){
            fn[l++] = *tmp++;
        }else{
            tmp++;
            l = 0;
        }
    }
    fn[l] = '/';

    int k = *len + 1;
    filename[k] = pathname;

    strip_path(fn, path, &dir_inode);
    int dir_blk0_nr = dir_inode->i_start_sect;
    int nr_dir_blks = (dir_inode->i_size + SECTOR_SIZE - 1) / SECTOR_SIZE;
    int nr_dir_entries = dir_inode->i_size / DIR_ENTRY_SIZE;
    int m = 0;
    struct dir_entry * pde;
    for (i = 0; i < nr_dir_blks; i++){
        RD_SECT(dir_inode->i_dev, dir_blk0_nr + i);
        pde = (struct dir_entry *)fsbuf;
        
        int length = 0;
        for (j = 0; j < SECTOR_SIZE / DIR_ENTRY_SIZE; j++, pde++){
                /*struct inode *n = find_inode(pde->inode_nr);*/
            if (strlen(pde->name)) {
                char dir_name[30] = "";
                char* t = pathname;
                int _len = 0;
                while (*t) {
                    dir_name[_len++] = *(t++);
                }
                dir_name[_len++] = '/';
                dir_name[_len] = 0;
                strcat(dir_name, pde->name);

                strcat(filename, dir_name);
                char space_str[2] = " ";
                strcat(filename, space_str);
                printl("filename = \"%s\"\n", filename);
                printl("filename = %d\n", filename);

                length += strlen(dir_name) + 1;
            }   
        }
        filename[length] = 0;
    }
    return 0;
}
