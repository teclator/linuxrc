/*
 *
 * rootimage.c   Loading of rootimage
 *
 * Copyright (c) 1996-2002  Hubert Mantel, SuSE Linux AG  (mantel@suse.de)
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <ctype.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <endian.h>
#include <errno.h>

#include "global.h"
#include "text.h"
#include "util.h"
#include "dialog.h"
#include "window.h"
#include "display.h"
#include "rootimage.h"
#include "module.h"
#include "ftp.h"
#include "linuxrc.h"

#include "linux_fs.h"


#define BLOCKSIZE	10240
#define BLOCKSIZE_KB	(BLOCKSIZE >> 10)

typedef struct
    {
    char *dev_name;
    int   major;
    int   minor;
    } device_t;

static device_t root_devices_arm [] =
{
    { "hda",           3,   0 },
    { "hdb",           3,  64 },
    { "hdc",          22,   0 },
    { "hdd",          22,  64 },
    { "hde",          33,   0 },
    { "hdf",          33,  64 },
    { "hdg",          34,   0 },
    { "hdh",          34,  64 },
    { "hdi",          56,   0 },
    { "hdj",          56,  64 },
    { "hdk",          57,   0 },
    { "hdl",          57,  64 },
    { "sda",           8,   0 },
    { "sdb",           8,  16 },
    { "sdc",           8,  32 },
    { "sdd",           8,  48 },
    { "sde",           8,  64 },
    { "sdf",           8,  80 },
    { "sdg",           8,  96 },
    { "sdh",           8, 112 },
    { "sdi",           8, 128 },
    { "sdj",           8, 144 },
    { "sdk",           8, 160 },
    { "sdl",           8, 176 },
    { "sdm",           8, 192 },
    { "sdn",           8, 208 },
    { "sdo",           8, 224 },
    { "sdp",           8, 240 },
    { "rd/c0d0p",     48,   0 },
    { "rd/c0d1p",     48,   8 },
    { "rd/c0d2p",     48,  16 },
    { "rd/c0d3p",     48,  24 },
    { "rd/c0d4p",     48,  32 },
    { "rd/c0d5p",     48,  40 },
    { "rd/c0d6p",     48,  48 },
    { "rd/c0d7p",     48,  56 },
    { "rd/c1d0p",     49,   0 },
    { "rd/c1d1p",     49,   8 },
    { "rd/c1d2p",     49,  16 },
    { "rd/c1d3p",     49,  24 },
    { "rd/c1d4p",     49,  32 },
    { "rd/c1d5p",     49,  40 },
    { "rd/c1d6p",     49,  48 },
    { "rd/c1d7p",     49,  56 },
    { "ida/c0d0p",    72,   0 },
    { "ida/c0d1p",    72,  16 },
    { "ida/c0d2p",    72,  32 },
    { "ida/c0d3p",    72,  48 },
    { "ida/c0d4p",    72,  64 },
    { "ida/c0d5p",    72,  80 },
    { "ida/c0d6p",    72,  96 },
    { "ida/c0d7p",    72, 112 },
    { "ida/c1d0p",    73,   0 },
    { "ida/c1d1p",    73,  16 },
    { "ida/c1d2p",    73,  32 },
    { "ida/c1d3p",    73,  48 },
    { "ida/c1d4p",    73,  64 },
    { "ida/c1d5p",    73,  80 },
    { "ida/c1d6p",    73,  96 },
    { "ida/c1d7p",    73, 112 },
    { "cciss/c0d0p", 104,   0 },
    { "cciss/c0d1p", 104,  16 },
    { "cciss/c0d2p", 104,  32 },
    { "cciss/c0d3p", 104,  48 },
    { "cciss/c0d4p", 104,  64 },
    { "cciss/c0d5p", 104,  80 },
    { "cciss/c0d6p", 104,  96 },
    { "cciss/c0d7p", 104, 112 },
    { "cciss/c1d0p", 105,   0 },
    { "cciss/c1d1p", 105,  16 },
    { "cciss/c1d2p", 105,  32 },
    { "cciss/c1d3p", 105,  48 },
    { "cciss/c1d4p", 105,  64 },
    { "cciss/c1d5p", 105,  80 },
    { "cciss/c1d6p", 105,  96 },
    { "cciss/c1d7p", 105, 112 },
    { "ram",         1,   0 },
    { "md0",         9,   0 },
    { "md1",         9,   1 },
    { "md2",         9,   2 },
    { "md3",         9,   3 },
    { 0,             0,   0 }
};

static int       root_nr_blocks_im;
static window_t  root_status_win_rm;
static int       root_infile_im;
static int       root_outfile_im;

static struct {
  int rd;
} image = { rd: -1 };

#define fd_read		root_infile_im

static int ask_for_swap(int size);
static int  root_check_root      (char *root_string_tv);
static void root_update_status(int block);
static int fill_inbuf(void);
static void flush_window(void);
static void error(char *msg);
static int root_load_compressed(void);


int root_load_rootimage(char *infile_tv)
{
  char  buffer_ti [BLOCKSIZE];
  int   bytes_read_ii;
  int   rc_ii;
  int   current_block_ii;
  int   compressed_ii;
  int32_t filesize_li;
  int   error_ii = FALSE;

  image.rd = -1;

  fprintf(stderr, "Loading Image \"%s\"%s\n", infile_tv, config.win ? "" : "...");
  mod_free_modules();

  if(
    config.instmode == inst_floppy ||
    config.instmode == inst_ftp ||
    config.instmode == inst_http ||
    config.instmode == inst_tftp
  ) {
    if(config.instmode == inst_floppy) {
      root_nr_blocks_im = (4000L * 1024L) / BLOCKSIZE;
    }
    else {
      root_nr_blocks_im = (11151L * 1024L) / BLOCKSIZE;
    }
    compressed_ii = TRUE;
    sprintf(buffer_ti, "%s%s", txt_get(TXT_LOADING), config.win ? "" : "...");
  }
  else {
    rc_ii = util_fileinfo(infile_tv, &filesize_li, &compressed_ii);
    if(rc_ii) {
      if(!config.suppress_warnings) {
        dia_message(txt_get(TXT_RI_NOT_FOUND), MSGTYPE_ERROR);
      }
      return rc_ii;
    }

    root_nr_blocks_im = filesize_li / BLOCKSIZE;

    sprintf(buffer_ti, "%s (%d kB)%s",
      txt_get(TXT_LOADING),
      (root_nr_blocks_im * BLOCKSIZE) / 1024,
      config.win ? "" : "..."
    );
  }

  dia_status_on(&root_status_win_rm, buffer_ti);

  if(
    config.instmode == inst_ftp ||
    config.instmode == inst_http ||
    config.instmode == inst_tftp
  ) {
    root_infile_im = net_open(infile_tv);
    if(root_infile_im < 0) {
      util_print_net_error();
      error_ii = TRUE;
    }
  }
  else {
    root_infile_im = open(infile_tv, O_RDONLY);
    if(root_infile_im < 0) error_ii = TRUE;
  }

  root_outfile_im = open(RAMDISK_2, O_RDWR | O_CREAT | O_TRUNC, 0644);
  if(root_outfile_im < 0) error_ii = TRUE;

  if(error_ii) {
    net_close(root_infile_im);
    close(root_outfile_im);
    win_close(&root_status_win_rm);
    return -1;
  }

  if(compressed_ii) {
    root_load_compressed();
  }
  else {
    current_block_ii = 0;
    while((bytes_read_ii = net_read(root_infile_im, buffer_ti, BLOCKSIZE)) > 0) {
      rc_ii = write (root_outfile_im, buffer_ti, bytes_read_ii);
      if(rc_ii != bytes_read_ii) return -1;
      root_update_status(++current_block_ii);
    }
  }

  net_close(root_infile_im);
  close(root_outfile_im);

  win_close(&root_status_win_rm);
  root_set_root(RAMDISK_2);

  if(config.instmode == inst_floppy) {
    dia_message(txt_get(TXT_REMOVE_DISK), MSGTYPE_INFO);
  }

  return 0;
}


int ramdisk_open()
{
  int i;

  for(i = 0; i < sizeof config.ramdisk / sizeof *config.ramdisk; i++) {
    if(!config.ramdisk[i].inuse) {
      config.ramdisk[i].fd = open(config.ramdisk[i].dev, O_RDWR | O_CREAT | O_TRUNC, 0644);
      if(config.ramdisk[i].fd < 0) {
        perror(config.ramdisk[i].dev);
        return -1;
      }
      config.ramdisk[i].inuse = 1;

      return i;
    }
  }

  fprintf(stderr, "error: no free ramdisk!\n");

  return -1;
}


void ramdisk_close(int rd)
{
  if(rd < 0 || rd >= sizeof config.ramdisk / sizeof *config.ramdisk) return;

  if(!config.ramdisk[rd].inuse) return;

  if(config.ramdisk[rd].fd >= 0) {
    close(config.ramdisk[rd].fd);
    config.ramdisk[rd].fd = -1;
  }
}


void ramdisk_free(int rd)
{
  int i;

  if(rd < 0 || rd >= sizeof config.ramdisk / sizeof *config.ramdisk) return;

  if(!config.ramdisk[rd].inuse) return;

  ramdisk_close(rd);

  if(ramdisk_umount(rd)) return;

  i = util_free_ramdisk(config.ramdisk[rd].dev);

  if(!i) {
    config.ramdisk[rd].inuse = 0;
    config.ramdisk[rd].size = 0;
  }
}


int ramdisk_write(int rd, void *buf, int count)
{
  int i;

  if(
    rd < 0 ||
    rd >= sizeof config.ramdisk / sizeof *config.ramdisk ||
    !config.ramdisk[rd].inuse
  ) {
    fprintf(stderr, "oops: trying to write to invalid ramdisk %d\n", rd);
    return -1;
  }

  util_update_meminfo();

  if(ask_for_swap(count)) return -1;

  i = write(config.ramdisk[rd].fd, buf, count);

  if(i >= 0) config.ramdisk[rd].size += i;

  return i;
}


int ramdisk_umount(int rd)
{
  int i;

  if(rd < 0 || rd >= sizeof config.ramdisk / sizeof *config.ramdisk) return -1;

  if(!config.ramdisk[rd].inuse) return -1;

  if(!config.ramdisk[rd].mountpoint) return 0;

  if(!(i = umount(config.ramdisk[rd].mountpoint))) {
    str_copy(&config.ramdisk[rd].mountpoint, NULL);
  }
  else {
    fprintf(stderr, "umount: %s: %s\n", config.ramdisk[rd].dev, strerror(errno));
  }

  return i;
}


int ramdisk_mount(int rd, char *dir)
{
  int i;

  if(rd < 0 || rd >= sizeof config.ramdisk / sizeof *config.ramdisk) return -1;

  if(!config.ramdisk[rd].inuse) return -1;

  if(config.ramdisk[rd].mountpoint) return -1;

  if(!(i = util_mount_ro(config.ramdisk[rd].dev, dir))) {
    str_copy(&config.ramdisk[rd].mountpoint, dir);
  }
  else {
    fprintf(stderr, "mount: %s: %s\n", config.ramdisk[rd].dev, strerror(errno));
  }

  return i;
}


/*
 * Check if have still have enough free memory for 'size'.If not, ask user
 * for more swap.
 *
 * return: 0 ok, -1 error
 */
int ask_for_swap(int size)
{
  int i, win_old;
  char tmp[256];

  if(config.memory.current - (size >> 10) < config.memory.min_free) {
    if(!(win_old = config.win)) util_disp_init();
    strcpy(tmp, "There is not enough memory to load all data.\n\nTo continue, activate some swap space.");
    i = dia_contabort(tmp, NO);
    if(!win_old) util_disp_done();
    if(i != YES) return -1;
  }

  return 0;
}


/*
 * returns ramdisk index if successful
 */
int load_image(char *file_name, instmode_t mode)
{
  char buffer[BLOCKSIZE], cramfs_name[17];
  int bytes_read, current_pos;
  int i, rc, compressed;
  int err = 0, got_size = 0;
  char *real_name = NULL;
  char *buf2;
  struct cramfs_super_block *cramfssb;

  fprintf(stderr, "Loading image \"%s\"%s\n", file_name, config.win ? "" : "...");

  /* check if have to actually _load_ the image to get info about it */
  if(mode == inst_ftp || mode == inst_http || mode == inst_tftp) {
    mode = inst_net;
  }

  /* assume 10MB */
  root_nr_blocks_im = 10240 / BLOCKSIZE_KB;

  image.rd = -1;

  if(mode != inst_floppy && mode != inst_net) {
    rc = util_fileinfo(file_name, &i, &compressed);
    if(rc) {
      if(!config.suppress_warnings) {
        dia_message(txt_get(TXT_RI_NOT_FOUND), MSGTYPE_ERROR);
      }
      return -1;
    }

    got_size = 1;
    root_nr_blocks_im = i / BLOCKSIZE;
  }

  if(mode == inst_net) {
    fd_read = net_open(file_name);
    if(fd_read < 0) {
      util_print_net_error();
      err = 1;
    }
  }
  else {
    fd_read = open(file_name, O_RDONLY);
    if(fd_read < 0) err = 1;
  }

  if((image.rd = ramdisk_open()) < 0) {
    dia_message("No usable ramdisk found.", MSGTYPE_ERROR);
    err = 1;
  }

  if(err) {
    net_close(fd_read);
    ramdisk_close(image.rd);
    return -1;
  }

  if(!got_size) {
    /* read maybe more to check super block? */
    buf2 = malloc(config.cache.size = 256);
    config.cache.cnt = 0;

    for(i = 0; i < config.cache.size; i += bytes_read) {
      bytes_read = net_read(fd_read, buf2 + i, config.cache.size - i);
      // fprintf(stderr, "got %d bytes\n", bytes_read);
      if(bytes_read <= 0) break;
    }

    config.cache.buf = buf2;

    if(i) config.cache.size = i;

#if 0
    fprintf(stderr, "cache: %d\n", config.cache.size);
    for(i = 0; i < 32; i++) {
      fprintf(stderr, " %02x", (unsigned char) buf2[i]);
    }
    fprintf(stderr, "\n");
#endif

    if(config.cache.size > 64) {
      if(buf2[0] == 0x1f && (unsigned char) buf2[1] == 0x8b) {
        /* gzip'ed image */
        compressed = 1;

        // fprintf(stderr, "compressed\n");

        if((buf2[3] & 0x08)) {
          real_name = buf2 + 10;
          for(i = 0; i < config.cache.size - 10 && i < 128; i++) {
            if(!real_name[i]) break;
          }
          if(i > 128) real_name = NULL;
        }
      }
      else {
        cramfssb = (struct cramfs_super_block *) config.cache.buf;
        if(cramfsmagic((*cramfssb)) == CRAMFS_SUPER_MAGIC) {
          /* cramfs */
          memcpy(cramfs_name, config.cache.buf + 0x30, sizeof cramfs_name - 1);
          cramfs_name[sizeof cramfs_name - 1] = 0;
          real_name = cramfs_name;
          fprintf(stderr, "cramfs: \"%s\"\n", real_name);
        }
      }
    }

    if(real_name) {
      // fprintf(stderr, "file name: \"%s\"\n", real_name);

      i = 0;
      if(sscanf(real_name, "%*s %d", &i) >= 1) {
        if(i > 0) {
          root_nr_blocks_im = i / BLOCKSIZE_KB;
          got_size = 1;
          fprintf(stderr, "image size: %d kB\n", i);
        }
      }
    }

    if(!compressed && !got_size) {
      /* check fs superblock */
    }

  }

  if(got_size) {
    if(ask_for_swap(root_nr_blocks_im * BLOCKSIZE)) {
      net_close(fd_read);
      ramdisk_free(image.rd);
      return image.rd = -1;
    }

    sprintf(buffer, "%s (%d kB)%s",
      txt_get(TXT_LOADING),
      root_nr_blocks_im * BLOCKSIZE_KB,
      config.win ? "" : "..."
    );
  }
  else {
    sprintf(buffer, "%s%s", txt_get(TXT_LOADING), config.win ? "" : "...");
  }

  dia_status_on(&root_status_win_rm, buffer);

  if(compressed) {
    err = root_load_compressed();
  }
  else {
    current_pos = 0;
    while((bytes_read = net_read(fd_read, buffer, BLOCKSIZE)) > 0) {
      rc = ramdisk_write(image.rd, buffer, bytes_read);
      if(rc != bytes_read) {
        err = 1;
        break;
      }
      root_update_status((current_pos += bytes_read) / BLOCKSIZE);
    }
  }

  net_close(fd_read);
  ramdisk_close(image.rd);

  win_close(&root_status_win_rm);

  if(err) {
    fprintf(stderr, "error loading ramdisk\n");
    ramdisk_free(image.rd);
    image.rd = -1;
  }
  else if(config.instmode == inst_floppy) {
    dia_message(txt_get(TXT_REMOVE_DISK), MSGTYPE_INFO);
  }

  return image.rd;
}


int root_check_root(char *root_string_tv)
{
  char buf[256];
  int rc;

  if(strstr(root_string_tv, "/dev/") == root_string_tv) {
    root_string_tv += sizeof "/dev/" - 1;
  }

  sprintf(buf, "/dev/%s", root_string_tv);

  if(util_mount_ro(buf, mountpoint_tg)) return -1;

  sprintf(buf, "%s/etc/passwd", mountpoint_tg);
  rc = util_check_exist(buf);

  umount(mountpoint_tg);

  return rc == TRUE ? 0 : -1;
}


void root_set_root (char *root_string_tv)
    {
    FILE  *proc_root_pri;
    int    root_ii;
    char  *tmp_string_pci;
    int    found_ii = FALSE;
    int    i_ii = 0;


    lxrc_new_root = strdup (root_string_tv);
    if (!strncmp ("/dev/", root_string_tv, 5))
        tmp_string_pci = root_string_tv + 5;
    else
        tmp_string_pci = root_string_tv;

    while (!found_ii && root_devices_arm [i_ii].dev_name)
        if (!strncmp (tmp_string_pci, root_devices_arm [i_ii].dev_name,
                      strlen (root_devices_arm [i_ii].dev_name)))
            found_ii = TRUE;
        else
            i_ii++;

    if (!found_ii)
        return;

    root_ii = root_devices_arm [i_ii].major * 256 +
              root_devices_arm [i_ii].minor +
              atoi (tmp_string_pci + strlen (root_devices_arm [i_ii].dev_name));

    root_ii *= 65537;

    proc_root_pri = fopen ("/proc/sys/kernel/real-root-dev", "w");
    if (!proc_root_pri)
        return;

    fprintf (proc_root_pri, "%d\n", root_ii);
    fclose (proc_root_pri);
    }


int root_boot_system()
{
  int  rc, mtype;
  char *root = NULL;
  char *module, *type;
  char buf[256];

  str_copy(&root, "/dev/");

  do {
    if((rc = dia_input2(txt_get(TXT_ENTER_ROOT_FS), &root, 25, 0))) {
      str_copy(&root, NULL);
      return rc;
    }

    if((type = util_fstype(root, &module))) {
      if(module && config.module.dir) {
        sprintf(buf, "%s/%s.o", config.module.dir, module);
        if(!util_check_exist(buf)) {
          mtype = mod_get_type("file system");

          sprintf(buf, txt_get(TXT_FILE_SYSTEM), type);
          strcat(buf, "\n\n");
          mod_disk_text(buf + strlen(buf), mtype);

          rc = dia_okcancel(buf, YES) == YES ? 1 : 0;

          if(rc) mod_add_disk(0, mtype);
        }

        mod_load_modules(module, 0);
      }
    }

    if((rc = root_check_root(root))) {
      dia_message(txt_get(TXT_INVALID_ROOT_FS), MSGTYPE_ERROR);
    }
  }
  while(rc);

  root_set_root(root);

  free(root);

  return 0;
}


void root_update_status(int block)
{
  static int old_percent_is;
  int percent;

  percent = (block * 100) / root_nr_blocks_im;
  if(percent != old_percent_is) {
    dia_status(&root_status_win_rm, old_percent_is = percent);
  }
}


/* --------------------------------- GZIP -------------------------------- */

#define OF(args)  args

#define memzero(s, n)     memset ((s), 0, (n))

typedef unsigned char  uch;
typedef unsigned short ush;
typedef unsigned long  ulg;

#define INBUFSIZ 4096
#define WSIZE 0x8000    /* window size--must be a power of two, and */
                        /*  at least 32K for zip's deflate method */

static uch *inbuf;
static uch *window;

static unsigned insize = 0;  /* valid bytes in inbuf */
static unsigned inptr = 0;   /* index of next byte to be processed in inbuf */
static unsigned outcnt = 0;  /* bytes in output buffer */
static int exit_code = 0;
static long bytes_out = 0;

#define get_byte()  (inptr < insize ? inbuf[inptr++] : fill_inbuf())
                
/* Diagnostic functions (stubbed out) */
#define Assert(cond,msg)
#define Trace(x)
#define Tracev(x)
#define Tracevv(x)
#define Tracec(c,x)
#define Tracecv(c,x)

#define STATIC static

static int  fill_inbuf(void);
static void flush_window(void);
static void error(char *m);
static void gzip_mark(void **);
static void gzip_release(void **);


#include "inflate.c"


static void gzip_mark(void **ptr)
{
}


static void gzip_release(void **ptr)
{
}


/* ===========================================================================
 * Fill the input buffer. This is called only when the buffer is empty
 * and at least one byte is really needed.
 */
int fill_inbuf()
{
  fd_set emptySet, readSet;
  struct timeval timeout;
  int rc;

  if(
    config.instmode == inst_ftp ||
    config.instmode == inst_http
    /* !!! NOT '|| config.instmode == inst_tftp' !!! */
  ) {
    FD_ZERO(&emptySet);
    FD_ZERO(&readSet);
    FD_SET(root_infile_im, &readSet);

    timeout.tv_sec = TIMEOUT_SECS;
    timeout.tv_usec = 0;

    rc = select(root_infile_im + 1, &readSet, &emptySet, &emptySet, &timeout);

    if(rc <= 0) {
      util_print_net_error();
      exit_code = 1;
      insize = INBUFSIZ;
      inptr = 1;
      return -1;
    }
  }

  insize = net_read(root_infile_im, inbuf, INBUFSIZ);

  if(insize <= 0) {
    exit_code = 1;
    return -1;
  }

  inptr = 1;

  return inbuf[0];
}


/* ===========================================================================
 * Write the output window window[0..outcnt-1] and update crc and bytes_out.
 * (Used for the decompressed data only.)
 */
void flush_window()
{
  ulg c = crc;		/* temporary variable */
  unsigned n;
  uch *in, ch;
  int i;

  if(exit_code) {
    fprintf(stderr, ".");
    fflush(stderr);
    bytes_out += (ulg) outcnt;
    outcnt = 0;
    return;
  }
    
  if(image.rd >= 0) {
    i = ramdisk_write(image.rd, window, outcnt);
    if(i < 0) exit_code = 1;
  }
  else {
    write(root_outfile_im, window, outcnt);
  }

  in = window;
  for(n = 0; n < outcnt; n++) {
    ch = *in++;
    c = crc_32_tab[((int)c ^ ch) & 0xff] ^ (c >> 8);
  }
  crc = c;

  bytes_out += (ulg) outcnt;
  root_update_status(bytes_out / BLOCKSIZE);
  outcnt = 0;
}


void error(char *msg)
{
  fprintf(stderr, "%s\n", msg);
  exit_code = 1;
}


int root_load_compressed()
{
  int err;

  inbuf = malloc(INBUFSIZ);
  window = malloc(WSIZE);
  insize = 0;
  inptr = 0;
  outcnt = 0;
  exit_code = 0;
  bytes_out = 0;
  crc = 0xffffffffL;

  makecrc();
  err = gunzip();

  free(inbuf);
  free(window);

  return err;
}

